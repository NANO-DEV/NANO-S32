// Sound functionality

#ifndef _SOUND_H
#define _SOUND_H

// Initialize
void io_sound_init();

// True if sound is available
bool io_sound_is_enabled();

// Play wav file, non blocking
uint io_sound_play(const char *wav_file);

// Return true while a sound is playing
bool io_sound_is_playing();

// Stop playing sound
void io_sound_stop();

#endif // _SOUND_H