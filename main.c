#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define TILES_WIDTH  100
#define TILES_HEIGHT 100

#define BLACK_BACKGROUND 40
#define GREEN_BACKGROUND 42
#define CYAN_BACKGROUND  46

static struct termios original;

typedef enum {
  EMPTY,
  GRASS,
  WATER,
} Tile;

static int tile_colors[] = {
  BLACK_BACKGROUND,
  GREEN_BACKGROUND,
  CYAN_BACKGROUND,
};

static int  player_position[2];
static Tile tiles[TILES_HEIGHT][TILES_WIDTH];

static char print_buffer[1 << 14];
static int  print_buffered;

static void flush() {
  if (print_buffered > 0) {
    write(STDOUT_FILENO, print_buffer, print_buffered);
    print_buffered = 0;
  }
}

static void print_char(char c) {
  if (print_buffered == sizeof(print_buffer)) {
    flush();
  }
  print_buffer[print_buffered] = c;
  print_buffered++;
}

static void print_string(char* s) {
  for (int i = 0; s[i] != 0; i++) {
    print_char(s[i]);
  }
}

static void print_int(int n) {
  char storage[32] = {};
  int  end         = 32;
  int  start       = 31;

  do {
    storage[start] = n % 10 + '0';
    start          = start - 1;
    n              = n / 10;
  } while (n > 0);

  while (start < end) {
    print_char(storage[start]);
    start++;
  }
}

static void clear_screen();

static void die(char* s) {
  clear_screen();
  perror(s);
  exit(EXIT_FAILURE);
}

static void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1) {
    die("tcsetattr");
  }
}

static void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &original) == -1) {
    die("tcgetattr");
  }
  atexit(disable_raw_mode);

  struct termios raw = original;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO  | ICANON | IEXTEN | ISIG);

  raw.c_cc[VMIN]  = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
      die("tcsetattr");
  }
}

static void cursor_to_top_left() {
  write(STDOUT_FILENO, "\x1b[H",  3);
}

static void clear_screen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
}

static void hide_cursor() {
  write(STDOUT_FILENO, "\x1b[?25l", 6);
}

static void show_cursor() {
  write(STDOUT_FILENO, "\x1b[?25h", 6);
}

static void get_window_size(int* rows, int* columns) {
  struct winsize size = {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
    die("ioctl");
  }
  *rows    = size.ws_row;
  *columns = size.ws_col;
}

static void build_map() {
  for (int i = 0; i < TILES_HEIGHT; i++) {
    for (int j = 0; j < TILES_WIDTH; j++) {
      if (fabs(i - j + sin((i + j) / 5) * 5) < 5) {
	tiles[i][j] = WATER;
      } else {
	tiles[i][j] = GRASS;
      }
    }
  }
}

static int read_key() {
  int     key        = 0;
  ssize_t bytes_read = 0;
  while (bytes_read != 1) {
    bytes_read = read(STDIN_FILENO, &key, 1);
    if (bytes_read == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return key;
}

static void handle_input() {
  int key = read_key();
  if (key == 'w') {
    player_position[1]++;
  } else if (key == 's') {
    player_position[1]--;
  } else if (key == 'a') {
    player_position[0]--;
  } else if (key == 'd') {
    player_position[0]++;
  } else if (key == 'q') {
    exit(EXIT_SUCCESS);
  }
}

static void render() {
  print_string("\x1b[2J");
  
  int rows    = 0;
  int columns = 0;
  get_window_size(&rows, &columns);

  cursor_to_top_left();

  int last_color = -1;

  for (int row = 0; row < rows; row++) {
    for (int column = 0; column < columns; column++) {
      int position[2] = {};
      position[0]     = column - columns / 2 + TILES_HEIGHT / 2 + player_position[0];
      position[1]     = row    - rows    / 2 + TILES_WIDTH  / 2 - player_position[1];

      char c = ' ';
      if (row == rows / 2 && column == columns / 2) {
	c = 'P';
      }
      
      int color = tile_colors[EMPTY];
      if (0 <= position[0] && position[0] < TILES_WIDTH) {
	if (0 <= position[1] && position[1] < TILES_HEIGHT) {
	  Tile tile = tiles[position[1]][position[0]];
	  color     = tile_colors[tile];
	}
      }

      if (color != last_color) {
	print_char(0x1B);
	print_char('[');
	print_int(color);
	print_char('m');
      }

      print_char(c);
    }
  }

  flush();
}

int main() {
  enable_raw_mode();
  
  hide_cursor();
  atexit(show_cursor);

  build_map();

  while (1) {
    render();
    handle_input();
  }
}
