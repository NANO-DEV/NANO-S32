#include "types.h"
#include "x86.h"
#include "hwio.h"
#include "pci.h"
#include "ulib/ulib.h"
#include "net.h"

/* Network controller
 * Assumes NE2000 compatible nic
 * Example: Realtek RTL8019AS
 * http://www.ethernut.de/pdf/8019asds.pdf
 */
/*
 * The Ne2000 network card uses two ring buffers for packet handling.
 * These are circular buffers made of 256-byte pages that the chip's DMA logic
 * will use to store received packets or to get received packets.
 * Note that a packet will always start on a page boundary,
 * thus there may be unused bytes at the end of a page.
 *
 * Two registers NE2K_PSTART and NE2K_PSTOP define a set of 256-byte pages in the buffer
 * memory that will be used for the ring buffer. As soon as the DMA attempts to
 * read/write to NE2K_PSTOP, it will be sent back to NE2K_PSTART
 *
 * NE2K_PSTART                                                                       NE2K_PSTOP
 * ####+-8------+-9------+-a------+-b------+-c------+-d------+-e------+-f------+####
 * ####| Packet 3 (cont) |########|########|Packet1#|   Packet  2#####|Packet 3|####
 * ####+--------+--------+--------+--------+--------+--------+--------+--------+####
 * (An 8-page ring buffer with 3 packets and 2 free slots)
 * While receiving, the NIC has 2 additional registers that point to the first
 * packet that's still to be read and to the start of the currently written
 * packet (named boundary pointer and current page respectively).
 *
 * Programming registers of the NE2000 are collected in pages.
 * Page 0 contains most of the control and status registers while
 * page 1 contains physical (NE2K_PAR0..NE2K_PAR5) and multicast addresses (NE2K_MAR0..NE2K_MAR7)
 * to be checked by the card
 */

// Byte swap operation
#define BSWAP_16(value) \
((((value) & 0xFF) << 8) | (((value)&0xFF00) >> 8))

#define NUM_COMPATIBLE_DEVICES 1
struct device_id_t {
  uint16_t  vendor_id;
  uint16_t  device_id;
} const ne2k_compatible[NUM_COMPATIBLE_DEVICES] = {
  {0x10EC, 0x8029} // Realtek
};

// Registers
#define NE2K_CR       0x00 // Command register
// 7-6:PS1-PS0 5-3:RD2-0 2:TXP 1:STA 0:STP

// Page 0 registers, read
#define NE2K_CLDA0    0x01 // Current Local DMA Address
#define NE2K_CLDA1    0x02
#define NE2K_BNRY     0x03 // Boundary
#define NE2K_TSR      0x04 // Transmit status
#define NE2K_NCR      0x05 // Collision counter
#define NE2K_FIFO     0x06 // Allows to examine the contents of the FIFO after loopback
#define NE2K_ISR      0x07 // Interrupt status
#define NE2K_CRDA0    0x08 // Current Remote DMA Address
#define NE2K_CRDA1    0x09
#define NE2K_RSR      0x0C // Receive status
#define NE2K_CNTR0    0x0D // Error counters
#define NE2K_CNTR1    0x0E
#define NE2K_CNTR2    0x0F

// Page 0 registers, write
#define NE2K_PSTART   0x01 // Page start (read page 2)
#define NE2K_PSTOP    0x02 // Page stop (read page 2)
#define NE2K_TPSR     0x04 // Transmit page start (read page 2)
#define NE2K_TBCR0    0x05 // Transmit byte count
#define NE2K_TBCR1    0x06
#define NE2K_RSAR0    0x08 // Remote start address
#define NE2K_RSAR1    0x09
#define NE2K_RBCR0    0x0A // Remote byte count
#define NE2K_RBCR1    0x0B
#define NE2K_RCR      0x0C // Receive config (read page 2)
#define NE2K_TCR      0x0D // Transmit config (read page 2)
#define NE2K_DCR      0x0E // Data config (read page 2)
#define NE2K_IMR      0x0F // Interrupt mask (read page 2)

