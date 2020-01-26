// Sound functionality
// Sound Blaster 16 driver

#include "types.h"
#include "x86.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "fs.h"
#include "sound.h"

// WAV fileformat
#define WAV_RIFF 0x46464952
#define WAV_WAVE 0x45564157
typedef struct RIFF_chunk_t {
  uint32_t RIFF;
  uint32_t next_chunk_size;
  uint32_t RIFF_type;
} RIFF_chunk_t;

#define WAV_FMT 0x20746D66
typedef struct fmt_chunk_t {
  uint32_t fmt;
  uint32_t fmt_length;
  uint16_t wave_type;
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t bytes_per_second;
  uint16_t block_alignment;
  uint16_t bit_resolution;
} fmt_chunk_t;

#define WAV_DATA 0x61746164
typedef struct data_chunk_t {
  uint32_t data;
  uint32_t data_length;
} data_chunk_t;

// ISA DMA
#define DMA_MASK_ON 0x04
static const uint8_t DMA_SINGLE_CHANNEL_MASK[2] = {0x0A, 0xD4};
static const uint8_t DMA_FLIPFLOP_RESET[2]      = {0x0C, 0xD8};
static const uint8_t DMA_MODE[2]                = {0x0B, 0xD6};
static const uint8_t DMA_STATUS[2]              = {0x08, 0xD0};
static const uint8_t DMA_PAGE_ADDRESS[8] =
  {0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A};
static const uint8_t DMA_START_ADDRESS[8] =
  {0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC};
static const uint8_t DMA_COUNT[8] =
  {0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE};

// Sound Blaster 16
// Mixer register status
#define SB_IRQ_2  0x1
#define SB_IRQ_5  0x2
#define SB_IRQ_7  0x4
#define SB_IRQ_10 0x8

#define SB_DMA_0 0x01
#define SB_DMA_1 0x02
#define SB_DMA_3 0x08
#define SB_DMA_5 0x20
#define SB_DMA_6 0x40
#define SB_DMA_7 0x80

// Mixer ports
#define MIXER_ADDRESS_PORT 0x04
#define MIXER_DATA_PORT    0x05

#define MIXER_RESET_CMD     0x00
#define MIXER_READ_IRQ_PORT 0x80
#define MIXER_READ_DMA_PORT 0x81

// Interrupt status
#define MIXER_INT_STATUS_PORT      0x82
#define MIXER_INT_STATUS_DMA_8BIT  0x01
#define MIXER_INT_STATUS_DMA_16BIT 0x02

// DSP commands
#define DSP_DAC_SPEAKER_TURN_ON   0xD1
#define DSP_DAC_SPEAKER_TURN_OFF  0xD3
#define DSP_PAUSE_DMA_MODE        0xD0
#define DSP_EXIT_AUTO_DMA_MODE_8  0xDA
#define DSP_EXIT_AUTO_DMA_MODE_16 0xD9
#define DSP_CMD_VERSION           0xE1

#define DSP_PLAY_AUTOINIT_8BIT  0xC6
#define DSP_PLAY_AUTOINIT_16BIT 0xB6
#define DSP_PLAY_SCT_8BIT       0xC0
#define DSP_PLAY_SCT_16BIT      0xB0
#define DSP_SET_SAMPLE_RATE     0x41

// DSP format
#define DSP_FORMAT_8BIT_MONO    0x00
#define DSP_FORMAT_8BIT_STEREO  0x20
#define DSP_FORMAT_16BIT_MONO   0x10
#define DSP_FORMAT_16BIT_STEREO 0x30

// DSP response
#define DSP_RESET_SUCCESS    0xAA
#define DSP_DATA_IN_BUFFER   0x80

// DSP ports
#define DSP_ADDR_WIRTE             0x0C
#define DSP_ADDR_READ_DATA         0x0A
#define DSP_ADDR_READ_STATUS_8BIT  0x0E
#define DSP_ADDR_READ_STATUS_16BIT 0x0F
#define DSP_ADDR_RESET             0x06

// Device info and sound system state
static struct device_struct {
  bool     enabled;
  uint16_t base;          // Sound Blaster base address
  uint8_t  DMA8_channel;  // DMA channel (8bit)
  uint8_t  DMA16_channel; // DMA channel (16bit)
} device;

