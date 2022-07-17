#ifdef __NOP__
#include <nop/term.h>
#else
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#define putstr(str) printf("%s", str)
#endif

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
  
  fixed = (fixed % ((ln_uint_t)(1) << LN_FIXED_DOT)) * 10;
  
  if (!fixed) return;
  putchar('.');
  
  for (int i = 0; i < 7; i++) {
    digit = (fixed >> LN_FIXED_DOT) % 10;
    putchar('0' + digit);
    
    if (!(fixed % ((ln_uint_t)(1) << LN_FIXED_DOT))) break;
    fixed *= 10;
  }
}

static void print_value(ln_uint_t value) {
  int type = LN_VALUE_TYPE(value);
  
  if (type == ln_type_number) {
    if (LN_VALUE_SIGN(value)) putchar('-');
    print_fixed(LN_FIXED_ABS(LN_VALUE_TO_FIXED(value)), 1);
  } else if (type == ln_type_pointer) {
    if (ln_check(LN_VALUE_TO_PTR(value), 1)) putstr(ln_data + LN_VALUE_TO_PTR(value));
    else printf("[pointer %u]", LN_VALUE_TO_PTR(value));
  } else if (value == LN_NULL) {
    putstr("[null]");
  } else if (value == LN_UNDEFINED) {
    putstr("[undefined]");
  } else if (value == LN_DIVIDE_BY_ZERO) {
    putstr("[divide by zero]");
  } else if (value == LN_INVALID_TYPE) {
    putstr("[invalid type]");
  } else if (value == LN_OUT_OF_BOUNDS) {
    putstr("[out of bounds]");
  } else {
    printf("[error %u]", LN_VALUE_TO_ERR(value));
  }
}

static ln_uint_t printf_lann(void) {
  int index = 0;
  
  ln_uint_t value = ln_get_arg(index++);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return LN_INVALID_TYPE;
  
  const char *format = ln_data + LN_VALUE_TO_PTR(value);
  
  while (*format) {
    if (!ln_check((ln_uint_t)((void *)(format) - (void *)(ln_data)), 1)) return LN_OUT_OF_BOUNDS;
    
    if (*format == '[') {
      format++;
      
      if (!ln_check((ln_uint_t)((void *)(format) - (void *)(ln_data)), 1)) return LN_OUT_OF_BOUNDS;
      
      if (*format == '[') putchar('[');
      else print_value(ln_get_arg(index++));
    } else {
      putchar(*format);
    }

    format++;
  }
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
  
  return LN_NULL;
}

static ln_uint_t put_text_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return LN_INVALID_TYPE;
  
  if (!ln_check(LN_VALUE_TO_PTR(value), 1)) return LN_OUT_OF_BOUNDS;
  putstr(ln_data + LN_VALUE_TO_PTR(value));
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
  
  return LN_NULL;
}

static ln_uint_t put_char_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_number) return LN_INVALID_TYPE;
  
  putchar(LN_VALUE_TO_INT(value));
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
  
  return LN_NULL;
}

static ln_uint_t get_text_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return LN_INVALID_TYPE;
  
  if (!ln_check(LN_VALUE_TO_PTR(value), 1)) return LN_OUT_OF_BOUNDS;
  
  #ifdef __NOP__
  gets_s(ln_data + LN_VALUE_TO_PTR(value), ln_bump_size);
  #else
  fgets(ln_data + LN_VALUE_TO_PTR(value), ln_bump_size, stdin);
  #endif
  
  size_t length = strlen(ln_data + LN_VALUE_TO_PTR(value));
  
  while (ln_data[LN_VALUE_TO_PTR(value) + (length - 1)] == '\n') {
    ln_data[LN_VALUE_TO_PTR(value) + (length - 1)] = '\0';
    length--;
  }
  
  return LN_INT_TO_VALUE(length);
}

static ln_uint_t get_char_lann(void) {
  char chr;
  
  #ifdef __NOP__
  if (!term_read(&chr, 1)) chr = '\0';
  #else
  if (read(STDIN_FILENO, &chr, 1) <= 0) chr = '\0';
  #endif
  
  return LN_INT_TO_VALUE(chr);
}

static ln_uint_t raw_mode_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_number) return LN_INVALID_TYPE;
  
  #ifdef __NOP__
  static int old_mode;
  
  if (LN_VALUE_TO_FIXED(value) != 0) {
    old_mode = term_getmode();
    term_setmode(0);
  } else {
    term_setmode(old_mode);
  }
  #else
  static struct termios old_termios;
  
  if (LN_VALUE_TO_FIXED(value) != 0) {
    tcgetattr(STDIN_FILENO, &old_termios);
    struct termios new_termios = old_termios;
    
    new_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    new_termios.c_oflag &= ~(OPOST);
    new_termios.c_cflag |= (CS8);
    new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
  } else {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
  }
  #endif
  
  return LN_NULL;
}

static ln_uint_t get_term_lann(void) {
  ln_uint_t ptr_1 = ln_get_arg(0), ptr_2 = ln_get_arg(1);
  if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) return LN_INVALID_TYPE;
  
  if (!ln_check(LN_VALUE_TO_PTR(ptr_1), sizeof(ln_uint_t))) return LN_OUT_OF_BOUNDS;
  if (!ln_check(LN_VALUE_TO_PTR(ptr_2), sizeof(ln_uint_t))) return LN_OUT_OF_BOUNDS;
  
  #ifdef __NOP__
  // TODO
  *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr_1))) = LN_INT_TO_VALUE(80);
  *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr_2))) = LN_INT_TO_VALUE(25);
  #else
  struct winsize ws;
  
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return LN_INT_TO_VALUE(0);
  }
  
  *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr_1))) = LN_INT_TO_VALUE(ws.ws_col);
  *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr_2))) = LN_INT_TO_VALUE(ws.ws_row);
  #endif
  
  return LN_INT_TO_VALUE(1);
}

