// User program: Play sound file

#include "types.h"
#include "ulib/ulib.h"

// Program entry point
int main(int argc, char *argv[])
{
  // Check args
  const bool play_background_mode =
    (argc == 3 && strcmp(argv[1],"back")==0);

  if(argc != 2 && !play_background_mode) {
    putstr("usage: %s [back] <wav_file>\n", argv[0]);
    return 0;
  }

  // Play
  const uint file_argv_index = argc - 1;
  char *file_name = argv[file_argv_index];
  const uint result = sound_play(file_name);
  if(result != NO_ERROR) {
    putstr("Error: Couldn't play file\n");
    return 1;
  }

  // Wait if not in background mode
  if(!play_background_mode) {
    putstr("Playing %s...", file_name);
    while(sound_is_playing()) {
    };
    sound_stop();

    putstr(" Done\n");
  }

  return 0;
}
