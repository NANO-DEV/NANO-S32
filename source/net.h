// Network functionality

#ifndef _NET_H
#define _NET_H

extern uint8_t local_ip[IP_LEN];
extern uint8_t local_gate[IP_LEN];

// Initialize network
void net_init();

// Send buffer to dst
uint net_send(net_address_t* dst, uint8_t* buff, size_t len);

// Enable reception in this port and disable all others
// Must be called once before start calling net_recv
void net_recv_set_port(uint16_t port);

// Get and remove from buffer received data.
// src and buff are filled by the function
// Call net_recv_set_port once before calling to this 
// function to enable a reception port
uint net_recv(net_address_t* src, uint8_t* buff, size_t buff_size);

enum NET_STATE
{
  NET_STATE_DISABLED = 0,
  NET_STATE_ENABLED = 1,
  NET_STATE_UNINITIALIZED = 0xFFFFFFFF
};

// Get network state
// See NET_STATE enum for returned values
uint net_get_state();

#endif // _NET_H