// DMA buffer
#define DMA_BUFFER_ADDRESS 0x70000 // Linear memory address
static uint8_t *DMA_buffer = (uint8_t*)DMA_BUFFER_ADDRESS;
const uint16_t  DMA_buffer_size = 0x2000; // Bytes
static const uint16_t buffer_address_high = DMA_BUFFER_ADDRESS >> 16;
static const uint16_t buffer_address_low = DMA_BUFFER_ADDRESS & 0xFFFF;

// Currently playing wav file
static struct playing_file_struct {
  char path[MAX_PATH];
  uint pos;
  uint bits;
  uint rate;
  uint channels;
  uint bytes_per_sample;
  uint length_seconds;
} playing_file;

static struct play_state_struct {
  volatile int remaining_samples; // Amount of samples to be played
  volatile int read_remaining_bytes; // Amount of bytes to be read
  volatile bool is_playing;
  uint started_time_seconds;
  uint16_t read_buffer_half; // 0=First half 1=Second half
} play_state;

// Read a byte from the mixer on the Sound Blaster
static void sb_write_mixer(uint8_t addr, uint8_t value)
{
  outb(device.base + MIXER_ADDRESS_PORT, addr);
  outb(device.base + MIXER_DATA_PORT, value);
}

// Read a byte from the mixer on the Sound Blaster
static uint8_t sb_read_mixer(uint8_t address)
{
  outb(device.base + MIXER_ADDRESS_PORT, address);
  return inb(device.base + MIXER_DATA_PORT);
}

// Try to reset a Sound Blaster at a given address
// Return true if succesful, false otherwise
static bool sb_reset_DSP(uint16_t addr)
{
  // Reset the DSP
  outb(addr + DSP_ADDR_RESET, 1);
  wait(10);
  outb(addr + DSP_ADDR_RESET, 0);
  wait(10);

  // Check if reset was succesfull
  if((inb(addr + DSP_ADDR_READ_STATUS_8BIT) & DSP_DATA_IN_BUFFER) &&
    (inb(addr + DSP_ADDR_READ_DATA) == DSP_RESET_SUCCESS))
  {
    // DSP was found
    return TRUE;
  }

  // DSP was not found
  return FALSE;
}

// Write a byte to the DSP on the Sound Blaster
static void sb_write_DSP(uint8_t value)
{
  // Wait for the DSP to be ready to accept data
  while(inb(device.base + DSP_ADDR_WIRTE) & DSP_DATA_IN_BUFFER);
  // Write byte
  outb(device.base + DSP_ADDR_WIRTE, value);
}

// Read a byte from the DSP on the Sound Blaster
static uint8_t sb_read_DSP()
{
  // Wait for the DSP to be ready
  while(!(inb(device.base + DSP_ADDR_READ_STATUS_8BIT) & DSP_DATA_IN_BUFFER));
  // Read byte
  return inb(device.base + DSP_ADDR_READ_DATA);
}

// Try to find a Sound Blaster
static void sb_find()
{
  // Nothing found yet
  device.base = 0;

  // Check for Sound Blaster at ports
  // 210, 220, 230, 240, 250, 260 and 280
  for(uint8_t i = 1; i < 9; i++) {
    if(i != 7) {
      const uint16_t addr = 0x200 + (i << 4);
      if(sb_reset_DSP(addr)) {
        device.base = addr;
        break;
      }
    }
  }
}

// Return true if a Sound Blaster has been found
static bool sb_found()
{
  return device.base != 0;
}

// Convert bytes to samples
static uint bytes_to_samples(uint bytes)
{
  return bytes/playing_file.bytes_per_sample;
}

// Load one half of the DMA buffer from the file
static void read_buffer(uint8_t half_index)
{
  const size_t half_buff_size = DMA_buffer_size / 2;

  if(play_state.read_remaining_bytes <= 0) {
    return;
  }
  uint8_t *buff = DMA_buffer + half_buff_size * half_index;

  // If the remaining part of the file is smaller than half the size
  // of the buffer, load it and fill out with silence
  if(play_state.read_remaining_bytes < (int)half_buff_size) {

    const uint8_t zero_value =
      playing_file.bits == 8 ? 0x80 : 0;
    memset(buff, zero_value, half_buff_size);

    const uint result = fs_read_file(buff, playing_file.path,
      playing_file.pos, play_state.read_remaining_bytes);
    if((int)result != play_state.read_remaining_bytes) {
      debug_putstr("Sound: Can't read wave file data at %d\n",
        playing_file.pos);
    }
    playing_file.pos += play_state.read_remaining_bytes;
    play_state.read_remaining_bytes = 0;

  } else {

    const uint result = fs_read_file(buff, playing_file.path,
      playing_file.pos, half_buff_size);
    if(result != half_buff_size) {
      debug_putstr("Sound: Can't read wave file data at %d\n",
        playing_file.pos);
    }
    playing_file.pos += half_buff_size;
    play_state.read_remaining_bytes -= half_buff_size;
  }
}

