// User program: Text editor

#include "types.h"
#include "ulib/ulib.h"

// modes for next_line function. See function
#define SHOW_CURRENT 0
#define SKIP_CURRENT 1


// char attributes for title bar and editor area
// title: black text over light gray background
#define TITLE_ATTRIBUTES (AT_T_BLACK|AT_B_LGRAY)
// editor: white text over blue background
#define EDITOR_ATTRIBUTES (AT_T_WHITE|AT_B_BLUE)

// Screen size
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 28

// This buffer holds a copy of screen chars
// Screen is only updated when it's actually needed
static char *screen_buff = NULL;

// Check the local screen buffer before actually updating
// the screen
static void editor_putchar(uint col, uint row, char c)
{
  const uint screen_offset = col + (row-1)*SCREEN_WIDTH;
  const char buff_c = *(screen_buff + screen_offset);

  if(c != buff_c) {
    *(screen_buff + screen_offset) = c;
    putc_attr(col, row, c, EDITOR_ATTRIBUTES);
  }
}

// Given a string (line input parameter)
// returns a pointer to next line or to end of string
// if there are not more lines
//
// Initial line of input string can be:
// - shown at given row if mode==SHOW_CURRENT
// - just skipped if mode==SKIP_CURRENT
static char *next_line(uint mode, uint row, char *line)
{
  // Process string until next line or end of string
  // and show text if needed
  uint col = 0;
  while(line && *line && *line!='\n' && col<SCREEN_WIDTH) {
    if(mode == SHOW_CURRENT) {
      editor_putchar(col, row, *line);
    }
    col++;
    line++;
  }

  // Clear the rest of this line with spaces
  while(col<SCREEN_WIDTH && mode == SHOW_CURRENT) {
    editor_putchar(col++, row, ' ');
  }

  // Advance until next line begins
  if(line && *line == '\n') {
    line++;
  }

  return line;
}

// Shows buffer in editor area starting at a given
// number of line of buffer.
// It's recommended to hide cursor before calling this function
void show_buffer_at_line(char *buff, uint n)
{
  // Skip buffer until required line number
  uint l = 0;
  while(l<n && *buff) {
    buff = next_line(SKIP_CURRENT, 0, buff);
    l++;
  }

  // Show required buffer lines
  for(l=1; l<SCREEN_HEIGHT; l++) {
    if(*buff) {
      buff = next_line(SHOW_CURRENT, l, buff);
    } else {
      next_line(SHOW_CURRENT, l, 0);
    }
  }
}

// Convert linear buffer offset (uint offset)
// to line and col (output params)
void buffer_offset_to_linecol(char *buff, uint offset, uint *col, uint *line)
{
  *col = 0;
  *line = 0;

  for(; offset>0 && *buff; offset--, buff++) {

    if(*buff == '\n') {
      (*line)++;
      *col = 0;
    } else {
      (*col)++;
      if(*col >= SCREEN_WIDTH) {
        (*line)++;
        *col = 0;
      }
    }

  }
}

// Converts line and col (input params) to
// linear buffer offset (return value)
uint linecol_to_buffer_offset(char *buff, uint col, uint line)
{
  uint offset = 0;
  uint c = 0;
  while(*buff && line>0) {
    if(*buff == '\n' || c >= SCREEN_WIDTH) {
      line--;
      c=0;
    }
    c++;
    offset++;
    buff++;
  }

  while(*buff && col>0) {
    if(*buff == '\n') {
      break;
    }
    col--;
    offset++;
    buff++;
  }

  return offset;
}

// Convert linear buffer offset (uint offset)
// to file line
// Only function which is not screen space
uint buffer_offset_to_fileline(char *buff, uint offset)
{
  uint line = 0;
  for(; offset>0 && *buff; offset--, buff++) {
    if(*buff == '\n') {
      line++;
    }
  }

  return line;
}

