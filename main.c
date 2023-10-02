/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/

#define TAU_VERSION "0.0.1"
#define TAU_TAB_STOP 8
#define TAU_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** data ***/

struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
};

struct editorConfig E;

// FUCK TON OF CODE HERE
#define CONFIG_FILE                                                            \
  "/home/mihai/Desktop/TauFinal/config.tau" // Change to desired config file

// Function to trim leading and trailing whitespace and quotes
char *trim_garbage(char *str) {
  int inside_quotes = 0; // Flag to keep track of quoted sections
  char *start = str;
  char *end = str + strlen(str) - 1;

  // Trim leading whitespace
  while (isspace((unsigned char)*start)) {
    start++;
  }

  // Trim trailing whitespace
  while (end > start && isspace((unsigned char)*end)) {
    end--;
  }

  // Trim leading and trailing quotes
  if (*start == '"' && *end == '"') {
    inside_quotes = 1; // Inside a quoted section
    start++;
    end--;
  } else if (*start == '\'' && *end == '\'') {
    inside_quotes = 1; // Inside a quoted section
    start++;
    end--;
  }

  // Preserve spaces within quotes
  char *current = start;
  while (current <= end) {
    if (*current == '"' || *current == '\'') {
      if (inside_quotes) {
        inside_quotes = 0; // Exiting a quoted section
      } else {
        inside_quotes = 1; // Entering a quoted section
      }
    } else if (inside_quotes && *current == ' ') {
      *current =
          '\a'; // Replace spaces within quotes with a non-printable character
    }
    current++;
  }

  // Shift non-printable characters back to spaces
  current = start;
  while (current <= end) {
    if (*current == '\a') {
      *current = ' ';
    }
    current++;
  }

  end[1] = '\0';
  return start;
}

// TODO(mihai): organize this further so you don't have the welcomeMsg here, but
// as a const

// This is shit, but it works.
//
//
// TODO(mihai): FIXME
char LOGO[21][41] = {{"      NNXXXXXXXXXXXXXXXXXXXXXXXKK0X  \n"},
                     {"    X0OkxdxxxxxddxxxxxxxxxxxxxddddkX  \n"},
                     {"  XOxdddddooddddddddddddddddddddx0N   \n"},
                     {"XOxddxxxxxxxdddddddxxdddddddxxk0N     \n"},
                     {"KkOKXNNNNNNXkddodOKXXXXXXXXXXN        \n"},
                     {"             Nkdddd0N                   \n"},
                     {"             Xkdddd0N                   \n"},
                     {"             Kxdddd0                    \n"},
                     {"             0ddddd0                    \n"},
                     {"            NOddddxK                    \n"},
                     {"            NkddddxK                    \n"},
                     {"            XkddddxK                    \n"},
                     {"            KxddddxK                    \n"},
                     {"            Kxddddd0N                   \n"},
                     {"            KxdddddOXN                  \n"},
                     {"            KxdddddxOK        NNXN      \n"},
                     {"            Xkdddddddx0XXXXK0OkxkK      \n"},
                     {"            N0ddddddodddddddddk0XN      \n"},
                     {"             NOdddddodddddxk0X          \n"},
                     {"              N0kxxxxxxkOKX             \n"},
                     {"                 NXXXNN                \n"}};

char WELCOME_PROMPT[256] = "Tau editor -- version %s";
size_t TAB_LENGTH = TAU_TAB_STOP;
size_t QUIT_CNT = TAU_QUIT_TIMES;
size_t LINE_FORMAT = 0;

int num_rows = 256;
int num_cols = 256;

void die(const char *s);

// Load and parse the config file
void load_config() {
  FILE *file = fopen(CONFIG_FILE, "r");
  if (file == NULL) {
    perror("(-) Error opening config file");
    return;
  }
  // Read the file line by line
  char line[256];
  while (fgets(line, sizeof(line), file)) {
    // Remove leading and trailing whitespaces
    char *trimmed_line = trim_garbage(line);

    // Check if the line is a valid config option
    if (trimmed_line[0] != '#' && trimmed_line[0] != '\0') {
      // Split the line into key and value
      char *key = strtok(trimmed_line, "=");
      char *value = strtok(NULL, "=");

      // Process the config option based on the key
      if (key && value) {

        char *trimmed_key = trim_garbage(key);

        // Handle shell prompt setting
        /*if (strcmp(trimmed_key, "logo") == 0) {
          char *trimmed_value = trim_garbage(value);
          FILE *file = fopen(trimmed_value, "rb");
          if (file == NULL)
            die("fopen");
          fseek(file, 0, SEEK_END);
          long size = ftell(file);
          rewind(file);
          char *data = (char *)malloc(size * sizeof(char));
          if (data == NULL) {
            die("Error allocating memory.\n");
          }
          fread(data, sizeof(char), size, file);

          // Copy each line of the file into the rows of LOGO
          int row = 0;
          int col = 0;
          for (int i = 0; i < size; i++) {
            if (data[i] == '\n') {
              // End of line, move to next row
              row++;
              col = 0;
            } else if (row < num_rows && col < num_cols) {
              // Copy character into matrix
              LOGO[row][col] = data[i];
              col++;
            }
          }

          //         strncpy(LOGO, data, 1024);

          fclose(file);
          // Now the file contents are stored in data[].
          free(data);
        }*/
        if (strcmp(trimmed_key, "prompt") == 0) {
          char *trimmed_value = trim_garbage(value);
          strncpy(WELCOME_PROMPT, trimmed_value, sizeof(WELCOME_PROMPT));
        }
        if (strcmp(trimmed_key, "tab-length") == 0) {
          char *trimmed_value = trim_garbage(value);
          TAB_LENGTH = (size_t)atoi(trimmed_value);
        }
        // TODO(mihai): BROKEN, WIP
        if (strcmp(trimmed_key, "quit-cnt") == 0) {
          char *trimmed_value = trim_garbage(value);
          QUIT_CNT = (size_t)atoi(trimmed_value);
        }
        if (strcmp(trimmed_key, "line-format") == 0) {
          char *trimmed_value = trim_garbage(value);
          LINE_FORMAT = (size_t)atoi(trimmed_value);
        }
      }
    }
  }
  fclose(file);
}
// FUCK TON OF CODE HERE

