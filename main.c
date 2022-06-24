#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lann.h>

int main(int argc, const char **argv) {
  const char *path = "repl.ln";
  
  for (int i = 1; i < argc; i++) {
    path = argv[i];
  }
  
  FILE *file = fopen(path, "r");
  if (!file) return 1;
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  
  char *code = calloc(size + 1, 1);
  
  rewind(file);
  fread(code, 1, size, file);
  
  fclose(file);
  
  ln_code = code;
  ln_code_offset = 0;
  
  ln_uint_t value = LN_INT_TO_VALUE(0);
  
  ln_eval(1);
  if (ln_back) value = ln_return;
  
  free(code);
  
  return LN_VALUE_TO_INT(value);
}