// Program the DMA controller. The DSP is instructed to play blocks of
// half the total buffer size and then generate an interrupt
// which allows the program to load the next part that should be played
static void sb_auto_init_playback()
{
  const uint8_t DMA_channel =
    playing_file.bits == 8 ? device.DMA8_channel :
    playing_file.bits == 16 ? device.DMA16_channel :
    0;

  const uint8_t bits =
    playing_file.bits == 8 ? 0 :
    playing_file.bits == 16 ? 1 :
    0;

  const uint8_t channel_mask = DMA_channel % 4;

  const uint16_t buff_addr_low =
    playing_file.bits == 8 ? buffer_address_low :
    playing_file.bits == 16 ? buffer_address_low >> 1 :
    0;

  // Mask DMA channel
  outb(DMA_SINGLE_CHANNEL_MASK[bits], DMA_MASK_ON|channel_mask);
  outb(DMA_FLIPFLOP_RESET[bits], 0);         // Clear byte pointer
  outb(DMA_MODE[bits], 0x58 | channel_mask); // Set mode
  outb(DMA_START_ADDRESS[DMA_channel], buff_addr_low & 0xFF); // Write offset
  outb(DMA_START_ADDRESS[DMA_channel], buff_addr_low >> 8);

  /*
    The mode consists of the following:
    0x58+x = binary 01 01 10 xx
                    |  |  |  |
                    |  |  |  +- DMA channel
                    |  |  +---- Read operation (the DSP reads from memory)
                    |  +------- Auto init mode
                    +---------- Block mode
  */

  // Write the page to the DMA controller
  outb(DMA_PAGE_ADDRESS[DMA_channel], buffer_address_high);

  // Set the block length to buffer size
  const uint16_t DMA_buff_size =
    playing_file.bits == 8 ? DMA_buffer_size-1 :
    playing_file.bits == 16 ? (DMA_buffer_size/2)-1 :
    0;

  outb(DMA_COUNT[DMA_channel], DMA_buff_size & 0xFF);
  outb(DMA_COUNT[DMA_channel], DMA_buff_size >> 8);
  outb(DMA_SINGLE_CHANNEL_MASK[bits], channel_mask); // Unmask DMA channel

  // Set sample rate
  sb_write_DSP(DSP_SET_SAMPLE_RATE);
  sb_write_DSP(playing_file.rate >> 8);
  sb_write_DSP(playing_file.rate & 0xFF);

  // Set the block length to half buffer size minus one
  const uint16_t buffer_size =
    playing_file.bits == 8 ? (DMA_buffer_size/2)-1 :
    playing_file.bits == 16 ? (DMA_buffer_size/4)-1 :
    0;

  const uint8_t command =
    playing_file.bits == 8 ? DSP_PLAY_AUTOINIT_8BIT :
    playing_file.bits == 16 ? DSP_PLAY_AUTOINIT_16BIT :
    0;
  const uint8_t format =
    playing_file.channels == 1 ?
      playing_file.bits == 8 ? DSP_FORMAT_8BIT_MONO :
      playing_file.bits == 16 ? DSP_FORMAT_16BIT_MONO : 0 :
    playing_file.channels == 2 ?
      playing_file.bits == 8 ? DSP_FORMAT_8BIT_STEREO :
      playing_file.bits == 16 ? DSP_FORMAT_16BIT_STEREO : 0 :
    0;

  sb_write_DSP(command);
  sb_write_DSP(format);
  sb_write_DSP(buffer_size & 0xFF);
  sb_write_DSP(buffer_size >> 8);
}