/*** filetypes ***/
/* TODO(mihai): no more hardcoding it, add config files*/

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {"switch",    "if",      "while",   "for",    "break",
                         "continue",  "return",  "else",    "struct", "union",
                         "typedef",   "static",  "enum",    "class",  "case",

                         "int|",      "long|",   "double|", "float|", "char|",
                         "unsigned|", "signed|", "void|",   NULL};
char *TAU_HL_extensions[] = {".tau", NULL};
char *TAU_HL_keywords[] = {"prompt", "line-format", "tab-length"};

struct editorSyntax HLDB[] = {
    {"c", C_HL_extensions, C_HL_keywords, "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
    {"tau", TAU_HL_extensions, TAU_HL_keywords, "#", "##", "###",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS}};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** syntax highlighting ***/

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->singleline_comment_start;
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mce_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }
    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;

        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
  switch (hl) {
  case HL_COMMENT:
  case HL_MLCOMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
  case HL_MATCH:
    return 34;
  default:
    return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL)
    return;

  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;

        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }

        return;
      }
      i++;
    }
  }
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TAB_LENGTH - 1) - (rx % TAB_LENGTH);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (TAB_LENGTH - 1) - (cur_rx % TAB_LENGTH);
    cur_rx++;

    if (cur_rx > rx)
      return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (TAB_LENGTH - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TAB_LENGTH != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows)
    return;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++)
    E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++)
    E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == 0 && E.cy == 0)
    return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/*** file i/o ***/

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }

  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find ***/

void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;
  int current = last_match;
  int i;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;

    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query =
      editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/*** output ***/

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawHelpBar(struct abuf *ab);

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen =
            snprintf(welcome, sizeof(welcome), WELCOME_PROMPT, TAU_VERSION);

        // FIXME(mihai)
        /*
                for (int i = 0; i < 21; i++) {
                  int logolen = strlen(LOGO[i]);
                  if (logolen > E.screencols)
                    logolen = E.screencols;
                  abAppend(ab, LOGO[i], logolen);
                  abAppend(ab, "\n", 1);
                }*/

        // END OF FIXME
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;

        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);

        // TODO
        //      editorDrawHelpBar(ab);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_color = -1;
      int j;
      for (j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  // abAppend(ab, "\x1b[7m", 4);
  abAppend(ab, "\x1b[48;5;4m", 9); // set background color to blue
  char status[80], rstatus[80];

  // TODO(mihai): REMOVE OLD CODE
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No Name]", E.numrows,
                     E.dirty ? "(modified) " : "");
  int rlen =
      snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
               E.syntax ? E.syntax->filetype : " no ft", E.cy + 1, E.numrows);
  // TODO(mihai): Better way when not tired and fix the 150% overflow "bug".
  int percent =
      (E.numrows > 1) ? (int)(((float)E.cy / (E.numrows - 1)) * 100) : 0;
  if (LINE_FORMAT == 1) {
    rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d%%",
                    E.syntax ? E.syntax->filetype : " no ft", percent);
  }

  if (len > E.screencols)
    len = E.screencols;

  abAppend(ab, status, len);

  // abAppend(ab, "\ue0bc", 3); // Add unicode triangle

  // TODO(mihai): Research this further

  // abAppend(ab, "\x1b[38;5;8m", 9); // set foreground color to gray
  // abAppend(ab, " \x1b[38;5;8m", 9); // set foreground color to gray
  // abAppend(ab, " \ue0ba", 3);       // add char
  abAppend(ab, "\x1b[48;5;8m", 9); // set background color to gray

  // abAppend(ab, "\x1b[47m", 5); // Set bg to gray

  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, "\x1b[48;5;4m", 9); // set bg to blue
      abAppend(ab, rstatus, rlen);
      abAppend(ab, "\x1b[48;5;8m", 9); // set bg back to gray
      break;
    } else {
      abAppend(ab, " ",
               1); // fill status with spaces so it reaches end of screen
      len++;
    }
  }

  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

