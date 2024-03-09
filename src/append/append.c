#include <stdlib.h>
#include <string.h>
#include "append.h"

void abAppend(struct AppendBuffer *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct AppendBuffer *ab) {
  free(ab->b);
}