// Program the DMA controller to play a single block
static void sb_single_cycle_playback()
{
  const uint8_t DMA_channel =
    playing_file.bits == 8 ? device.DMA8_channel :
    playing_file.bits == 16 ? device.DMA16_channel :
    0;

  const uint8_t bits =
    playing_file.bits == 8 ? 0 :
    playing_file.bits == 16 ? 1 :
    0;

  const uint8_t channel_mask = DMA_channel % 4;

  const uint16_t buff_offset =
    buffer_address_low + play_state.read_buffer_half * DMA_buffer_size/2;

  const uint16_t buff_addr_low =
    playing_file.bits == 8 ? buff_offset :
    playing_file.bits == 16 ? buff_offset :
    0;

  // Mask DMA channel
  outb(DMA_SINGLE_CHANNEL_MASK[bits], DMA_MASK_ON|channel_mask);
  outb(DMA_FLIPFLOP_RESET[bits], 0);         // Clear byte pointer
  outb(DMA_MODE[bits], 0x48 | channel_mask); // Set mode
  outb(DMA_START_ADDRESS[DMA_channel], buff_addr_low & 0xFF);
  outb(DMA_START_ADDRESS[DMA_channel], buff_addr_low >> 8); // Write offset

  /*
    The mode consists of the following:
    0x48+x = binary 01 00 10 xx
                    |  |  |  |
                    |  |  |  +- DMA channel
                    |  |  +---- Read operation (the DSP reads from memory)
                    |  +------- Single cycle mode
                    +---------- Block mode
  */

  // Write the page to the DMA controller
  outb(DMA_PAGE_ADDRESS[DMA_channel], buffer_address_high);

  // Set the block length
  play_state.remaining_samples--; // DMA needs this (see specification)
  outb(DMA_COUNT[DMA_channel], play_state.remaining_samples & 0xFF);
  outb(DMA_COUNT[DMA_channel], play_state.remaining_samples >> 8);
  outb(DMA_SINGLE_CHANNEL_MASK[bits], channel_mask); // Unmask DMA channel

  // Set sample rate
  sb_write_DSP(DSP_SET_SAMPLE_RATE);
  sb_write_DSP(playing_file.rate >> 8);
  sb_write_DSP(playing_file.rate & 0xFF);

  // DSP command single cycle playback
  const uint8_t command =
    playing_file.bits == 8 ? DSP_PLAY_SCT_8BIT :
    playing_file.bits == 16 ? DSP_PLAY_SCT_16BIT :
    0;
  const uint8_t format =
    playing_file.channels == 1 ?
      playing_file.bits == 8 ? DSP_FORMAT_8BIT_MONO :
      playing_file.bits == 16 ? DSP_FORMAT_16BIT_MONO : 0 :
    playing_file.channels == 2 ?
      playing_file.bits == 8 ? DSP_FORMAT_8BIT_STEREO :
      playing_file.bits == 16 ? DSP_FORMAT_16BIT_STEREO : 0 :
    0;

  const uint16_t remaining_samples =
    playing_file.bits == 8 ?
      play_state.remaining_samples/playing_file.channels :
      (uint16_t)play_state.remaining_samples;

  sb_write_DSP(command);
  sb_write_DSP(format);
  sb_write_DSP(remaining_samples & 0xFF);
  sb_write_DSP(remaining_samples >> 8);

  // Nothing left to play
  play_state.remaining_samples = 0;
}

// IRQ service routine
// Called when the DSP has finished playing a block
void sound_handler()
{
  disable_interrupts();

  const uint8_t interrupt_status =
    sb_read_mixer(MIXER_INT_STATUS_PORT);

  debug_putstr("Sound: Handling interruption (%2x)\n",
    interrupt_status);

  if(io_sound_is_enabled()) {
    // Take appropriate action
    if(play_state.is_playing) {
      const int num_samples_in_buffer = bytes_to_samples(DMA_buffer_size);

      play_state.remaining_samples -= num_samples_in_buffer/2;
      if(play_state.remaining_samples > 0) {

        read_buffer(play_state.read_buffer_half);
        if(play_state.remaining_samples <= num_samples_in_buffer/2) {
          play_state.read_buffer_half ^= 1;
          sb_single_cycle_playback();

        } else if(play_state.remaining_samples <= num_samples_in_buffer) {
          if(playing_file.bits == 8) {
            sb_write_DSP(DSP_EXIT_AUTO_DMA_MODE_8);
          } else if(playing_file.bits == 16) {
            sb_write_DSP(DSP_EXIT_AUTO_DMA_MODE_16);
          }
        }
        play_state.read_buffer_half ^= 1;
      } else {
        play_state.is_playing = FALSE;
        debug_putstr("Sound: Play sound %s finished\n",
          playing_file.path);
      }
    }

    // Acknowledge to DSP
    if(interrupt_status & MIXER_INT_STATUS_DMA_8BIT) {
      inb(device.base + DSP_ADDR_READ_STATUS_8BIT);
    }
    if(interrupt_status & MIXER_INT_STATUS_DMA_16BIT) {
      inb(device.base + DSP_ADDR_READ_STATUS_16BIT);
    }
  }

  // Acknowledge hardware interrupt
  lapic_eoi();
  enable_interrupts();
}

