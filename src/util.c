#include "util.h"

#include <ctype.h>
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
