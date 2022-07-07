#ifdef __NOP__
#include <nop/term.h>
#else
#include <sys/ioctl.h>
#include <sys/stat.h>
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

void print_value(ln_uint_t value, int result) {
  int type = LN_VALUE_TYPE(value);
  
  if (type == ln_type_number) {
    if (LN_VALUE_SIGN(value)) putchar('-');
    print_fixed(LN_FIXED_ABS(LN_VALUE_TO_FIXED(value)), 1);
  } else if (type == ln_type_pointer) {
    if (result) {
      printf("\"%s\"", ln_data + LN_VALUE_TO_PTR(value));
    } else {
      putstr(ln_data + LN_VALUE_TO_PTR(value));
    }
  } else if (value == LN_NULL) {
    putstr("[null]");
  } else if (value == LN_UNDEFINED) {
    putstr("[undefined]");
  } else if (value == LN_DIVIDE_BY_ZERO) {
    putstr("[divide by zero]");
  } else if (value == LN_INVALID_TYPE) {
    putstr("[invalid type]");
  } else {
    printf("[error %u]", LN_VALUE_TO_ERR(value));
  }
}

static void printf_lann(void) {
  int index = 0;
  
  ln_uint_t value = ln_get_arg(index++);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return;
  
  const char *format = ln_data + LN_VALUE_TO_PTR(value);
  
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
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
}

static void put_text_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return;
  
  putstr(ln_data + LN_VALUE_TO_PTR(value));
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
}

static void put_char_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_number) return;
  
  putchar(LN_VALUE_TO_INT(value));
  
  #ifndef __NOP__
  fflush(stdout);
  #endif
}

static ln_uint_t get_text_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_pointer) return LN_INVALID_TYPE;
  
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

static void raw_mode_lann(void) {
  ln_uint_t value = ln_get_arg(0);
  if (LN_VALUE_TYPE(value) != ln_type_number) return;
  
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
}

static ln_uint_t get_term_lann(void) {
  ln_uint_t ptr_1 = ln_get_arg(0), ptr_2 = ln_get_arg(1);
  if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) return LN_INVALID_TYPE;
  
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

static void stats_lann(void) {
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
  
  printf("word: %u/%u words skipped (%d%%)\n",
    ln_words_saved,
    ln_words_total,
    (100 * ln_words_saved + ln_words_total / 2) / ln_words_total
  );
}

int repl_handle(ln_uint_t *value, ln_uint_t hash) {
  if (hash == ln_hash("printf")) {
    printf_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("put_text")) {
    put_text_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("put_char")) {
    put_char_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("get_text")) {
    *value = get_text_lann();
    return 1;
  } else if (hash == ln_hash("get_char")) {
    *value = get_char_lann();
    return 1;
  } else if (hash == ln_hash("raw_mode")) {
    raw_mode_lann();
    
    *value = LN_NULL;
    return 1;
  } else if (hash == ln_hash("get_term")) {
    *value = get_term_lann();
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
  } else if (hash == ln_hash("file_size")) {
    if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer) {
      *value = LN_INVALID_TYPE;
      return 1;
    }
    
    const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
    FILE *file = fopen(path, "r");
    
    if (!file) {
      *value = LN_NULL;
      return 1;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    
    fclose(file);
    
    *value = LN_INT_TO_VALUE(size);
    return 1;
  } else if (hash == ln_hash("file_load")) {
    if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(1)) != ln_type_pointer) {
      *value = LN_INVALID_TYPE;
      return 1;
    }
    
    const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
    FILE *file = fopen(path, "r");
    
    if (!file) {
      *value = LN_NULL;
      return 1;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    
    rewind(file);
    size_t total = fread(ln_data + LN_VALUE_TO_PTR(ln_get_arg(1)), 1, size, file);
    
    *value = LN_INT_TO_VALUE(total);
    return 1;
  } else if (hash == ln_hash("file_save")) {
    if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(1)) != ln_type_pointer || LN_VALUE_TYPE(ln_get_arg(2)) != ln_type_number) {
      *value = LN_INVALID_TYPE;
      return 1;
    }
    
    const char *path = ln_data + LN_VALUE_TO_PTR(ln_get_arg(0));
    FILE *file = fopen(path, "w");
    
    if (!file) {
      *value = LN_NULL;
      return 1;
    }
    
    size_t size = (size_t)(LN_VALUE_TO_INT(ln_get_arg(2)));
    size_t total = fwrite(ln_data + LN_VALUE_TO_PTR(ln_get_arg(1)), 1, size, file);
    
    *value = LN_INT_TO_VALUE(total);
    return 1;
  } else if (hash == ln_hash("file_delete")) {
    if (LN_VALUE_TYPE(ln_get_arg(0)) != ln_type_pointer) {
      *value = LN_INVALID_TYPE;
      return 1;
    }
    
    remove(ln_data + LN_VALUE_TO_PTR(ln_get_arg(0)));
    return 1;
  }
  
  return 0;
}