// Page 1 registers, read/write
#define NE2K_PAR0    0x01 // Physical address
#define NE2K_PAR1    0x02
#define NE2K_PAR2    0x03
#define NE2K_PAR3    0x04
#define NE2K_PAR4    0x05
#define NE2K_PAR5    0x06
#define NE2K_CURR    0x07 // Current page
#define NE2K_MAR0    0x08 // Multicast address
#define NE2K_MAR1    0x09
#define NE2K_MAR2    0x0A
#define NE2K_MAR3    0x0B
#define NE2K_MAR4    0x0C
#define NE2K_MAR5    0x0D
#define NE2K_MAR6    0x0E
#define NE2K_MAR7    0x0F

#define NE2K_DATA    0x10 // Data i/o
#define NE2K_RESET   0x1F // Reset register

// NE2K_ISR/NE2K_IMR flags
#define NE2K_STAT_RX  0x01 // Packet received
#define NE2K_STAT_TX  0x02 // Packet sent
#define NE2K_STAT_RXE 0x04 // Receive Error
#define NE2K_STAT_TXE 0x08 // Transmission Error
#define NE2K_STAT_OVW 0x10 // Overwrite
#define NE2K_STAT_CNT 0x20 // Counter Overflow
#define NE2K_STAT_RDC 0x40 // Remote Data Complete
#define NE2K_STAT_RST 0x80 // Reset status

// Interrupt Mask Register (IMR)
#define NE2K_IMR_PRXE 0x01  // Packet Received Interrupt Enable
#define NE2K_IMR_PTXE 0x02  // Packet Transmit Interrupt Enable
#define NE2K_IMR_RXEE 0x04  // Receive Error Interrupt Enable
#define NE2K_IMR_TXEE 0x08  // Transmit Error Interrupt Enable
#define NE2K_IMR_OVWE 0x10  // Overwrite Error Interrupt Enable
#define NE2K_IMR_CNTE 0x20  // Counter Overflow Interrupt Enable
#define NE2K_IMR_RDCE 0x40  // Remote DMA Complete Interrupt Enable

// Store here current reception page
static uint rx_next = 0x47;

// Is network enabled
static uint network_state = NET_STATE_UNINITIALIZED;

// Hardware info
#define MAC_LEN 6 // Bytes size of a MAC address
static uint base = 0xC000; // Base device port
static uint8_t local_mac[MAC_LEN] = {0}; // Get from network card

// IP protocol network params
uint8_t local_ip[IP_LEN] = {192,168,0,40}; // Default value
uint8_t local_gate[IP_LEN] = {192,168,0,1}; // Default value
static uint8_t local_net[IP_LEN] = {255,255,255,0}; // Default value

// Default send/recv port
#define UDP_SEND_PORT 8086

// Enable reception only in rcv_port
static uint16_t rcv_port = UDP_SEND_PORT;

// Buffer to send/receive packets
typedef struct {
  net_address_t addr;
  size_t      size;
  uint8_t     buff[256];
} netpacket_t;

static netpacket_t rcv_buff;
static uint8_t   snd_buff[256] = {0};
static uint8_t   tmp_buff[256] = {0};

// Ethernet related
typedef struct {
  uint8_t  dst[MAC_LEN];
  uint8_t  src[MAC_LEN];
  uint16_t type;
  uint8_t  data[0];  // size 46-1500
} eth_hdr_t;

// Values of eth_hdr_t->eh_type
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IP   0x0800

#define ETH_HDR_LEN   14

#define ETH_MTU       1500
#define ETH_VLAN_LEN  4
#define ETH_CRC_LEN   4

#define ETH_PKT_MAX_LEN  (ETH_HDR_LEN+ETH_VLAN_LEN+ETH_MTU)

// ARP related
typedef struct {
  uint16_t hrd;   // format of hardware address
  uint16_t pro;   // format of protocol address
  uint8_t  hln;   // length of hardware address
  uint8_t  pln;   // length of protocol address
  uint16_t op;    // arp/rarp operation
  uint8_t  sha[MAC_LEN];
  uint8_t  spa[IP_LEN];
  uint8_t  dha[MAC_LEN];
  uint8_t  dpa[IP_LEN];
} arp_hdr_t;

// Values of arp_hdr_t->ah_hrd
#define ARP_HTYPE_ETHER 1  // Ethernet hardware type

