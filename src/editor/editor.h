#ifndef _EDITOR_H_
#define _EDITOR_H_

#include "../append/append.h"
#include <termios.h>
#include <time.h>

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

enum EditorKey {
  BACKSPACE =  127,
  ARROW_LEFT =  1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE_KEY,
  HOME,
  END,
  PAGE_UP,
  PAGE_DOWN
};

enum editorHighlighting {
  HL_NORMAL = 0,
  HL_NUMBER,
  HL_COMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
};

struct editorSyntax {
  char *filetype, **filematch, *singleline_comment_start, **keywords;
  int flags;
};

struct EditorRow {
  int size, rsize;
  char *chars, *render;
  unsigned char *hl;
};

struct EditorConfig {
  int cx, cy, rx, rowoff, coloff, screenrows, screencols, numrows, dirty;
  struct EditorRow *row;
  char *filename, statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
  struct editorSyntax *syntax;
};

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);
int editorReadKey();
int editorRowCxToRx(struct EditorRow *row, int cx);
void editorUpdatEditorRow(struct EditorRow *row);
void editorDelRow(int at);
void editorRowInsertChar(struct EditorRow *row, int at, int c);
void editorInsertRow(int at, char *s, size_t len);
void editorRowAppendString(struct EditorRow *row, char *s, size_t len);
void editorRowDelChar(struct EditorRow *row, int at);
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();
char *editorRowsToString(int *buflen);
void editorOpen(char *filename);
void editorSave();
void editorScroll();
void editorDrawRows(struct AppendBuffer *ab);
void editorDrawStatusBar(struct AppendBuffer *ab);
void editorDrawMessageBar(struct AppendBuffer *ab);
void editorRefreshScreen();
void editorMoveCursor(int key);
void editorFreEditorRow(struct EditorRow *row);
void editorProcessKeypress();
void editorUpdateSyntax(struct EditorRow *row);
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int hl);
int is_separator(int c);

extern struct EditorConfig E;

#endif // _EDITOR_H_