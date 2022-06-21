#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lann.h>

void print_value(ln_uint_t value, int result) {
  int type = LN_VALUE_TYPE(value);
  
  if (type == ln_type_number) {
    ln_uint_t fixed = (ln_uint_t)(LN_VALUE_TO_FIXED(LN_VALUE_ABS(value)));
    int decimals = 0;
    
    while (decimals < LN_FIXED_DOT && !(fixed % (2 << decimals))) {
      decimals++;
    }
    
    printf("%.*f", LN_VALUE_TO_FLOAT(value), LN_FIXED_DOT - decimals);
  } else if (type == ln_type_pointer) {
    if (result) {
      printf("\"%s\"", ln_bump + LN_VALUE_TO_PTR(value));
    } else {
      printf("%s", ln_bump + LN_VALUE_TO_PTR(value));
    }
  } else if (value == LN_NULL) {
    printf("[null]");
  } else if (value == LN_UNDEFINED) {
    printf("[undefined]");
  } else if (value == LN_DIVIDE_BY_ZERO) {
    printf("[divide by zero]");
  } else if (value == LN_INVALID_TYPE) {
    printf("[invalid type]");
  } else {
    printf("[error %u]", LN_VALUE_TO_ERR(value));
  }
}

static void printf_lann(void) {
  int index = 0;
  
  ln_uint_t value = ln_get_arg(index++);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return;
  
  const char *format = ln_bump + LN_VALUE_TO_PTR(value);
  
  while (*format) {
    if (*format == '[') {
      format++;
      
      if (*format == '[') putchar('[');
      else print_value(ln_get_arg(index++), 0);
    } else {
      putchar(*format);
    }

    format++;
  }
}

static void stats_lann(void) {
  printf("bump: %u/%u bytes used (%.2f%%)\n",
    ln_bump_offset,
    LN_BUMP_SIZE,
    (float)(100 * ln_bump_offset) / (float)(LN_BUMP_SIZE)
  );
  
  printf("vars: %u/%u bytes used (%.2f%%)\n",
    ln_context_offset * sizeof(ln_entry_t),
    LN_CONTEXT_SIZE * sizeof(ln_entry_t),
    (float)(100 * ln_context_offset) / (float)(LN_CONTEXT_SIZE)
  );
}

int ln_func_handle(ln_uint_t *value, ln_uint_t hash) {
  if (hash == ln_hash("printf")) {
    printf_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("stats")) {
    stats_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("error")) {
    if (ln_get_arg(0) == LN_NULL) {
      *value = LN_NULL;
    } else {
      *value = LN_ERR_TO_VALUE((ln_uint_t)(LN_VALUE_TO_INT(ln_get_arg(0))));
    }
    
    return 1;
  }
  
  return 0;
}

void repl(void) {
  char *code = NULL;
  size_t size = 0;
  
  printf("lann r01, by segfaultdev\n");
  
  for (;;) {
    printf("> ", ln_context_offset * sizeof(ln_entry_t), LN_CONTEXT_SIZE * sizeof(ln_entry_t));
    fflush(stdout);
    
    getline(&code, &size, stdin);
    
    ln_code = code;
    ln_code_offset = 0;
    
    ln_uint_t value = ln_eval(1);
    
    if (value != LN_NULL) {
      printf("= ");
      print_value(value, 1);
      
      printf("\n");
    }
  }
}

int main(int argc, const char **argv) {
  const char *path = NULL;
  
  for (int i = 1; i < argc; i++) {
    path = argv[i]; // TODO: arguments
  }
  
  if (path) {
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
    
    ln_eval(1);
    free(code);
  } else {
    repl();
  }
  
  return 0;
}
