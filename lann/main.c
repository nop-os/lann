#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lann.h>

extern void add_funcs(void);
extern int  import_handle(ln_uint_t *value, const char *path);

int main(int argc, const char **argv) {
  ln_uint_t mem_size = 1048576;
  ln_init(malloc(mem_size), mem_size, import_handle);
  
  add_funcs();
  
  const char *path = "/usr/share/lann/repl.ln";
  if (strcmp(argv[0], "lann")) path = "repl.ln";
  
  if (argc >= 2) path = argv[1];
  
  int count = argc - 2;
  if (count < 0) count = 0;
  
  ln_uint_t args[count];
  count = 0;
  
  for (int i = 2; i < argc; i++) {
    args[count++] = ln_bump_offset;
    ln_bump_text(argv[i]);
  }
  
  ln_bump_args = ln_bump_offset;
  
  for (int i = 0; i < count; i++) {
    ln_bump_value(LN_PTR_TO_VALUE(args[i]));
  }
  
  ln_bump_value(LN_NULL);
  
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