// Return true if a sound is still playing
bool io_sound_is_playing()
{
  if(play_state.is_playing) {
    const uint time_seconds = io_gettimer() / 1000;
    const uint time_elapsed = time_seconds - play_state.started_time_seconds;
    if(time_elapsed > playing_file.length_seconds) {
      debug_putstr("Sound: Forced sound stop (%s). lenght=%us elapsed=%us\n",
        playing_file.path, playing_file.length_seconds, time_elapsed);
      io_sound_stop();
    }
  }
  return play_state.is_playing;
}

// Stop playing sound
void io_sound_stop()
{
  // Stop DMA transfer
  if(io_sound_is_enabled()) {
    sb_write_DSP(DSP_PAUSE_DMA_MODE);
    sb_write_DSP(DSP_DAC_SPEAKER_TURN_OFF);
  }

  play_state.is_playing = FALSE;
}

// Start playing a WAV file
uint sb_play(const char *wav_file_path)
{
  memset(playing_file.path, 0, sizeof(playing_file.path));
  strncpy(playing_file.path, wav_file_path,
    sizeof(playing_file.path));

  // Start playback in buffer 0 and clear the buffer
  play_state.read_buffer_half = 0;
  memset(DMA_buffer, 0, DMA_buffer_size);

  // Read RIFF chunk
  RIFF_chunk_t RIFF_chunk = {0};
  uint result =
    fs_read_file(&RIFF_chunk, playing_file.path, 0, sizeof(RIFF_chunk));

  if(result != sizeof(RIFF_chunk) ||
    RIFF_chunk.RIFF != WAV_RIFF ||
    RIFF_chunk.RIFF_type != WAV_WAVE) {
    debug_putstr("Sound: Can't read wave file RIFF (%s)\n",
      playing_file.path);

    return ERROR_IO;
  }
  playing_file.pos = sizeof(RIFF_chunk);

  // Read fmt chunk
  fmt_chunk_t fmt_chunk = {0};
  do {
    result = fs_read_file(&fmt_chunk, playing_file.path,
      playing_file.pos, sizeof(fmt_chunk));

    if(result != sizeof(fmt_chunk)) {
      debug_putstr("Sound: Can't read wave file fmt (%s)\n",
        playing_file.path);

      return ERROR_IO;
    }
    playing_file.pos += fmt_chunk.fmt_length + 8;
  } while(fmt_chunk.fmt != WAV_FMT);

  // Set format
  playing_file.bits = fmt_chunk.bit_resolution;
  if(playing_file.bits != 8 && playing_file.bits != 16) {
    debug_putstr("Sound: Unsupported bit depth (%s,%d)\n",
      playing_file.path, playing_file.bits);
    return ERROR_IO;
  }

  playing_file.rate = fmt_chunk.sample_rate;
  playing_file.channels = fmt_chunk.channels;
  if(playing_file.channels != 1 &&
    playing_file.channels != 2) {
    debug_putstr("Sound: Unsupported number of channels (%s,%d)\n",
      playing_file.path, playing_file.channels);
    return ERROR_IO;
  }
  playing_file.bytes_per_sample = fmt_chunk.bit_resolution/8;

  // Read data chunk
  data_chunk_t data_chunk = {0};
  do {
    result = fs_read_file(&data_chunk, playing_file.path,
      playing_file.pos, sizeof(data_chunk));

    if(result != sizeof(data_chunk)) {
      debug_putstr("Sound: Can't read wave file data (%s)\n",
        playing_file.path);
      return ERROR_IO;
    }
    if(data_chunk.data != WAV_DATA) {
      playing_file.pos += data_chunk.data_length + 8;
    }
  } while(data_chunk.data != WAV_DATA);
  play_state.read_remaining_bytes = data_chunk.data_length;
  play_state.remaining_samples = bytes_to_samples(data_chunk.data_length);
  playing_file.length_seconds = 1 +
    (play_state.remaining_samples / playing_file.channels) /
    playing_file.rate;

  debug_putstr("Sound: Read wave file data (%s, %d bytes)\n",
    playing_file.path, play_state.read_remaining_bytes);

  disable_interrupts();

  // Enable speaker
  sb_write_DSP(DSP_DAC_SPEAKER_TURN_ON);

  // Read first bit of data
  read_buffer(0);
  read_buffer(1);

  if(play_state.read_remaining_bytes > 0) {
    debug_putstr("Sound: Auto init playback (%s) %u seconds "
      "samples=%d bytes=%d bytes/sample=%d channels=%d\n",
      playing_file.path, playing_file.length_seconds,
      play_state.remaining_samples, play_state.read_remaining_bytes,
      playing_file.bytes_per_sample, playing_file.channels);

    sb_auto_init_playback();
  } else {
    debug_putstr("Sound: Single cycle playback (%s) %u seconds\n,",
      playing_file.path, playing_file.length_seconds);

    sb_single_cycle_playback();
  }

  // Check if the sound is actually playing
  const uint8_t bits =
    playing_file.bits == 8 ? 0 :
    playing_file.bits == 16 ? 1 :
    0;
  const uint8_t status = inb(DMA_STATUS[bits]);
  if(status & 0xF0) {
    play_state.is_playing = TRUE;
    play_state.started_time_seconds = io_gettimer() / 1000;
  } else {
    debug_putstr("Sound: Couldn't initialize DMA\n");
    io_sound_stop();
  }
  enable_interrupts();
  return NO_ERROR;
}

