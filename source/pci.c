// PCI

#include "types.h"
#include "x86.h"
#include "pci.h"
#include "ulib/ulib.h"

#define PCI_CONFIG_ADDR_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

#define MAX_PCI_DEVICE 16
static uint pci_count = 0;
static PCI_device_t pci_devices[MAX_PCI_DEVICE];

// Read config
static uint32_t pci_read_config(uint32_t pci_dev, uint8_t offset)
{
  const uint32_t data = 0x80000000 | pci_dev | (offset & 0xFC);
  outd(PCI_CONFIG_ADDR_PORT, data);
  return ind(PCI_CONFIG_DATA_PORT);
}

// Scan devices in bus 0
void pci_init()
{
  // Initialize PCI table
  memset(pci_devices, 0, sizeof(pci_devices));

  // Scan bus 0 devices
  const uint8_t bus = 0;
  pci_count = 0;
  for(uint8_t slot=0; slot<32; slot++) {
    for(uint8_t func=0; func<8; func++) {
      uint32_t pci_dev_addr =
        ((bus<<16) | (slot<<11) | (func<<8));

      uint16_t vendor_id = pci_read_config(pci_dev_addr, 0)&0xFFFF;

      // Discard unused
      if(vendor_id == 0xFFFF) {
        continue;
      }

      // Read device
      uint32_t *p = (uint32_t *)&pci_devices[pci_count];

      for(uint i=0; i<sizeof(PCI_device_t); i+=4) {
        *p++ = pci_read_config(pci_dev_addr, i);
      }

      pci_count++;
      if(pci_count >= MAX_PCI_DEVICE) {
        debug_putstr("There are unlisted PCI devices\n");
        return;
      }
    }
  }

  // Print debug info
  debug_putstr("PCI initialized\n");
  for(uint i=0; i<pci_count; i++) {
    debug_putstr("PCI device: vendor:%4x  device:%4x\n",
      pci_devices[i].vendor_id, pci_devices[i].device_id);
  }
}

// Find device in bus 0
// Previous initialization is required
PCI_device_t *pci_find_device(uint16_t vendor, uint16_t device)
{
  for(uint i=0; i<pci_count; i++) {
    if(vendor==pci_devices[i].vendor_id &&
      device==pci_devices[i].device_id) {
      return &pci_devices[i];
    }
  }
  return NULL;
}
