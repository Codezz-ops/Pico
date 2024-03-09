#ifndef _APPEND_H_
#define _APPEND_H_

struct AppendBuffer {
  char *b;
  int len;
};

void abAppend(struct AppendBuffer *ab, const char *s, int len);
void abFree(struct AppendBuffer *ab);

#endif // _APPEND_H_