// Program entry point
int main(int argc, char *argv[])
{
  // Chck usage
  if(argc != 2) {
    putstr("Usage: %s <file>\n\n", argv[0]);
    putstr("<file> can be:\n");
    putstr("-an existing file path: opens existing file to edit\n");
    putstr("-a new file path: opens empty editor. File is created on save\n");
    putstr("\n");
    return 1;
  }

  // Allocate fixed size text buffer
  char *buff = malloc(0xFFFF);
  if(!buff) {
    putstr("Error: can't allocate memory\n");
    return 1;
  }

  // Find file
  fs_entry_t entry;
  const uint n = get_entry(&entry, argv[1], UNKNOWN_VALUE, UNKNOWN_VALUE);

   // buff_size is the size in bytes actually used in buff
  size_t buff_size = 0;

  // Load file or show error
  if(n<ERROR_ANY && (entry.flags & FST_FILE)) {

    if(entry.size >= 0xFFFE) {
      mfree(buff);
      putstr("Can't read file %s (too big)\n", argv[1]);
      return 1;
    }

    memset(buff, 0, entry.size);
    buff_size = entry.size;
    uint result = read_file(buff, argv[1], 0, buff_size);
    debug_putstr("File read\n");

    if(result >= ERROR_ANY) {
      mfree(buff);
      putstr("Can't read file %s (error=%x)\n", argv[1], result);
      return 1;
    }

    if(result != entry.size) {
      mfree(buff);
      putstr("Can't read file (readed %u bytes, expected %u)\n",
        result, entry.size);
      return 1;
    }
    // Buffer must finish with a 0 and
    // must fit at least this 0, so buff_size can't be 0
    if(buff_size == 0 || *(buff + buff_size-1) != 0) {
      buff_size++;
    }
  } else if(n<ERROR_ANY && !(entry.flags & FST_FILE)) {
    mfree(buff);
    putstr("Invalid file\n");
    return 1;
  }

  // Create 1 byte buffer if this is a new file
  // This byte is for the final 0
  if(buff_size == 0) {
    buff_size = 1;
    memset(buff, 0, buff_size);
  }

  // Allocate screen buffer
  screen_buff = malloc((SCREEN_WIDTH*(SCREEN_HEIGHT-1)));
  if(screen_buff == 0) {
    putstr("Error: can't allocate memory\n");
    mfree(buff);
    return 1;
  }

  clear_screen();

  // Clear screen buffer
  memset(screen_buff, 0, (SCREEN_WIDTH*(SCREEN_HEIGHT-1)));

  // Write title
  const char *title_info = "L:     F1:Save ESC:Exit";
  uint title_char = 0;
  for(title_char=0; title_char<strlen(argv[1]); title_char++) {
    putc_attr(title_char, 0, argv[1][title_char], TITLE_ATTRIBUTES);
  }
  for(; title_char<SCREEN_WIDTH-strlen(title_info); title_char++) {
    putc_attr(title_char, 0, ' ', TITLE_ATTRIBUTES);
  }
  for(; title_char<SCREEN_WIDTH; title_char++) {
    putc_attr(title_char, 0,
      title_info[title_char+strlen(title_info)-SCREEN_WIDTH],
      TITLE_ATTRIBUTES);
  }

  // First line number to display in the editor area
  uint current_line = 0;

  // Show buffer and set cursor at start
  set_show_cursor(0);
  show_buffer_at_line(buff, current_line);
  set_cursor_pos(0, 1);
  set_show_cursor(1);

  // Var to get key presses
  uint k = 0;

  // buff_cursor_offset is the linear offset of current
  // cursor position inside buff
  uint buff_cursor_offset = 0;

  // Main loop
  while(k != KEY_ESC) {
    uint col=0, line=0;

    // Get key press
    k = getkey(GETKEY_WAITMODE_WAIT);

    // Process key actions

    // Keys to ignore
    if((k>KEY_F1 && k<=KEY_F10) ||
      k==KEY_F11 || k==KEY_F12 ||
      k==KEY_PRT_SC || k==KEY_INS ||
      k==0) {
      continue;

    // Key F1: Save
    } else if(k == KEY_F1) {
      const uint result = write_file(buff, argv[1], 0,
        buff_size, FWF_CREATE|FWF_TRUNCATE);

      // Update state indicator
      if(result < ERROR_ANY) {
        putc_attr(strlen(argv[1]), 0, ' ', TITLE_ATTRIBUTES);
      } else {
        putc_attr(strlen(argv[1]), 0, '*', (TITLE_ATTRIBUTES&0xF0)|AT_T_RED);
      }
      // This opperation takes some time
      // Keyboard buffer could be cleared here

    // Cursor keys: Move cursor
    } else if(k == KEY_UP) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      if(line > 0) {
        line -= 1;
        buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);
      }

    } else if(k == KEY_DOWN) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      line += 1;
      buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);

    } else if(k == KEY_LEFT) {
      if(buff_cursor_offset > 0) {
        buff_cursor_offset--;
      }

    } else if(k == KEY_RIGHT) {
      if(buff_cursor_offset < buff_size - 1) {
        buff_cursor_offset++;
      }

    // HOME, END, PG_UP and PG_DN keys
    } else if(k == KEY_HOME) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      col = 0;
      buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);

    } else if(k == KEY_END) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      col = 0xFFFF;
      buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);

    } else if(k == KEY_PG_DN) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      line += SCREEN_HEIGHT-1;
      buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);

    } else if(k == KEY_PG_UP) {
      buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
      line -= min(line, SCREEN_HEIGHT-1);
      buff_cursor_offset = linecol_to_buffer_offset(buff, col, line);


    // Backspace key: delete char before cursor and move cursor there
    } else if(k == KEY_BACKSPACE) {
      if(buff_cursor_offset > 0) {
        memcpy(buff+buff_cursor_offset-1, buff+buff_cursor_offset,
          buff_size-buff_cursor_offset);
        buff_size--;
        putc_attr(strlen(argv[1]), 0, '*', TITLE_ATTRIBUTES);
        buff_cursor_offset--;
      }

    // Del key: delete char at cursor
    } else if(k == KEY_DEL) {
      if(buff_cursor_offset < buff_size-1) {
        memcpy(buff+buff_cursor_offset, buff+buff_cursor_offset+1,
          buff_size-buff_cursor_offset-1);
        buff_size--;
        putc_attr(strlen(argv[1]), 0, '*', TITLE_ATTRIBUTES);
      }

    // Any other key but esc: insert char at cursor
    } else if(k != KEY_ESC && k != 0) {

      if(k == KEY_RETURN) {
        k = '\n';
      }
      if(k == KEY_TAB) {
        k = '\t';
      }
      memcpy(buff+buff_cursor_offset+1, buff+buff_cursor_offset,
        buff_size-buff_cursor_offset);
      *(buff + buff_cursor_offset++) = k;
      buff_size++;
      putc_attr(strlen(argv[1]), 0, '*', TITLE_ATTRIBUTES);
    }

    // Update cursor position and display
    buffer_offset_to_linecol(buff, buff_cursor_offset, &col, &line);
    if(line < current_line) {
      current_line = line;
    } else if(line > current_line + SCREEN_HEIGHT - 2) {
      current_line = line - SCREEN_HEIGHT + 2;
    }

    // Update line number in title
    // Compute bcd value (reversed)
    char line_ibcd[4]={0}; // To store line number digits
    uint ibcdt =
      min(9999, buffer_offset_to_fileline(buff, buff_cursor_offset)+1);
    for(uint i=0; i<4; i++) {
      line_ibcd[i] = ibcdt%10;
      ibcdt /= 10;
      if(ibcdt==0) {
        ibcdt = i;
        break;
      }
    }
    // Display it
    const uint start_pos = SCREEN_WIDTH-strlen(title_info)+2;
    for(uint i=0; i<4; i++) {
      char c = i<=ibcdt?line_ibcd[ibcdt-i]+'0':' ';
      putc_attr(start_pos+i, 0, c, TITLE_ATTRIBUTES);
    }

    set_show_cursor(0);
    show_buffer_at_line(buff, current_line);
    line -= current_line;
    line += 1;
    set_cursor_pos(col, line);
    set_show_cursor(1);
  }

  // Free screen buffer
  mfree(screen_buff);

  // Free buffer
  mfree(buff);

  // Reset screen
  clear_screen();
  set_cursor_pos(0, 0);

  return 0;
}