static ln_uint_t get_time_lann(void) {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  
  ln_uint_t fixed_time = ((tv.tv_usec << LN_FIXED_DOT) / 1000000) + (tv.tv_sec << LN_FIXED_DOT);
  return LN_FIXED_TO_VALUE(fixed_time);
}

static ln_uint_t stats_lann(void) {
  printf("bump: %u/%u bytes used (%d%%)\n",
    ln_bump_offset,
    ln_bump_size,
    (100 * ln_bump_offset + ln_bump_size / 2) / ln_bump_size
  );
  
  printf("heap: %u/%u bytes used (%d%%)\n",
    ln_heap_used,
    ln_heap_size,
    (100 * ln_heap_used + ln_heap_size / 2) / ln_heap_size
  );
  
  printf("vars: %u/%u bytes used (%d%%)\n",
    ln_context_offset * sizeof(ln_entry_t),
    ln_context_size,
    (100 * ln_context_offset * sizeof(ln_entry_t) + ln_context_size / 2) / ln_context_size
  );
  
  printf("func: %u/%u bytes used (%d%%)\n",
    ln_func_offset * sizeof(ln_func_t),
    ln_func_size,
    (100 * ln_func_offset * sizeof(ln_func_t) + ln_func_size / 2) / ln_func_size
  );
  
  printf("word: %u/%u words skipped (%d%%)\n",
    ln_words_saved,
    ln_words_total,
    (100 * ln_words_saved + ln_words_total / 2) / ln_words_total
  );
  
  return LN_NULL;
}

static ln_uint_t exit_lann(void) {
  if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_number) {
    return LN_INVALID_TYPE;
  }
  
  exit(LN_VALUE_TO_INT(ln_get_arg(0)));
  return LN_NULL;
}

static ln_uint_t file_size_lann(void) {
  if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer) {
    return LN_INVALID_TYPE;
  }
  
  const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
  FILE *file = fopen(path, "r");
  
  if (!file) {
    return LN_NULL;
  }
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  
  fclose(file);
  return LN_INT_TO_VALUE(size);
}

static ln_uint_t file_load_lann(void) {
  if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(1)) != ln_type_pointer) {
    return LN_INVALID_TYPE;
  }
  
  const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
  FILE *file = fopen(path, "r");
  
  if (!file) {
    return LN_NULL;
  }
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  
  rewind(file);
  
  if (!ln_check(LN_VALUE_TO_PTR(ln_get_arg(1)), size)) {
    fclose(file);
    return LN_OUT_OF_BOUNDS;
  }
  
  size_t total = fread(ln_data + LN_VALUE_TO_PTR(ln_get_arg(1)), 1, size, file);
  fclose(file);
  
  return LN_INT_TO_VALUE(total);
}

static ln_uint_t file_save_lann(void) {
  if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(1)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(2)) != ln_type_number) {
    return LN_INVALID_TYPE;
  }
  
  const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
  FILE *file = fopen(path, "w");
  
  if (!file) {
    return LN_NULL;
  }
  
  size_t size = (size_t)(LN_VALUE_TO_INT(ln_get_arg(2)));
  
  if (!ln_check(LN_VALUE_TO_PTR(ln_get_arg(1)), size)) {
    fclose(file);
    return LN_OUT_OF_BOUNDS;
  }
  
  size_t total = fwrite(ln_data + LN_VALUE_TO_PTR(ln_get_arg(1)), 1, size, file);
  fclose(file);
  
  return LN_INT_TO_VALUE(total);
}

static ln_uint_t file_delete_lann(void) {
  if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer) {
    return LN_INVALID_TYPE;
  }
  
  if (!ln_check(LN_VALUE_TO_PTR(ln_get_arg(0)), 1)) {
    return LN_OUT_OF_BOUNDS;
  }
  
  remove(ln_data + LN_VALUE_TO_PTR(ln_get_arg(0)));
  return LN_NULL;
}

void add_funcs(void) {
  ln_func_add("printf", printf_lann);
  ln_func_add("put_text", put_text_lann);
  ln_func_add("put_char", put_char_lann);
  ln_func_add("get_text", get_text_lann);
  ln_func_add("get_char", get_char_lann);
  ln_func_add("raw_mode", raw_mode_lann);
  ln_func_add("get_term", get_term_lann);
  ln_func_add("get_time", get_time_lann);
  ln_func_add("stats", stats_lann);
  ln_func_add("exit", exit_lann);
  ln_func_add("file_size", file_size_lann);
  ln_func_add("file_load", file_load_lann);
  ln_func_add("file_save", file_save_lann);
  ln_func_add("file_delete", file_delete_lann);
}

int import_handle(ln_uint_t *value, const char *path) {
  FILE *file = fopen(path, "r");
  if (!file) return 0;
  
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  
  char *code = calloc(size + 1, 1);
  
  rewind(file);
  fread(code, 1, size, file);
  
  fclose(file);
  
  ln_code = code;
  ln_code_offset = 0;
  
  ln_last_curr = 0;
  ln_last_next = 0;
  
  *value = ln_eval(1);
  free(code);
  
  return 1;
}