// Values of arp_hdr_t->ah_pro
#define ARP_PTYPE_IP    0x0800 // IP protocol type
#define ARP_PTYPE_ARP   0x0806 // ARP protocol type

// Values of arp_hdr_t->ah_op
#define ARP_OP_REQUEST  1  // Request op code
#define ARP_OP_REPLY    2  // Reply op code


// IP Protocol
#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_TCP  6
#define IP_PROTOCOL_UDP  17

// IPv4 Header
typedef struct {
  uint8_t  verIhl;
  uint8_t  tos;
  uint16_t len;
  uint16_t id;
  uint16_t offset;
  uint8_t  ttl;
  uint8_t  protocol;
  uint16_t checksum;
  uint8_t  src[IP_LEN];
  uint8_t  dst[IP_LEN];
} ip_hdr_t;

// UDP header
typedef struct {
  uint16_t srcPort;
  uint16_t dstPort;
  uint16_t len;
  uint16_t checksum;
} udp_hdr_t;

// ARP table to hold IP-MAC entries
#define ARP_TABLE_LEN 8
struct ARP_TABLE {
  uint8_t ip[IP_LEN];
  uint8_t mac[MAC_LEN];
} arp_table[ARP_TABLE_LEN];

// Given an IP address, provide effective IP address to send packet
static uint8_t* get_effective_ip(uint8_t* ip)
{
  uint i = 0;

  // If IP is outside the local network return gate address
  for(i=0; i<IP_LEN; i++) {
    if((ip[i]&local_net[i]) != (local_ip[i]&local_net[i])) {
      break;
    }
  }
  if(i!=IP_LEN || memcmp(ip, local_ip, sizeof(local_ip))==0) {
    return local_gate;
  }
  // Otherwise, return the same IP
  return ip;
}

// Given an IP address, provide MAC address
// if found in the translation table
static uint8_t* find_mac_in_table(uint8_t* ip)
{
  // Local IP -> local MAC
  if(memcmp(ip, local_ip, sizeof(local_ip)) == 0) {
    return local_mac;
  }

  // Otherwise search in the table
  for(uint i=0; i<ARP_TABLE_LEN; i++) {
    if(memcmp(arp_table[i].ip, ip,
      sizeof(arp_table[i].ip)) == 0) {
      return arp_table[i].mac;
    }
  }
  return 0;
}

// Keep checksum 16-bits
static uint16_t net_checksum_final(uint32_t sum)
{
  sum = (sum&0xFFFF) + (sum>>16);

  uint16_t temp = ~sum;
  return ((temp&0x00FF)<<8) | ((temp&0xFF00)>>8);
}

// Checksum accumulation function
static uint32_t net_checksum_acc(uint8_t* data, uint32_t len)
{
  uint32_t sum = 0;
  uint16_t* p = (uint16_t*)data;

  while(len > 1) {
    sum += *p++;
    len -= 2;
  }

  if(len) {
    sum += *(uint8_t*)p;
  }

  return sum;
}

// Main checksum function
static uint16_t net_checksum(uint8_t* data, uint len)
{
  uint32_t sum = net_checksum_acc(data, len);
  return net_checksum_final(sum);
}