// Initialize sound driver
void io_sound_init()
{
  device.enabled = FALSE;
  play_state.read_buffer_half = 0;
  play_state.is_playing = FALSE;

  // Check for Sound Blaster
  sb_find();
  if(!sb_found()) {
    debug_putstr("Sound: Sound Blaster not found\n");
    return;
  }

  // Get IRQ
  uint8_t IRQ = 0;
  const uint read_IRQ = sb_read_mixer(MIXER_READ_IRQ_PORT);
  switch(read_IRQ) {
    case SB_IRQ_2: IRQ = 2; break;
    case SB_IRQ_5: IRQ = 5; break;
    case SB_IRQ_7: IRQ = 7; break;
  };
  if(IRQ == 0) {
    debug_putstr("Sound: Failed to get Sound Blaster IRQ\n");
    return;
  }

  // Get 8bit DMA channel
  device.DMA8_channel = 10;
  const uint read_DMA = sb_read_mixer(MIXER_READ_DMA_PORT);
  if(read_DMA & SB_DMA_0) {
    device.DMA8_channel = 0;
  } else if(read_DMA & SB_DMA_1) {
    device.DMA8_channel = 1;
  } else if(read_DMA & SB_DMA_3) {
    device.DMA8_channel = 3;
  }
  // Get 16bit DMA channel
  device.DMA16_channel = 10;
  if(read_DMA & SB_DMA_5) {
    device.DMA16_channel = 5;
  } else if(read_DMA & SB_DMA_6) {
    device.DMA16_channel = 6;
  } else if(read_DMA & SB_DMA_7) {
    device.DMA16_channel = 7;
  }
  if(device.DMA8_channel > 3 || device.DMA16_channel > 7) {
    debug_putstr("Sound: Failed to get Sound Blaster DMA\n");
    return;
  }

  // Install IRQ handler
  set_sound_IRQ(IRQ);

  sb_write_DSP(DSP_DAC_SPEAKER_TURN_OFF);

  // Get DSP version
  sb_write_DSP(DSP_CMD_VERSION);
  const uint version_low = sb_read_DSP();
  const uint version_high = sb_read_DSP();
  debug_putstr("Sound: Sound Blaster found at %x "
    "(IRQ=%d DMA=%d,%d) DSP v%d.%d\n",
    device.base, IRQ, device.DMA8_channel, device.DMA16_channel,
    version_low, version_high);

  sb_write_mixer(MIXER_RESET_CMD, 0x00); // Reset mixer

  device.enabled = TRUE;
}

// Play wave file
uint io_sound_play(const char *wav_file_path)
{
  if(io_sound_is_enabled()) {
    io_sound_stop();
    return sb_play(wav_file_path);
  }
  return ERROR_NOT_AVAILABLE;
}

// Is sound enabled?
bool io_sound_is_enabled()
{
  return device.enabled;
}