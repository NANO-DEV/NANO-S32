// User program: Basic net interface

#include "types.h"
#include "ulib/ulib.h"

// Use this arbitrarily chosen UDP port
#define UNET_PORT 8086

// Program called with recv argument
static int unet_recv()
{
  // Set reception port
  recv_set_port(UNET_PORT);

  // Receive in buffer
  char recv_buff[64] = {0};
  net_address_t src_addr;
  const uint result =
    recv(&src_addr, (uint8_t*)recv_buff, sizeof(recv_buff));

  // If result == 0, nothing has been received
  if(result == 0) {
    putstr("Buffer is empty\n");

  // Else, result contains number of received bytes
  } else {
    // Append a 0 after received data
    // to safely use as string
    recv_buff[min(result, sizeof(recv_buff)-1)] = 0;

    // Show source address and contents
    putstr("Received %s from %u.%u.%u.%u:%u\n", recv_buff,
      src_addr.ip[0], src_addr.ip[1],
      src_addr.ip[2], src_addr.ip[3],
      src_addr.port);
  }
  return (int)result;
}


// Program called with send argument
static int unet_send(net_address_t *dst_addr, char *message)
{
  // Send
  const uint result =
    send(dst_addr, (uint8_t*)message, strlen(message)+1);

  // If result == 0, then data was sent
  if(result == NO_ERROR) {
    putstr("Sent %s to %u.%u.%u.%u:%u\n", message,
      dst_addr->ip[0], dst_addr->ip[1],
      dst_addr->ip[2], dst_addr->ip[3],
      dst_addr->port);
  } else {
    // Else, failed
    putstr("Failed to send\n");
  }

  return (int)result;
}





// Section: Chat related functions
// *******************************

// Auxiliar function for unet_chat: Clear current screen line
static void unet_chat__clear_current_line()
{
  const uint SCREEN_WIDTH = 80;
  putstr("\r");
  for(uint i=0; i<SCREEN_WIDTH-1;i++) {
    putc(' ');
  }
  putstr("\r");
}

// Auxiliar function for unet_chat: Handle message reception
static void unet_chat__receive(net_address_t *remote_chat_addr)
{
  // Receive in buffer
  char recv_msg[256] = {0};
  net_address_t recv_addr;
  const uint result = recv(&recv_addr, (uint8_t*)recv_msg, sizeof(recv_msg));

  // If received something and source is the remote chat address...
  if(result != 0 &&
    memcmp(recv_addr.ip, remote_chat_addr->ip, sizeof(recv_addr.ip)) == 0 &&
    recv_addr.port == UNET_PORT) {

    // Append a 0 after received data
    // to safely use as string
    recv_msg[min(result, sizeof(recv_msg)-1)] = 0;

    // Go to the beginning of the current line
    unet_chat__clear_current_line();

    // Print source address and message
    putstr("%d.%d.%d.%d: %s\n",
      recv_addr.ip[0], recv_addr.ip[1],
      recv_addr.ip[2], recv_addr.ip[3],
      recv_msg);
  }
}

