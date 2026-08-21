#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

char* str_trim(char* s) {
  while (isspace((unsigned char)*s)) {
    s++;
  }

  if (*s == '\0') {
    return s;
  }

  char* end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    end--;
  }
  end[1] = '\0';

  return s;
}

int split_fields(const char* line, char delimiter,
                  char fields[][MAX_FIELD_LEN]) {
  int count = 0;
  const char* p = line;

  while (count < MAX_FIELDS) {
    while (*p == ' ' || *p == '\t') {
      p++;
    }

    bool quoted = (*p == '"');
    if (quoted) {
      p++;
    }

    char* out = fields[count];
    int out_len = 0;

    while (*p != '\0') {
      if (quoted) {
        if (*p == '"' && *(p + 1) == '"') {
          if (out_len < MAX_FIELD_LEN - 1) out[out_len++] = '"';
          p += 2;
          continue;
        }
        if (*p == '"') {
          p++;
          quoted = false;  // closing quote found; field ends at next delimiter
          continue;
        }
      } else if (*p == delimiter) {
        break;
      }

      if (out_len < MAX_FIELD_LEN - 1) out[out_len++] = *p;
      p++;
    }
    out[out_len] = '\0';
    count++;

    if (*p != delimiter) {
      break;
    }
    p++;  // skip the delimiter and parse the next field
  }

  return count;
}
