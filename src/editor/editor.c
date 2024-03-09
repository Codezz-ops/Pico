#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include "editor.h"
#include "../errors/die.h"
#include "../append/append.h"
#include "../hl/hl.h"

#define VERSION "0.0.1"
#define TAB_STOP 4
#define ABUF_INIT {NULL, 0}
#define CTRL_KEY(k) ((k) & 0x1f)
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

struct EditorConfig E;

struct editorSyntax HLDB[] = {
  {
    "C/C++",
    C_HL_extensions,
    "//",
    C_HL_keywords,
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
  {
    "Assembler",
    ASM_HL_extensions,
    ";",
    ASM_HL_keywords,
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  }
};

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
   	if (nread == -1 && errno != EAGAIN) 
			die("read");
    }

  if (c == '\x1b') {
   	char seq[3];

  if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
  if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

  if (seq[0] == '[') {
    if (seq[1] >= '0' && seq[1] <= '9') {
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
      if (seq[2] == '~') {
        switch (seq[1]) {
          case '1': return HOME;
          case '3': return DELETE_KEY;
          case '4': return END;
          case '5': return PAGE_UP;
          case '6': return PAGE_DOWN;
          case '7': return HOME;
          case '8': return END;
        }
      }
    } else {
      switch (seq[1]) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
        case 'H': return HOME;
        case 'F': return END;
      }
    }
  } else if (seq[0] == 'O') {
    switch (seq[1]) {
      case 'H': return HOME;
      case 'F': return END;
    }
  }
  
  return '\x1b';
  } else {
    return c;
  }
}

int editorRowCxToRx(struct EditorRow *row, int cx) {
  int rx = 0;
  for (int i = 0; i < cx; i++) {
      if (row->chars[i] == '\t')
      rx += (TAB_STOP - 1) - (rx % TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdatEditorRow(struct EditorRow *row) {
  int tabs = 0, idx = 0;
  for (int i = 0; i < row->size; i++)
      if (row->chars[i] == '\t') tabs++;
  free(row->render);
  row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);
  for (int i = 0; i < row->size; i++) {
      if (row->chars[i] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
      } else {
          row->render[idx++] = row->chars[i];
      }
  }
  
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreEditorRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(struct EditorRow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(struct EditorRow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdatEditorRow(row);
  E.dirty++;
}

void editorRowAppendString(struct EditorRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdatEditorRow(row);
  E.dirty++;
}

void editorRowDelChar(struct EditorRow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdatEditorRow(row);
  E.dirty++;
}

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
    struct EditorRow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdatEditorRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;
  struct EditorRow *row = &E.row[E.cy];
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

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  for (int i = 0; i < E.numrows; i++)
    totlen += E.row[i].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (int i = 0; i < E.numrows; i++) {
    memcpy(p, E.row[i].chars, E.row[i].size);
    p += E.row[i].size;
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
  if (!fp) {
    fp = fopen(filename, "w");
    if (!fp)
      die("fopen");
  } else {
    fclose(fp);
    fp = fopen(filename, "r+");
    if (!fp)
      die("fopen");
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  
  E.row = realloc(E.row, sizeof(struct EditorRow) * (E.numrows + 1));
  
  memmove(&E.row[at + 1], &E.row[at], sizeof(struct EditorRow) * (E.numrows - at));
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;

  editorUpdatEditorRow(&E.row[at]);
  E.numrows++;
  E.dirty++;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)");
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

void editorDrawRows(struct AppendBuffer *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filEditorRow = y + E.rowoff;
    if (filEditorRow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "pico -- version %s", VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filEditorRow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      char *c = &E.row[filEditorRow].render[E.coloff];
      unsigned char *hl = &E.row[filEditorRow].hl[E.coloff];
      int current_color = 1;
      for (int i = 0; i < len; i++) {
        if (hl[i] == HL_NORMAL) {
          if (current_color != 1) {
           abAppend(ab, "\x1b[39m", 5);
           current_color = -1; 
          }
          abAppend(ab, &c[i], 1);
        } else {
          int color = editorSyntaxToColor(hl[i]);
          if (color != current_color) {
            current_color = color;
            char buffer[16];
            int clen = snprintf(buffer, sizeof(buffer), "\x1b[%dm", color);
            abAppend(ab, buffer, clen);
          }
          abAppend(ab, &c[i], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
      // abAppend(ab, &E.row[filEditorRow].render[E.coloff], len);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct AppendBuffer *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct AppendBuffer *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();

  struct AppendBuffer ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
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

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DELETE_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
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
  }
}

void editorMoveCursor(int key) {
  struct EditorRow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

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

void editorProcessKeypress() {
  static int quit_times = 3;

  int c = editorReadKey();

  switch (c) {
    case '\r':
      editorInsertNewline();
      break;

    case CTRL_KEY('x'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. "
          "Press Ctrl-X %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2i", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME:
      E.cx = 0;
      break;

    case END:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DELETE_KEY:
      if (c == DELETE_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }

        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }

  quit_times = quit_times;
}

void editorFreEditorRow(struct EditorRow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorUpdateSyntax(struct EditorRow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);
  if (E.syntax == NULL) return;
  char **keywords = E.syntax->keywords;
  char *scs = E.syntax->singleline_comment_start;
  int scs_len = scs ? strlen(scs) : 0;
  int prev_sep = 1;
  int in_string = 0;
  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
    if (scs_len && !in_string) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
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
        if (c == in_string) in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        } else if (c == '<') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
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
        if (kw2) klen--;
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
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;
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

int editorSyntaxToColor(int hl) {
  switch (hl) {
    case HL_NUMBER:
      return 33;
    case HL_STRING:
      return 92;
    case HL_COMMENT:
      return 234;
    case HL_KEYWORD1:
      return 35;
    case HL_KEYWORD2:
      return 36;
    default:
      return 37;
  }
}

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