// FIXME TODO(MIHAI): HACKY
/*void editorDrawHelpBar(struct abuf *ab) {
  // Set the background color to gray
  abAppend(ab, "\x1b[48;5;235m", 12);

  // Clear the line and move the cursor to the beginning
  abAppend(ab, "\x1b[K", 3);

  // Set the text color to a suitable color for the help message
  abAppend(ab, "\x1b[38;5;15m", 12);

  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);

  // Reset colors and attributes to default
  abAppend(ab, "\x1b[0m", 4);
  abAppend(ab, "\n", 1);
}
void editorDrawHelpMenu(struct abuf *ab) {
  int numRows = 3;
  int numCols = 3;
  int optionWidth = E.screencols / numCols;
  int helpTextSize = 6; // Total number of help options

  // Calculate the starting row and column for the help menu above the status
  // bar
  int startRow = E.screenrows - numRows - 1;
  int startCol = 0;

  // Define an array of ANSI color codes for the options
  char *optionColors[] = {
      "\x1b[31m", // Red
      "\x1b[32m", // Green
      "\x1b[33m", // Yellow
      "\x1b[34m", // Blue
      "\x1b[35m", // Magenta
      "\x1b[36m"  // Cyan
  };

  // Define the help text for each option
  char *helpText[] = {"Option 1 Help", "Option 2 Help", "Option 3 Help",
                      "Option 4 Help", "Option 5 Help", "Option 6 Help"};

  // Calculate the width of each option
  int optionLength = E.screencols / helpTextSize;

  // Set the background color to gray for the entire help menu
  abAppend(ab, "\x1b[48;5;235m", 9);

  // Loop through the rows and options to draw the help menu
  for (int row = 0; row < numRows; row++) {
    int y = startRow + row;
    for (int optionIndex = row; optionIndex < helpTextSize;
         optionIndex += numRows) {
      char *option = helpText[optionIndex];

      // Calculate the starting column for this option
      int x = startCol + (optionIndex - row) * optionLength;

      // Set the cursor position
      char buf[32];
      snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
      abAppend(ab, buf, strlen(buf));

      // Set the color for the option
      abAppend(ab, optionColors[optionIndex % 6],
               strlen(optionColors[optionIndex % 6]));

      // Append the option text
      abAppend(ab, option, strlen(option));
    }

    // Reset colors and attributes to default
    abAppend(ab, "\x1b[0m", 4);

    abAppend(ab, "\n", 1);
    // Fill the remaining space with spaces
    int remainingSpace =
        E.screencols - (startCol + optionLength * helpTextSize);
    if (remainingSpace > 0) {
      for (int i = 0; i < remainingSpace; i++) {
        abAppend(ab, " ", 1);
      }
    }

  }
}*/

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  // editorDrawHelpMenu(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  // HACKY TODO(mihai)

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** TODO(mihai): Non-hacky way ***/
void editorSetHelpMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback)
          callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback)
      callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows) {
      E.cy++;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void initEditor();

void editorProcessKeypress() {
  static int quit_times = TAU_QUIT_TIMES;

  int c = editorReadKey();

  switch (c) {
  case '\r':
    editorInsertNewline();
    break;

  case CTRL_KEY('q'):
    if (E.dirty && quit_times > 0) {
      editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                             "Press Ctrl-Q %d more times to quit.",
                             quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    E.cx = 0;
    break;

  case END_KEY:
    if (E.cy < E.numrows)
      E.cx = E.row[E.cy].size;
    break;

  case CTRL_KEY('f'):
    editorFind();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY)
      editorMoveCursor(ARROW_RIGHT);
    editorDelChar();
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      E.cy = E.rowoff;
    } else if (c == PAGE_DOWN) {
      E.cy = E.rowoff + E.screenrows - 1;
      if (E.cy > E.numrows)
        E.cy = E.numrows;
    }

    int times = E.screenrows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

    // TODO(mihai): It works, but move it to a leader (space) + h or something
    // config
  case CTRL_KEY('p'):
    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = "
                           "find | Ctrl-E = open file | Ctrl-P = display help");
    break;
  // TODO(mihai): Add file opening, this shit is broken atm.
  case CTRL_KEY('e'): {
    char *file_path;
    file_path = editorPrompt("Open file %s", NULL);
    if (file_path != NULL)
      // TODO(mihai): Add else statement with file checking so the program
      // doesn't crash due to fopen.
      // BUG! Clear screen first!
      // dumb idea below
      initEditor();
    editorOpen(file_path);

    break;
  }

  default:
    editorInsertChar(c);
    break;
  }

  quit_times = TAU_QUIT_TIMES;
}

/*** init ***/

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
  E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
  // WIP
  // Load config files, if any
  load_config();

  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  // TODO(mihai): Add else statement with file checking so the program doesn't
  // crash due to fopen.

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find "
                         "| Ctrl-E = open file | Ctrl-P = show help");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