// Auxiliar function for unet_chat: process local input
// Return UNET_CHAT_EXIT if chat must finish
// Return UNET_CHAT_CONTINUE otherwise
#define UNET_CHAT_CONTINUE 0
#define UNET_CHAT_EXIT     1
static uint unet_chat__process_local_input(
  net_address_t *remote_chat_addr,
  char *send_msg_buff, size_t send_msg_buff_size)
{
  // Process keyboard input (don't wait for a keypress)
  const uint pressed_key = getkey(GETKEY_WAITMODE_NOWAIT);

  // If key RETURN: Send
  if(pressed_key == KEY_RETURN) {
    // Set a 0 at the end of the buffer
    // to safely use as string
    send_msg_buff[send_msg_buff_size-1] = 0;

    if(strlen(send_msg_buff) > 0) {
      const uint result = send(remote_chat_addr,
        (uint8_t*)send_msg_buff, strlen(send_msg_buff));

      unet_chat__clear_current_line();

      // If result == 0, then data was sent
      if(result == NO_ERROR) {
        putstr("local: %s\n", send_msg_buff);
      } else {
        // Else, failed
        putstr("Failed to send message\n");
      }
    }

    // Reset buffer
    memset(send_msg_buff, 0, send_msg_buff_size);
  }

  // If key BACKSPACE or DEL: Delete last char
  else if((pressed_key == KEY_BACKSPACE || pressed_key == KEY_DEL) &&
    strlen(send_msg_buff) > 0) {
    send_msg_buff[strlen(send_msg_buff)-1] = 0;

    // Rewrite buffer in screen
    unet_chat__clear_current_line();
    putstr("%s", send_msg_buff);
  }

  // If key is a char: Append it to current user string
  else if(pressed_key >= ' ' && pressed_key <= '}') {
    const uint current_char_index =
      min(send_msg_buff_size-2, strlen(send_msg_buff));

    // Note in the previous computation of current_char_index
    // Maximum index to append a char is send_msg_buff_size-2
    // because send_msg_buff_size-1 must be always 0
    // to ensure send_msg_buff contains a valid string

    send_msg_buff[current_char_index] = pressed_key;
    unet_chat__clear_current_line();
    putstr("%s", send_msg_buff);
  }

  // If key is KEY_ESC: Exit chat
  else if(pressed_key == KEY_ESC) {
    // Acknowledge ESC key pressed and return with exit value
    unet_chat__clear_current_line();
    putstr("-ESC-\n");
    return UNET_CHAT_EXIT;
  }

  // Unless key ESC is pressed, chat must continue
  return UNET_CHAT_CONTINUE;
}


// Program called with chat argument: Main chat function
static int unet_chat(net_address_t *remote_addr)
{
  // Show chat banner
  putstr("Chat with %u.%u.%u.%u. Press ESC to exit\n",
    remote_addr->ip[0], remote_addr->ip[1],
    remote_addr->ip[2], remote_addr->ip[3]);

  // Set reception port
  recv_set_port(UNET_PORT);

  // Initialize send message buffer
  char send_msg_buff[256] = {0};
  memset(send_msg_buff, 0, sizeof(send_msg_buff));

  // Main loop
  uint user_command = 0;
  do {
    // Show received messages, if any
    unet_chat__receive(remote_addr);

    // Process local user input, if any
    user_command = unet_chat__process_local_input(
      remote_addr, send_msg_buff, sizeof(send_msg_buff));

  } while(user_command != UNET_CHAT_EXIT);

  // Exit chat
  return 0;
}


// End of section: Chat related functions
// **************************************





// Program entry point
int main(int argc, char *argv[])
{
  // Switch functionality depending on call params
  if(argc == 2 && strcmp(argv[1], "recv") == 0) {

    // Syntax:
    // unet recv
    // Show received data

    return unet_recv();

  } else if(argc == 5 && strcmp(argv[1], "send") == 0) {

    // Syntax:
    // unet send <IP> <port> <single_word>
    // Send single word to IP:port

    // Parse IP from string
    net_address_t dst_addr;
    str_to_ip(dst_addr.ip, argv[2]);
    dst_addr.port = stou(argv[3]);

    return unet_send(&dst_addr, argv[4]);

  } else if(argc == 3 && strcmp(argv[1], "chat") == 0) {

    // Syntax:
    // unet chat <remote_IP>
    // Start a chat with a remote host (remote_IP)

    // Parse IP from string
    net_address_t remote_chat_addr;
    str_to_ip(remote_chat_addr.ip, argv[2]);
    remote_chat_addr.port = UNET_PORT;

    return unet_chat(&remote_chat_addr);

  } else {
    // Call syntax not recognized
    // Print usage
    putstr("usage: %s <send <dst_ip> <dst_port> <word> | recv | chat <dst_ip>>\n",
      argv[0]);
  }

  return 0;
}
