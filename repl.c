#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lann.h>

static void print_fixed(ln_uint_t fixed, int first) {
  if (!first && !(fixed >> LN_FIXED_DOT)) return;
  
  int digit = (fixed >> LN_FIXED_DOT) % 10;
  if (fixed >> LN_FIXED_DOT) print_fixed(fixed / 10, 0);
  
  putchar('0' + digit);
  if (!first) return;
  
  fixed = (fixed % (1 << LN_FIXED_DOT)) * 10;
  
  if (!fixed) return;
  putchar('.');
  
  for (int i = 0; i < 7; i++) {
    digit = (fixed >> LN_FIXED_DOT) % 10;
    putchar('0' + digit);
    
    if (!(fixed % (1 << LN_FIXED_DOT))) break;
    fixed *= 10;
  }
}

static void print_value(ln_uint_t value, int result) {
  int type = LN_VALUE_TYPE(value);
  
  if (type == ln_type_number) {
    if (LN_VALUE_SIGN(value)) putchar('-');
    print_fixed(LN_VALUE_TO_FIXED(LN_VALUE_ABS(value)), 1);
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
  
  // fflush(stdout);
}

static ln_uint_t get_line_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return LN_NULL;
  
  fgets(ln_bump + LN_VALUE_TO_PTR(value), LN_BUMP_SIZE, stdin);
  size_t length = strlen(ln_bump + LN_VALUE_TO_PTR(value));
  
  while (ln_bump[LN_VALUE_TO_PTR(value) + (length - 1)] == '\n') {
    ln_bump[LN_VALUE_TO_PTR(value) + (length - 1)] = '\0';
    length--;
  }
  
  return LN_INT_TO_VALUE(length);
}

static void stats_lann(void) {
  printf("bump: %u/%u bytes used (%d%%)\n",
    ln_bump_offset,
    LN_BUMP_SIZE,
    (100 * ln_bump_offset + LN_BUMP_SIZE / 2) / LN_BUMP_SIZE
  );
  
  printf("vars: %u/%u bytes used (%d%%)\n",
    ln_context_offset * sizeof(ln_entry_t),
    LN_CONTEXT_SIZE * sizeof(ln_entry_t),
    (100 * ln_context_offset + LN_CONTEXT_SIZE / 2) / LN_CONTEXT_SIZE
  );

}

int ln_func_handle(ln_uint_t *value, ln_uint_t hash) {
  if (hash == ln_hash("printf")) {
    printf_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("get_line")) {
    *value = get_line_lann();
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
  } else if (hash == ln_hash("exit")) {
    exit(LN_VALUE_TO_INT(ln_get_arg(0)));
    return 1;
  }
  
  return 0;
}

void repl(void) {
  char code[256];
  ln_code = code;
  
  printf("lann r01, by segfaultdev\n");
  
  for (;;) {
    printf("> ", ln_context_offset * sizeof(ln_entry_t), LN_CONTEXT_SIZE * sizeof(ln_entry_t));
    // fflush(stdout);
    
    fgets(code, 256, stdin);
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
    
    ln_uint_t value = ln_eval(1);
    free(code);
    
    return LN_VALUE_TO_INT(value);
  } else {
    repl();
    return 0;
  }
}