// This helps calculating CRC
static uint32_t poly8_lookup[256] =
{
 0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
 0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// Calculate a checksum on a buffer
static uint32_t crc32_byte(uint8_t* p, uint32_t bytelength)
{
  uint32_t crc = 0xFFFFFFFFL;
  while(bytelength-- !=0) {
    crc = (poly8_lookup[((uint8_t)(crc&0xFF)^(*(p++)))] ^ (crc>>8));
  }
  return (~crc);
}

// Select a registers page in the ne2k
static void ne2k_page_select(uint page)
{
  uint pg = (page&0x01)<<6;
  uint cm = 0x3F & inb(base + NE2K_CR);
  outb(base + NE2K_CR, (pg|cm));
}

// Send network packet (hardware, ne2k)
static uint ne2k_send(uint8_t* data, size_t len)
{
  while(inb(base + NE2K_CR) == 0x26) { // Abort/Complete DMA + Transmit + Start
  }

  // Prepare buffer and size
  ne2k_page_select(0);
  outb(base + NE2K_RSAR0, 0);
  outb(base + NE2K_RSAR1, 0x40);
  outb(base + NE2K_RBCR0, len & 0xFF);
  outb(base + NE2K_RBCR1, (len >> 8) & 0xFF);

  outb(base + NE2K_CR, 0x12);  // Start write

  // Load buffer
  for(uint i=0; i<len; i++) {
    outb(base + NE2K_DATA, data[i]);
  }

  // Wait until operation completed
  while((inb(base + NE2K_ISR) & NE2K_STAT_RDC) == 0) {
  }

  outb(base + NE2K_ISR, NE2K_STAT_RDC); // Clear completed bit

  outb(base + NE2K_TPSR, 0x40);
  outb(base + NE2K_TBCR0, (len & 0xFF));
  outb(base + NE2K_TBCR1, ((len >> 8) & 0xFF));
  outb(base + NE2K_CR, 0x26); // Abort/Complete DMA + Transmit + Start

  return 0;
}

// Send network packet (ethernet)
static uint eth_send(uint8_t* dst_mac, uint type, uint8_t* data, size_t len)
{
  const size_t head_len = sizeof(eth_hdr_t);
  eth_hdr_t* eh = (eth_hdr_t*)data;

  memcpy(eh->data, data, len);
  memcpy(eh->dst, dst_mac, sizeof(eh->dst));
  memcpy(eh->src, local_mac, sizeof(eh->src));
  eh->type = BSWAP_16(type);

  uint32_t eth_crc = crc32_byte(data, head_len+len);
  memcpy(&eh->data[len], &eth_crc, sizeof(eth_crc));
  return ne2k_send(data, len + head_len + ETH_CRC_LEN);
}

// Request mac address given an IP
static uint arp_request(uint8_t* ip)
{
  uint8_t broadcast_mac[MAC_LEN];
  memset(broadcast_mac, 0xFF, sizeof(broadcast_mac));

  arp_hdr_t* ah = (arp_hdr_t*)snd_buff;
  ah->hrd = BSWAP_16(ARP_HTYPE_ETHER);
  ah->pro = BSWAP_16(ARP_PTYPE_IP);
  ah->hln = MAC_LEN;
  ah->pln = IP_LEN;
  ah->op = BSWAP_16(ARP_OP_REQUEST);
  memcpy(ah->sha, local_mac, sizeof(ah->sha));
  memcpy(ah->spa, local_ip, sizeof(ah->spa));
  memcpy(ah->dha, broadcast_mac, sizeof(ah->dha));
  memcpy(ah->dpa, ip, sizeof(ah->dpa));
  const size_t head_len = sizeof(arp_hdr_t);
  return eth_send(broadcast_mac, ETH_TYPE_ARP, snd_buff, head_len);
}

// Reply an ARP request with local mac address
static uint arp_reply(uint8_t* mac, uint8_t* ip)
{
  arp_hdr_t* ah = (arp_hdr_t*)snd_buff;

  ah->hrd = BSWAP_16(ARP_HTYPE_ETHER);
  ah->pro = BSWAP_16(ARP_PTYPE_IP);
  ah->hln = MAC_LEN;
  ah->pln = IP_LEN;
  ah->op = BSWAP_16(ARP_OP_REPLY);
  memcpy(ah->sha, local_mac, sizeof(ah->sha));
  memcpy(ah->spa, local_ip, sizeof(ah->spa));
  memcpy(ah->dha, mac, sizeof(ah->dha));
  memcpy(ah->dpa, ip, sizeof(ah->dpa));
  const size_t head_len = sizeof(arp_hdr_t);
  return eth_send(mac, ETH_TYPE_ARP, snd_buff, head_len);
}

// Send IP packet
static uint ip_send(uint8_t* dst_ip, uint8_t protocol, uint8_t* data, size_t len)
{
  const size_t head_len = sizeof(ip_hdr_t);
  ip_hdr_t* ih = (ip_hdr_t*)data;
  static uint id = 0;
  id++;

  memcpy(&(data[head_len]), data, len);
  ih->verIhl = (4<<4) | 5;
  ih->tos = 0;
  ih->len = BSWAP_16(len+head_len);
  ih->id = BSWAP_16(id);
  ih->offset = BSWAP_16(0);
  ih->ttl = 128;
  ih->protocol = protocol;
  ih->checksum = 0;
  memcpy(ih->src, local_ip, sizeof(ih->src));
  memcpy(ih->dst, dst_ip, sizeof(ih->dst));

  const uint checksum = net_checksum(data, head_len);
  ih->checksum = BSWAP_16(checksum);

  // Try to find hw address in table
  uint8_t* dst_mac = find_mac_in_table(get_effective_ip(dst_ip));

  // Unsuccessful
  if(dst_mac == 0) {
    debug_putstr("net: IP: Can't find hw address for %d.%d.%d.%d. Aborted\n",
      dst_ip[0], dst_ip[1], dst_ip[2], dst_ip[3]);
    return 1;
  }

  return eth_send(dst_mac, ETH_TYPE_IP, data, head_len+len);
}

// Ensure mac address is in table
// Ask for it, if it isn't
static uint provide_mac_address(uint8_t* ip)
{
  // Get effective address
  ip = get_effective_ip(ip);

  // Find in table
  uint8_t* mac = find_mac_in_table(ip);

  // If not found, request
  uint i = 0;
  while(mac==NULL && i<16) {
    debug_putstr("net: Requesting mac for %d.%d.%d.%d...\n",
      ip[0], ip[1], ip[2], ip[3]);

    // Request it and wait
    arp_request(ip);
    wait(1000);

    // Try again
    mac = find_mac_in_table(ip);
    i++;
  }

  // Unsuccessful
  if(mac == NULL) {
    return 1;
  }

  return 0;
}

// Send UDP packet
static uint udp_send(uint8_t* dst_ip, uint16_t src_port, uint16_t dst_port,
  uint8_t* data, size_t len)
{
  typedef struct {
    uint8_t  sender[4];
    uint8_t  recver[4];
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t len;
  } udpip_hdr_t;

  // Provide hw addresss before process
  if(provide_mac_address(dst_ip) != 0 || 
    provide_mac_address(local_gate) != 0) {
    debug_putstr("net: can't find hw address for %d.%d.%d.%d. Aborted\n",
      dst_ip[0], dst_ip[1], dst_ip[2], dst_ip[3]);
    return 1;
  }

  // Clamp len
  const size_t head_len = sizeof(udp_hdr_t);
  const size_t iphead_len = sizeof(udpip_hdr_t);
  len = min(sizeof(snd_buff) - iphead_len-head_len -
    sizeof(ip_hdr_t) - sizeof(eth_hdr_t), len);

  // Generate UDP packet and send
  memset(snd_buff, 0, sizeof(snd_buff));
  memcpy(&(snd_buff[iphead_len+head_len]), data, len);

  udp_hdr_t* uh = (udp_hdr_t*)&snd_buff[iphead_len];
  uh->srcPort = BSWAP_16(src_port);
  uh->dstPort = BSWAP_16(dst_port);
  uh->len = BSWAP_16(len+head_len);

  udpip_hdr_t* ih = (udpip_hdr_t*)snd_buff;
  memcpy(ih->sender, local_ip, sizeof(ih->sender));
  memcpy(ih->recver, dst_ip, sizeof(ih->recver));
  ih->zero = 0;
  ih->protocol = IP_PROTOCOL_UDP;
  ih->len = uh->len;
  uint checksum = net_checksum(snd_buff, iphead_len+len+head_len);
  uh->checksum = BSWAP_16(checksum);
  return ip_send(dst_ip, IP_PROTOCOL_UDP, (uint8_t*)uh, head_len+len);
}

// Process received IP packet
static void ip_recv_process(uint8_t* buff)
{
  // Store only one packet
  if(rcv_buff.size > 0) {
    debug_putstr("net: packet received but discarded (buffer is full)\n");
    return;
  }

  // Check UDP packet type
  ip_hdr_t* ih = (ip_hdr_t*)buff;
  if(ih->protocol == IP_PROTOCOL_UDP) {
    size_t head_len = sizeof(ip_hdr_t);
    buff += head_len; // Advance buffer
    udp_hdr_t* uh = (udp_hdr_t*)buff;
    head_len = sizeof(udp_hdr_t);

    debug_putstr("net: UDP received: %u.%u.%u.%u:%u to port %u (%u bytes)\n",
      ih->src[0], ih->src[1], ih->src[2], ih->src[3],
      BSWAP_16(uh->srcPort), BSWAP_16(uh->dstPort), BSWAP_16(uh->len)-head_len);

    // Store it
    if(BSWAP_16(uh->dstPort) == rcv_port)
    {
      rcv_buff.addr.port = BSWAP_16(uh->srcPort);
      rcv_buff.size = min(BSWAP_16(uh->len)-head_len, sizeof(rcv_buff.buff));
      memcpy(rcv_buff.addr.ip, ih->src, sizeof(rcv_buff.addr.ip));
      memcpy(rcv_buff.buff, &buff[head_len], rcv_buff.size);
      debug_putstr("net: UDP packet was stored\n");
    }
  }
}

// Process received ARP packet
static void arp_recv_process(uint8_t* buff)
{
  arp_hdr_t* ah = (arp_hdr_t*)buff;

  if(ah->hrd == BSWAP_16(ARP_HTYPE_ETHER) &&
    ah->pro == BSWAP_16(ARP_PTYPE_IP)) {
    // If it's a reply
    if(ah->op == BSWAP_16(ARP_OP_REPLY) &&
      memcmp(ah->dpa, local_ip, sizeof(ah->dpa)) == 0) {
      // If exists in table, update entry
      uint8_t* mac = find_mac_in_table(ah->spa);
      if(mac) {
        memcpy(mac, ah->sha, sizeof(ah->sha));
        debug_putstr("net: ARP: updated: %d.%d.%d.%d : %2x:%2x:%2x:%2x:%2x:%2x\n",
          ah->spa[0], ah->spa[1], ah->spa[2], ah->spa[3],
          ah->sha[0], ah->sha[1], ah->sha[2], ah->sha[3], ah->sha[4], ah->sha[5]);
      } else { // If does not exist, create new entry
        for(uint i=0; i<ARP_TABLE_LEN; i++) {
          if(arp_table[i].ip[0] == 0 || i==ARP_TABLE_LEN-1) { // Free entry
            memcpy(arp_table[i].ip, ah->spa, sizeof(arp_table[i].ip));
            memcpy(arp_table[i].mac, ah->sha, sizeof(arp_table[i].mac));
            debug_putstr("net: ARP: added: %d.%d.%d.%d : %2x:%2x:%2x:%2x:%2x:%2x\n",
              ah->spa[0], ah->spa[1], ah->spa[2], ah->spa[3],
              ah->sha[0], ah->sha[1], ah->sha[2], ah->sha[3], ah->sha[4], ah->sha[5]);
            break;
          }
        }
      }
    // Reply in case it's a local MAC address request
    } else if(ah->op == BSWAP_16(ARP_OP_REQUEST)) {
      if(memcmp(ah->dpa, local_ip, sizeof(ah->dpa)) == 0) {
        arp_reply(ah->sha, ah->spa);
        debug_putstr("net: sent arp reply\n");
      }
    }
  }
}

// Receive network packet
static void ne2k_receive()
{
  struct {
    uint8_t  rsr;
    uint8_t  next;
    uint16_t len;
  } info;

  // Maybe more than one packet is in buffer
  // Retrieve all of them
  ne2k_page_select(1);
  uint8_t current = inb(base + NE2K_CURR);
  ne2k_page_select(0);
  uint8_t bndry = inb(base + NE2K_BNRY);

  while(bndry != current) {
    // Get reception info
    ne2k_page_select(0);
    outb(base + NE2K_RSAR0, 0);
    outb(base + NE2K_RSAR1, rx_next);
    outb(base + NE2K_RBCR0, 4);
    outb(base + NE2K_RBCR1, 0);
    outb(base + NE2K_CR, 0x12); // Read and start

    for(uint i=0; i<4; i++) {
      ((uint8_t*)&info)[i] = inb(base + NE2K_DATA);
    }

    // Get the data
    outb(base + NE2K_RSAR0, 4);
    outb(base + NE2K_RSAR1, rx_next);

    outb(base + NE2K_RBCR0, (info.len & 0xFF));
    outb(base + NE2K_RBCR1, ((info.len >> 8) & 0xFF));

    outb(base + NE2K_CR, 0x12); // Read and start

    for(uint i=0; i<info.len; i++) {
      tmp_buff[min(i, sizeof(tmp_buff)-1)] = inb(base + NE2K_DATA);
    }

    // Wait for operation completed
    while((inb(base + NE2K_ISR) & NE2K_STAT_RDC) == 0) {
    }
    // Clear completed bit
    outb(base + NE2K_ISR, NE2K_STAT_RDC);

    // Update reception pages
    if(info.next) {
      rx_next = info.next;
      if(rx_next == 0x40) {
        outb(base + NE2K_BNRY, 0x80);
      } else {
        outb(base + NE2K_BNRY, rx_next==0x46?0x7F:rx_next-1);
      }
    }

    // Update current and bndry values
    ne2k_page_select(1);
    current = inb(base + NE2K_CURR);
    ne2k_page_select(0);
    bndry = inb(base + NE2K_BNRY);

    // Wait and clear
    while((inb(base + NE2K_ISR) & NE2K_STAT_RDC) == 0) {
    }
    outb(base + NE2K_ISR, NE2K_STAT_RDC);

    // Process packet if broadcast or unicast to local_mac
    eth_hdr_t* eh = (eth_hdr_t*)tmp_buff;

    if(!memcmp(eh->dst, local_mac, sizeof(eh->dst)) ||
      !memcmp(eh->dst, arp_table[0].mac, sizeof(eh->dst)))
    {
      // Clamp length to reception buffer size
      info.len = min(sizeof(tmp_buff), info.len);

      // Redirect packets to type handlers
      switch(BSWAP_16(eh->type)) {
      case ARP_PTYPE_IP:
        ip_recv_process(eh->data);
        break;
      case ARP_PTYPE_ARP:
        arp_recv_process(eh->data);
        break;
      };
    }

    // Break if no more packets
    if(info.next == current || !info.next) {
      break;
    }
  }
}

// ne2k interrupt handler
void net_handler()
{
  // Network must be enabled
  if(network_state == NET_STATE_ENABLED) {
    uint8_t isr = 0;

    // Iterate because more interrupts
    // can be received while handling previous
    while((isr = inb(base + NE2K_ISR)) != 0) {
      if(isr & NE2K_STAT_RX) {
        ne2k_receive();
      }
      if(isr & NE2K_STAT_TX) {
      }

      // Clear interrupt bits
      outb(base + NE2K_ISR, isr);
    }
  }
 
  lapic_eoi();
  return;
}

// Initialize network
void net_init()
{
  // Reset translation table
  memset(arp_table, 0, sizeof(arp_table));

  // Reset received data
  memset(&rcv_buff.addr, 0, sizeof(rcv_buff.addr));
  rcv_buff.size = 0;

  // Detect card
  network_state = NET_STATE_DISABLED;
  uint net_irq = 0x0B; // network irq
  {
    PCI_device_t* pdev = NULL;

    // Find a compatible device
    for(uint i=0; i<NUM_COMPATIBLE_DEVICES; i++) {
      pdev = pci_find_device(ne2k_compatible[i].vendor_id,
        ne2k_compatible[i].device_id);

      if(pdev) {
        break;
      }
    }

    // If found, check
    if(pdev) {
      base = pdev->bar0 & ~3;
      net_irq = pdev->interrput_line;
      outb(base + NE2K_IMR, 0x80); // Disable interrupts except reset
      outb(base + NE2K_ISR, 0xFF); // Clear interrupts
      outb(base + NE2K_RESET, inb(base + NE2K_RESET)); // Reset
      wait(250); // Wait
      if((inb(base + NE2K_ISR) == NE2K_STAT_RST)) { // Detect reset
        debug_putstr("net: ne2000 compatible nic found. base=%x irq=%d\n", 
        base, net_irq);
        network_state = NET_STATE_ENABLED;
      }
    }
  }

  // Abort if network is not enabled or device not found
  if(network_state != NET_STATE_ENABLED) {
    debug_putstr("net: compatible nic not found\n");
    return;
  }

  // Install IRQ handler
  set_network_IRQ(net_irq);

  // Reset, and wait
  outb(base + NE2K_RESET, inb(base + NE2K_RESET));
  while((inb(base + NE2K_ISR) & NE2K_STAT_RST) == 0) {
  }

  debug_putstr("net: nic reset\n");

  ne2k_page_select(0);
  outb(base + NE2K_CR, 0x21);  // Stop DMA and MAC
  outb(base + NE2K_DCR, 0x48); // Access by bytes
  outb(base + NE2K_TCR, 0xE0); // Transmit: normal operation, aut-append and check CRC
  outb(base + NE2K_RCR, 0xDE); // Receive: Accept and buffer
  outb(base + NE2K_IMR, 0x00); // Disable interrupts
  outb(base + NE2K_ISR, 0xFF); // NE2K_ISR must be cleared

  outb(base + NE2K_TPSR, 0x40 );       // Transmit page start
  outb(base + NE2K_PSTART, rx_next-1); // Receive page start
  outb(base + NE2K_PSTOP, 0x80);       // Receive page stop
  outb(base + NE2K_BNRY, rx_next-1);   // Boundary
  ne2k_page_select(1);
  outb(base + NE2K_CURR, rx_next);     // Change current recv page

  ne2k_page_select(0);
  outb(base + NE2K_RSAR0, 0x00);       // Remote start address
  outb(base + NE2K_RSAR1, 0x00);
  outb(base + NE2K_RBCR0, 24);         // 24 bytes count
  outb(base + NE2K_RBCR1, 0x00);
  outb(base + NE2K_CR, 0x0A);
  // Print MAC
  debug_putstr("net: MAC: ");
  for(uint i=0; i<6; i++) {
    local_mac[i] = inb(base + NE2K_DATA);
    inb(base + NE2K_DATA); // Word sized, read again to advance
    debug_putstr("%2x ", local_mac[i]);
  }
  debug_putstr("\n");

  // Listen to this MAC
  ne2k_page_select(1);
  outb(base + NE2K_PAR0, local_mac[0]);
  outb(base + NE2K_PAR1, local_mac[1]);
  outb(base + NE2K_PAR2, local_mac[2]);
  outb(base + NE2K_PAR3, local_mac[3]);
  outb(base + NE2K_PAR4, local_mac[4]);
  outb(base + NE2K_PAR5, local_mac[5]);

  // Clear multicast
  for(uint i=NE2K_MAR0; i<=NE2K_MAR7; i++) {
    outb(base + i, 0);
  }

  ne2k_page_select(0);
  outb(base + NE2K_CR, 0x22); // Start and no DMA
  // Set interrupt mask to read/write
  outb(base + NE2K_IMR, NE2K_IMR_PRXE|NE2K_IMR_PTXE);

  // Add broadcast address to translation table
  memset(arp_table[0].mac, 0xFF, sizeof(arp_table[0].mac));
  memset(arp_table[0].ip, 0xFF, sizeof(arp_table[0].ip));
}

// Send buffer to dst
uint net_send(net_address_t* dst, uint8_t* buff, size_t len)
{
  if(network_state == NET_STATE_ENABLED) {
    return udp_send(dst->ip, UDP_SEND_PORT, dst->port,
      buff, len);
      return 0;
  }
  return 1;
}

// Receive data
uint net_recv(net_address_t* src, uint8_t* buff, size_t buff_size)
{
  if(network_state == NET_STATE_ENABLED) {
    // Now check if there is something in the system buffer
    if(rcv_buff.size > 0) {
      uint ret = min(rcv_buff.size, buff_size);
      memcpy(src, &rcv_buff.addr, sizeof(rcv_buff.addr));
      memcpy(buff, rcv_buff.buff, ret);
      rcv_buff.size = 0;
      return ret;
    }
  }
  return 0;
}

// Get network state
uint net_get_state()
{
  return network_state;
}

// Set reception port
void net_recv_set_port(uint16_t port)
{
  if(port != rcv_port) {
    // Clear buffer if port is different
    rcv_buff.size = 0;
    rcv_port = port;
  }
}
