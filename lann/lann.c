#ifdef __USE_TYPE_H__
#include <type.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include <string.h>
#include <lann.h>

int  (*ln_import_handle)(ln_uint_t *, const char *) = NULL;
void (*ln_syntax_handle)(void) = NULL;

uint8_t *ln_data = NULL;
ln_uint_t ln_bump_size = 0, ln_bump_offset = 0, ln_bump_args = 0;

ln_uint_t ln_heap_size = 0, ln_heap_used = 0;
int ln_heap_inited = 0;

ln_entry_t *ln_context = NULL;
ln_uint_t ln_context_size = 0, ln_context_offset = 0;

ln_func_t *ln_funcs = NULL;
ln_uint_t ln_func_size = 0, ln_func_offset = 0;

const char *ln_code = NULL;
ln_uint_t ln_code_offset = 0;

ln_word_t ln_last;
ln_uint_t ln_last_curr = 0, ln_last_next = 0;

int ln_back = 0, ln_break = 0;
ln_uint_t ln_return = LN_NULL;

ln_uint_t ln_words_total = 0, ln_words_saved = 0;

const int ln_type_match[4] = {ln_type_number, ln_type_number, ln_type_pointer, ln_type_error};

static inline char ln_upper(char chr) {
  if (chr >= 'a' && chr <= 'z') return (chr - 'a') + 'A';
  return chr;
}

static inline ln_uint_t ln_syntax_error(int exec) {
  if (exec) {
    if (ln_syntax_handle) ln_syntax_handle();
    
    ln_return = LN_SYNTAX_ERROR;
    ln_back = 1;
  }
  
  return LN_SYNTAX_ERROR;
}

int ln_check_heap(ln_uint_t offset) {
  if (offset < ln_bump_size || offset >= ln_bump_size + ln_heap_size) {
    return 0;
  }
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + ln_bump_size);
  
  while ((uint8_t *)(block) < ln_data + ln_bump_size + ln_heap_size) {
    ln_heap_t *next_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
    
    if (block == (ln_heap_t *)((ln_data + offset) - sizeof(ln_heap_t))) {
      if (block->free) return 0; // ohohoh, a use-after-free...
      return 1;
    }
    
    block = next_block;
  }
  
  return 0; // probably not aligned to any block
}

int ln_check(ln_uint_t offset, ln_uint_t size) {
  if (offset + size <= ln_bump_size) {
    if (offset + size > ln_bump_offset) return 0; // don't access unused bump memory!
  } else if (offset + size <= ln_bump_size + ln_heap_size) {
    ln_heap_t *block = (ln_heap_t *)(ln_data + ln_bump_size);
    
    while ((uint8_t *)(block) < ln_data + ln_bump_size + ln_heap_size) {
      ln_heap_t *next_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
      
      ln_uint_t start = ((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t);
      
      if (offset >= start && offset + size <= start + block->size) {
        if (block->free) return 0; // ohohoh, a use-after-free...
        return 1;
      }
      
      block = next_block;
    }
    
    return 0; // this should only happen when accessing headers, a really bad idea indeed...
  } else {
    return 0; // always out of bounds
  }
  
  return 1;
}

int ln_check_string(ln_uint_t offset) {
  for (;;) {
    if (!ln_check(offset, 1)) return 0;
    if (!ln_data[offset]) break;
    
    offset++;
  }
  
  return 1;
}

uint32_t ln_hash(const char *text) {
  uint32_t hash = 0x811C9DC5;
  
  while (*text) {
    hash ^= *(text++);
    hash *= 0x01000193;
  }
  
  return hash;
}

ln_int_t ln_fixed(const char *text) {
  ln_int_t value = 0;
  
  int negate = 0;
  int base = 10;
  
  if (*text == '-') negate = 1, text++;
  if (*text == '0') text++;
  
  if (*text == 'b') base = 2, text++;
  else if (*text == 'o') base = 8, text++;
  else if (*text == 'x') base = 16, text++;
  
  const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *ptr;
  
  while ((ptr = strchr(digits, ln_upper(*text)))) {
    if (!(*ptr)) break;
    
    value = (value * base) + (ptr - digits);
    text++;
  }
  
  value *= (ln_int_t)((ln_uint_t)(1) << LN_FIXED_DOT);
  
  if (*text != '.') {
    if (negate) value = -value;
    return value;
  }
  
  text++;
  int factor = base;
  
  while ((ptr = strchr(digits, ln_upper(*text)))) {
    if (!(*ptr)) break;
    
    value += ((ln_int_t)((ln_uint_t)(1) << LN_FIXED_DOT) * (ptr - digits)) / factor;
    text++;
    
    factor *= base;
  }
  
  if (negate) value = -value;
  return value;
}

void ln_init(void *buffer, ln_uint_t size, int (*import_handle)(ln_uint_t *, const char *), void (*syntax_handle)(void)) {
  ln_bump_size =    (6  * size) / 32;
  ln_context_size = (2  * size) / 32;
  ln_heap_size =    (23 * size) / 32;
  ln_func_size =    (1  * size) / 32;
  
  if (ln_func_size < sizeof(ln_func_t) * 16) {
    ln_func_size = sizeof(ln_func_t) * 16;
    size -= ln_func_size;
    
    ln_bump_size =    (2 * size) / 8;
    ln_context_size = (1 * size) / 8;
    ln_heap_size =    (5 * size) / 8;
  }
  
  ln_context = buffer;
  ln_funcs = buffer + ln_context_size;
  ln_data = buffer + ln_context_size + ln_func_size;
  
  ln_import_handle = import_handle;
  ln_syntax_handle = syntax_handle;
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + ln_bump_size);
  
  block->size = ln_heap_size - sizeof(ln_heap_t);
  block->free = 1;
  
  ln_heap_used = sizeof(ln_heap_t);
  
  ln_bump_offset = 0;
  ln_bump_args = 0;
  
  ln_context_offset = 0;
  ln_func_offset = 0;
  
  ln_last_curr = 0;
  ln_last_next = 0;
  
  ln_back = 0;
  ln_break = 0;
  
  ln_words_total = 0;
  ln_words_saved = 0;
}

void ln_func_add(const char *name, ln_uint_t (*func)(void)) {
  if ((ln_func_offset + 1) * sizeof(ln_func_t) > ln_func_size) return;
  
  ln_funcs[ln_func_offset++] = (ln_func_t){
    .hash = ln_hash(name),
    .name = name,
    .func = func
  };
}

int ln_func_call(ln_uint_t *value, ln_uint_t hash) {
  for (ln_uint_t i = 0; i < ln_func_offset; i++) {
    if (ln_funcs[i].hash == hash) {
      *value = ln_funcs[i].func();
      return 1;
    }
  }
  
  return 0;
}

void ln_heap_defrag(void) {
  ln_heap_t *block = (ln_heap_t *)(ln_data + ln_bump_size);
  
  while ((uint8_t *)(block) < ln_data + ln_bump_size + ln_heap_size) {
    ln_heap_t *next_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
    
    if (block->free && (uint8_t *)(next_block) < ln_data + ln_bump_size + ln_heap_size) {
      if (next_block->free) {
        block->size += next_block->size + sizeof(ln_heap_t);
        ln_heap_used -= sizeof(ln_heap_t);
        
        continue;
      }
    }
    
    block = next_block;
  }
}

ln_uint_t ln_heap_alloc(ln_uint_t size) {
  ln_heap_t *block = (ln_heap_t *)(ln_data + ln_bump_size);
  
  while ((uint8_t *)(block) < ln_data + ln_bump_size + ln_heap_size) {
    if (block->free) {
      if (block->size == size) {
        block->free = 0;
        
        ln_heap_used += size;
        return LN_PTR_TO_VALUE((ln_uint_t)(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t)));
      } else if (block->size >= size + sizeof(ln_heap_t)) {
        ln_heap_t *new_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + size);
        ln_uint_t old_size = block->size;
        
        block->size = size;
        block->free = 0;
        
        new_block->size = old_size - (block->size + sizeof(ln_heap_t));
        new_block->free = 1;
        
        ln_heap_used += size + sizeof(ln_heap_t);
        return LN_PTR_TO_VALUE((ln_uint_t)(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t)));
      }
    }
    
    block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
  }
  
  ln_heap_defrag();
  block = (ln_heap_t *)(ln_data + ln_bump_size);
  
  while ((uint8_t *)(block) < ln_data + ln_bump_size + ln_heap_size) {
    if (block->free) {
      if (block->size == size) {
        block->free = 0;
        
        ln_heap_used += size;
        return LN_PTR_TO_VALUE((ln_uint_t)(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t)));
      } else if (block->size >= size + sizeof(ln_heap_t)) {
        ln_heap_t *new_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + size);
        ln_uint_t old_size = block->size;
        
        block->size = size;
        block->free = 0;
        
        new_block->size = old_size - (block->size + sizeof(ln_heap_t));
        new_block->free = 1;
        
        ln_heap_used += size + sizeof(ln_heap_t);
        return LN_PTR_TO_VALUE((ln_uint_t)(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t)));
      }
    }
    
    block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
  }
  
  return LN_NULL;
}

ln_uint_t ln_heap_realloc(ln_uint_t ptr, ln_uint_t new_size) {
  if (ptr == LN_NULL) return ln_heap_alloc(new_size);
  
  ln_uint_t new_ptr = ln_heap_alloc(new_size);
  if (new_ptr == LN_NULL) return LN_NULL;
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + (LN_VALUE_TO_PTR(ptr) - sizeof(ln_heap_t)));
  memcpy(ln_data + LN_VALUE_TO_PTR(new_ptr), ln_data + LN_VALUE_TO_PTR(ptr), block->size < new_size ? block->size : new_size);
  
  ln_heap_free(ptr);
  return new_ptr;
}

void ln_heap_free(ln_uint_t ptr) {
  if (ptr == LN_NULL) return;
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + (LN_VALUE_TO_PTR(ptr) - sizeof(ln_heap_t)));
  block->free = 1;
  
  ln_heap_used -= block->size;
}

ln_uint_t ln_bump_value(ln_uint_t value) {
  memcpy(ln_data + ln_bump_offset, &value, sizeof(ln_uint_t));
  
  ln_uint_t offset = ln_bump_offset;
  ln_bump_offset += sizeof(ln_uint_t);
  
  return offset;
}

ln_uint_t ln_bump_text(const char *text) {
  ln_uint_t length = strlen(text);
  
  memcpy(ln_data + ln_bump_offset, text, length + 1);
  
  ln_uint_t offset = ln_bump_offset;
  ln_bump_offset += length + 1;
  
  return offset;
}

void ln_bump_text_fixed(ln_int_t fixed, int first) {
  if (fixed < 0) {
    ln_data[ln_bump_offset++] = '-';
    fixed = -fixed;
  }
  
  if (!first && !(fixed >> LN_FIXED_DOT)) return;
  
  int digit = (fixed >> LN_FIXED_DOT) % 10;
  if (fixed >> LN_FIXED_DOT) ln_bump_text_fixed(fixed / 10, 0);
  
  ln_data[ln_bump_offset++] = '0' + digit;
  if (!first) return;
  
  fixed = (fixed % ((ln_uint_t)(1) << LN_FIXED_DOT)) * 10;
  
  if (!fixed) return;
  ln_data[ln_bump_offset++] = '.';
  
  for (int i = 0; i < 7; i++) {
    digit = (fixed >> LN_FIXED_DOT) % 10;
    ln_data[ln_bump_offset++] = '0' + digit;
    
    if (!(fixed % ((ln_uint_t)(1) << LN_FIXED_DOT))) break;
    fixed *= 10;
  }
}

void ln_bump_text_int(ln_int_t value, int first) {
  if (value < 0) {
    ln_data[ln_bump_offset++] = '-';
    value = -value;
  }
  
  if (!first && !value) return;
  
  int digit = value % 10;
  if (value) ln_bump_text_int(value / 10, 0);
  
  ln_data[ln_bump_offset++] = '0' + digit;
}

ln_uint_t ln_get_arg(int index) {
  return ((ln_uint_t *)(ln_data + ln_bump_args))[index];
}

ln_uint_t ln_cast(ln_uint_t value, int type) {
  int old_type = LN_VALUE_TYPE(value);
  if (old_type == type) return value;
  
  if ((unsigned int)(type) > ln_type_error) return LN_INVALID_TYPE;
  
  if (old_type == ln_type_number) {
    if (type == ln_type_pointer) return LN_PTR_TO_VALUE((ln_uint_t)(LN_VALUE_TO_INT(value)));
    else if (type == ln_type_error) return LN_ERR_TO_VALUE((ln_uint_t)(LN_VALUE_TO_INT(value)));
  } else if (old_type == ln_type_pointer) {
    if (type == ln_type_number) return LN_INT_TO_VALUE(LN_VALUE_TO_PTR(value));
    else if (type == ln_type_error) return LN_ERR_TO_VALUE(LN_VALUE_TO_PTR(value));
  } else if (old_type == ln_type_error) {
    if (type == ln_type_number) {
      ln_uint_t error = LN_VALUE_TO_ERR(value);
      
      if (error >> (8 * sizeof(error) - 3)) {
        error |= ((ln_uint_t)(3) << (8 * sizeof(error) - 2));
      }
      
      return LN_INT_TO_VALUE((ln_int_t)(error));
    } else if (type == ln_type_pointer) {
      return LN_PTR_TO_VALUE(LN_VALUE_TO_ERR(value));
    }
  }
  
  return LN_NULL;
}

static inline int ln_space(char chr) {
  return (chr && chr <= 32);
}

char ln_keep(int in_string) {
  char chr = ln_code[ln_code_offset];
  if (!chr) return LN_CHAR_EOF;
  
  if (!in_string) {
    int is_delim = 0;
    
    for (;;) {
      if (chr == '#') {
        while (ln_code[ln_code_offset] && ln_code[ln_code_offset] != '\n') ln_code_offset++;
      } else if (ln_space(chr)) {
        ln_code_offset++;
      } else {
        break;
      }
      
      chr = ln_code[ln_code_offset];
      is_delim = 1;
    }
    
    if (is_delim) return LN_CHAR_DELIM;
  }
  
  return chr;
}

void ln_skip(char chr, int in_string) {
  if ((chr != LN_CHAR_DELIM || in_string) && chr != LN_CHAR_EOF) ln_code_offset++;
}

char ln_read(int in_string) {
  char chr = ln_keep(in_string);
  ln_skip(chr, in_string);
  
  return chr;
}

ln_word_t ln_take(void) {
  ln_words_total++;
  
  if (ln_last_curr < ln_last_next && ln_last_curr == ln_code_offset) {
    ln_last_curr = ln_last_next;
    
    if (ln_last.type != ln_word_string) {
      ln_code_offset = ln_last_next;
      ln_words_saved++;
      
      return ln_last;
    }
  }
  
  ln_uint_t offset = ln_bump_offset;
  const char *token = (const char *)(ln_data + offset);
  
  int in_string = 0;
  char string_chr = '\0';
  
  for (;;) {
    char chr = ln_read(in_string);
    if (chr == LN_CHAR_EOF) break;
    
    if (!in_string && chr == LN_CHAR_DELIM) {
      if (ln_bump_offset - offset) {
        break;
      } else {
        continue;
      }
    }
    
    if (in_string && chr == '\\') {
      chr = ln_read(in_string);
      
      if (chr == 'r') {
        ln_data[ln_bump_offset++] = '\r';
      } else if (chr == 'n') {
        ln_data[ln_bump_offset++] = '\n';
      } else if (chr == 't') {
        ln_data[ln_bump_offset++] = '\t';
      } else if (chr == 'x') {
        const char *digits = "0123456789ABCDEF";
        uint8_t value = 0;
        
        for (int i = 0; i < 2; i++) {
          chr = ln_keep(in_string);
          const char *ptr;
          
          if (!(ptr = strchr(digits, chr))) break;
          if (chr == LN_CHAR_EOF) break;
          
          value = (value * 16) + (ptr - digits);
          ln_skip(chr, in_string);
        }
        
        ln_data[ln_bump_offset++] = (char)(value);
      } else if (chr == '0') {
        const char *digits = "01234567";
        uint8_t value = 0;
        
        for (int i = 0; i < 3; i++) {
          chr = ln_keep(in_string);
          const char *ptr;
          
          if (!(ptr = strchr(digits, chr))) break;
          
          value = (value * 8) + (ptr - digits);
          ln_skip(chr, in_string);
        }
        
        ln_data[ln_bump_offset++] = (char)(value);
      } else {
        ln_data[ln_bump_offset++] = chr;
      }
      
      continue;
    }
    
    if (chr == '"' || chr == '\'') {
      if (in_string) {
        if (chr == string_chr) break;
      } else {
        string_chr = chr;
        in_string = 1;
        
        continue;
      }
    }
    
    if (!in_string && (chr <= 45 || chr == '/' || chr == '<' || chr == '=' || chr == '>' || chr == '^')) {
      if (ln_bump_offset - offset) {
        ln_code_offset--;
      } else {
        ln_data[ln_bump_offset++] = chr;
      }
      
      break;
    }
    
    ln_data[ln_bump_offset++] = chr;
  }
  
  ln_data[ln_bump_offset++] = '\0';
  
  ln_last_curr = ln_code_offset;
  ln_last_next = ln_code_offset;
  
  if (in_string && string_chr == '"') {
    return (ln_word_t){.type = ln_word_string, .data = offset};
  }
  
  ln_bump_offset = offset;
  uint32_t hash = ln_hash(token);
  
  if (hash == 0x77074BA4) {
    return (ln_word_t){.type = ln_word_null};
  } else if (hash == 0x4DB211E5) {
    return (ln_word_t){.type = ln_word_true};
  } else if (hash == 0x0B069958) {
    return (ln_word_t){.type = ln_word_false};
  } else if (hash == 0xEB0CBD62) {
    return (ln_word_t){.type = ln_word_block};
  } else if (hash == 0x68348A7E) {
    return (ln_word_t){.type = ln_word_begin};
  } else if (hash == 0x6A8E75AA) {
    return (ln_word_t){.type = ln_word_end};
  } else if (hash == 0x0E7E4807) {
    return (ln_word_t){.type = ln_word_func};
  } else if (hash == 0x506B03FA) {
    return (ln_word_t){.type = ln_word_let};
  } else if (hash == 0x39386E06) {
    return (ln_word_t){.type = ln_word_if};
  } else if (hash == 0xBDBF5BF0) {
    return (ln_word_t){.type = ln_word_else};
  } else if (hash == 0x8A58AD26) {
    return (ln_word_t){.type = ln_word_array};
  } else if (hash == 0x0DC628CE) {
    return (ln_word_t){.type = ln_word_while};
  } else if (hash == 0x994C2D1C) {
    return (ln_word_t){.type = ln_word_give};
  } else if (hash == 0xC9648178) {
    return (ln_word_t){.type = ln_word_break};
  } else if (hash == 0x42F48402) {
    return (ln_word_t){.type = ln_word_ref};
  } else if (hash == 0x9D0AA73C) {
    return (ln_word_t){.type = ln_word_args};
  } else if (hash == 0x1BD670A0) {
    return (ln_word_t){.type = ln_word_type_number};
  } else if (hash == 0xA35F792A) {
    return (ln_word_t){.type = ln_word_type_pointer};
  } else if (hash == 0x21918751) {
    return (ln_word_t){.type = ln_word_type_error};
  } else if (hash == 0x7A278DE7) {
    return (ln_word_t){.type = ln_word_func_type_of};
  } else if (hash == 0x8AD5F6CC) {
    return (ln_word_t){.type = ln_word_func_size_of};
  } else if (hash == 0xFEA791E9) {
    return (ln_word_t){.type = ln_word_func_to_type};
  } else if (hash == 0x540CA757) {
    return (ln_word_t){.type = ln_word_func_get};
  } else if (hash == 0xC6270703) {
    return (ln_word_t){.type = ln_word_func_set};
  } else if (hash == 0xABB47DC7) {
    return (ln_word_t){.type = ln_word_func_mem_read};
  } else if (hash == 0x3BDB7886) {
    return (ln_word_t){.type = ln_word_func_mem_write};
  } else if (hash == 0x65D1799A) {
    return (ln_word_t){.type = ln_word_func_mem_copy};
  } else if (hash == 0x24EB1CFA) {
    return (ln_word_t){.type = ln_word_func_mem_move};
  } else if (hash == 0x925347E7) {
    return (ln_word_t){.type = ln_word_func_mem_test};
  } else if (hash == 0x8490AE78) {
    return (ln_word_t){.type = ln_word_func_mem_alloc};
  } else if (hash == 0x894BE1D3) {
    return (ln_word_t){.type = ln_word_func_mem_realloc};
  } else if (hash == 0xB80435D9) {
    return (ln_word_t){.type = ln_word_func_mem_free};
  } else if (hash == 0xAD17E81C) {
    return (ln_word_t){.type = ln_word_func_str_copy};
  } else if (hash == 0xD7A9BDBD) {
    return (ln_word_t){.type = ln_word_func_str_test};
  } else if (hash == 0x9720B814) {
    return (ln_word_t){.type = ln_word_func_str_size};
  } else if (hash == 0x974B66AA) {
    return (ln_word_t){.type = ln_word_func_str_format};
  } else if (hash == 0xB7895734) {
    return (ln_word_t){.type = ln_word_func_str_parse};
  } else if (hash == 0x35B8DF69) {
    return (ln_word_t){.type = ln_word_func_str_hash};
  } else if (hash == 0x08D22E0F) {
    return (ln_word_t){.type = ln_word_func_eval};
  } else if (hash == 0x112A90D4) {
    return (ln_word_t){.type = ln_word_func_import};
  } else if (hash == 0x0F29C2A6) {
    return (ln_word_t){.type = ln_word_bool_and};
  } else if (hash == 0x5D342984) {
    return (ln_word_t){.type = ln_word_bool_or};
  } else if (hash == 0xCC6BDB7E) {
    return (ln_word_t){.type = ln_word_bool_xor};
  }
  
  if (token[0] == '\0') {
    return (ln_word_t){.type = ln_word_eof};
  } else if (token[0] == ',') {
    return (ln_word_t){.type = ln_word_comma};
  } else if (token[0] == '(') {
    return (ln_word_t){.type = ln_word_paren_left};
  } else if (token[0] == ')') {
    return (ln_word_t){.type = ln_word_paren_right};
  } else if (token[0] == '+') {
    return (ln_word_t){.type = ln_word_plus};
  } else if (token[0] == '-') {
    return (ln_word_t){.type = ln_word_minus};
  } else if (token[0] == '*') {
    return (ln_word_t){.type = ln_word_aster};
  } else if (token[0] == '/') {
    return (ln_word_t){.type = ln_word_slash};
  } else if (token[0] == '%') {
    return (ln_word_t){.type = ln_word_percent};
  } else if (token[0] == '!') {
    return (ln_word_t){.type = ln_word_exclam};
  } else if (token[0] == '=') {
    return (ln_word_t){.type = ln_word_equal};
  } else if (token[0] == '>') {
    return (ln_word_t){.type = ln_word_great};
  } else if (token[0] == '<') {
    return (ln_word_t){.type = ln_word_less};
  } else if (token[0] == '&') {
    return (ln_word_t){.type = ln_word_bit_and};
  } else if (token[0] == '|') {
    return (ln_word_t){.type = ln_word_bit_or};
  } else if (token[0] == '^') {
    return (ln_word_t){.type = ln_word_bit_xor};
  }
  
  if (token[0] >= '0' && token[0] <= '9') {
    return (ln_word_t){.type = ln_word_number, .data = ln_fixed(token)};
  } else if (in_string && string_chr == '\'') {
    return (ln_word_t){.type = ln_word_number, .data = ((ln_uint_t)(token[0]) << LN_FIXED_DOT)};
  } else {
    return (ln_word_t){.type = ln_word_name, .hash = hash};
  }
}

ln_word_t ln_peek(void) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word = ln_take();
  
  ln_last = word;
  
  ln_last_curr = offset;
  ln_last_next = ln_code_offset;
  
  ln_code_offset = offset;
  return word;
}

int ln_expect(ln_word_t *ptr, uint8_t type) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word = ln_take();
  
  if (type != word.type) {
    ln_last = word;
    
    ln_last_curr = offset;
    ln_last_next = ln_code_offset;
    
    ln_code_offset = offset;
    return 0;
  }
  
  if (ptr) *ptr = word;
  return 1;
}

int ln_expect_multi(ln_word_t *ptr, const int *types) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word = ln_take();
  
  while (*types) {
    if (*types == word.type) {
      if (ptr) *ptr = word;
      return 1;
    }
    
    types++;
  }
  
  ln_last = word;
  
  ln_last_curr = offset;
  ln_last_next = ln_code_offset;
  
  ln_code_offset = offset;
  return 0;
}

int ln_expect_2(uint8_t type_1, uint8_t type_2) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word_1 = ln_take();
  
  if (type_1 != word_1.type) {
    ln_last = word_1;
    
    ln_last_curr = offset;
    ln_last_next = ln_code_offset;
    
    ln_code_offset = offset;
    return 0;
  }
  
  ln_uint_t mid_offset = ln_code_offset;
  ln_word_t word_2 = ln_take();
  
  if (type_2 != word_2.type) {
    ln_last = word_1;
    
    ln_last_curr = offset;
    ln_last_next = mid_offset;
    
    ln_code_offset = offset;
    return 0;
  }
  
  return 1;
}

ln_int_t ln_muliply(ln_int_t x, ln_int_t y) {
  const int bits = sizeof(ln_uint_t) << 3;
  int sign = 0;
  
  if (x < 0) sign = 1 - sign, x = -x;
  if (y < 0) sign = 1 - sign, y = -y;
  
  ln_uint_t a = x >> (bits >> 1);
  ln_uint_t b = x & (((ln_uint_t)(1) << (bits >> 1)) - 1);
  ln_uint_t c = y >> (bits >> 1);
  ln_uint_t d = y & (((ln_uint_t)(1) << (bits >> 1)) - 1);
  
  ln_int_t z = (ln_int_t)(((a * c) << (bits - LN_FIXED_DOT)) + (((a * d) + (b * c)) << ((bits >> 1) - LN_FIXED_DOT)) + ((b * d) >> LN_FIXED_DOT));
  if (sign) z = -z;
  
  return z;
}

ln_int_t ln_divide(ln_int_t x, ln_int_t y) {
  
}

ln_uint_t ln_eval_0(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_word_t word = ln_take();
  
  if (word.type == ln_word_number) {
    return word.data;
  } else if (word.type == ln_word_string) {
    if (exec) return LN_PTR_TO_VALUE(word.data);
    
    ln_bump_offset = word.data;
    return LN_NULL;
  } else if (word.type == ln_word_name) {
    if (ln_expect(NULL, ln_word_paren_left)) {
      ln_uint_t context_offset = ln_context_offset;
      ln_uint_t bump_offset = ln_bump_offset;
      ln_uint_t code_offset = ln_code_offset;
      
      int count = 0;
      
      if (!ln_expect(NULL, ln_word_paren_right)) {
        for (;;) {
          ln_eval_expr(0);
          count++;
          
          if (ln_expect(NULL, ln_word_paren_right)) {
            break;
          } else if (!ln_expect(NULL, ln_word_comma)) {
            return ln_syntax_error(exec);
          }
        }
      }
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      if (!exec) return LN_NULL;
      
      ln_uint_t args[count];
      count = 0;
      
      ln_code_offset = code_offset;
      
      if (!ln_expect(NULL, ln_word_paren_right)) {
        for (;;) {
          args[count++] = ln_eval_expr(1);
          
          if (ln_expect(NULL, ln_word_paren_right)) {
            break;
          } else if (!ln_expect(NULL, ln_word_comma)) {
            return ln_syntax_error(exec);
          }
        }
      }
      
      ln_context_offset = context_offset;
      
      ln_uint_t bump_args = ln_bump_args;
      ln_bump_args = ln_bump_offset;
      
      for (int i = 0; i < count; i++) {
        ln_bump_value(args[i]);
      }
      
      ln_bump_value(LN_NULL);
      ln_uint_t value = LN_UNDEFINED;
      
      if (!ln_func_call(&value, word.hash)) {
        for (int i = ln_context_offset - 1; i >= 0; i--) {
          if (ln_context[i].name == word.hash) {
            ln_uint_t offset = LN_VALUE_TO_PTR(*((ln_uint_t *)(ln_data + ln_context[i].offset)));
            
            if (!ln_check_string(offset)) {
              value = LN_OUT_OF_BOUNDS;
              break;
            }
            
            const char *old_code = ln_code;
            ln_uint_t old_offset = ln_code_offset;
            
            ln_word_t old_last = ln_last;
            ln_uint_t old_last_curr = ln_last_curr;
            ln_uint_t old_last_next = ln_last_next;
            
            ln_code = (const char *)(ln_data + offset);
            ln_code_offset = 0;
            
            ln_last_curr = 0;
            ln_last_next = 0;
            
            value = ln_eval_stat(1);
            if (ln_back) value = ln_return;
            
            ln_back = 0;
            
            ln_code = old_code;
            ln_code_offset = old_offset;
            
            ln_last = old_last;
            ln_last_curr = old_last_curr;
            ln_last_next = old_last_next;
            
            break;
          }
        }
      }
      
      ln_bump_offset = bump_offset;
      ln_bump_args = bump_args;
      
      return value;
    } else {
      if (exec) {
        for (int i = ln_context_offset - 1; i >= 0; i--) {
          if (ln_context[i].name == word.hash) {
            return *((ln_uint_t *)(ln_data + ln_context[i].offset));
          }
        }
        
        return LN_UNDEFINED;
      } else {
        return LN_NULL;
      }
    }
  } else if (word.type == ln_word_paren_left) {
    ln_uint_t value = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    return value;
  } else if (word.type == ln_word_block) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_uint_t value = ln_eval_expr(exec);
    
    if (exec) {
      if (ln_back) value = ln_return;
      ln_back = 0;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return value;
  } else if (word.type == ln_word_begin) {
    return ln_eval(exec);
  } else if (word.type == ln_word_let) {
    if (!ln_expect(&word, ln_word_name)) return ln_syntax_error(exec);
    ln_uint_t value = ln_eval_expr(exec);
    
    if (exec) {
      int found = 0;
      
      for (int i = ln_context_offset - 1; i >= 0; i--) {
        if (ln_context[i].name == word.hash) {
          *((ln_uint_t *)(ln_data + ln_context[i].offset)) = value;
          found = 1;
          
          break;
        }
      }
      
      if (!found) {
        ln_context[ln_context_offset++] = (ln_entry_t){
          .name = word.hash,
          .offset = ln_bump_value(value)
        };
      }
    }
    
    return value;
  } else if (word.type == ln_word_func) {
    if (!ln_expect(&word, ln_word_name)) return ln_syntax_error(exec);
    
    ln_uint_t code_start = ln_code_offset;
    while (ln_space(ln_code[code_start])) code_start++;
    
    ln_eval_stat(0);
    
    ln_uint_t code_end = ln_code_offset;
    while (code_end && ln_space(ln_code[code_end - 1])) code_end--;
    
    ln_uint_t offset = ln_bump_offset;
    
    if (exec) {
      for (ln_uint_t i = code_start; i < code_end; i++) {
        ln_data[ln_bump_offset++] = ln_code[i];
      }
      
      ln_data[ln_bump_offset++] = '\0';
      
      ln_context[ln_context_offset++] = (ln_entry_t){
        .name = word.hash,
        .offset = ln_bump_value(LN_PTR_TO_VALUE(offset))
      };
    }
    
    return LN_PTR_TO_VALUE(offset);
  } else if (word.type == ln_word_array) {
    if (!ln_expect(&word, ln_word_name)) return ln_syntax_error(exec);
    
    ln_uint_t size = ln_eval_expr(exec);
    size = LN_VALUE_TO_INT(LN_VALUE_ABS(size));
    
    if (exec) {
      ln_uint_t offset = ln_bump_offset;
      ln_bump_offset += size;
      
      ln_context[ln_context_offset++] = (ln_entry_t){
        .name = word.data,
        .offset = ln_bump_value(LN_PTR_TO_VALUE(offset))
      };
      
      return LN_PTR_TO_VALUE(offset);
    }
    
    return LN_NULL;
  } else if (word.type == ln_word_null) {
    return LN_NULL;
  } else if (word.type == ln_word_if) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_uint_t value = ln_eval_expr(exec);
    int cond = LN_VALUE_TO_FIXED(value);
    
    if (ln_context_offset == context_offset) ln_bump_offset = bump_offset;
    
    if (exec) {
      if (LN_VALUE_TYPE(value) == ln_type_pointer) cond = ln_data[LN_VALUE_TO_PTR(value)];
      if (LN_VALUE_TYPE(value) == ln_type_error) cond = 0;
    }
    
    value = LN_NULL;
    
    if (cond && exec) value = ln_eval_expr(1);
    else ln_eval_expr(0);
    
    if (ln_expect(NULL, ln_word_else)) {
      if (!cond && exec) value = ln_eval_expr(1);
      else ln_eval_expr(0);
    }
    
    return value;
  } else if (word.type == ln_word_while) {
    ln_uint_t code_offset = ln_code_offset;
    ln_uint_t value = LN_NULL;
    
    if (!exec) {
      ln_eval_expr(0);
      ln_eval_expr(0);
      
      return LN_NULL;
    }
    
    for (;;) {
      ln_uint_t context_offset = ln_context_offset;
      ln_uint_t bump_offset = ln_bump_offset;
      
      value = ln_eval_expr(exec);
      int cond = LN_VALUE_TO_FIXED(value);
      
      if (ln_context_offset == context_offset) ln_bump_offset = bump_offset;
      
      if (LN_VALUE_TYPE(value) == ln_type_pointer) cond = 1;
      if (LN_VALUE_TYPE(value) == ln_type_error) cond = 0;
      
      value = LN_NULL;
      
      if (cond && exec) {
        value = ln_eval_expr(1);
        if (ln_back || ln_break) break;
      } else {
        ln_eval_expr(0);
        break;
      }
      
      ln_code_offset = code_offset;
      
      ln_last_curr = 0;
      ln_last_next = 0;
    }
    
    if (exec) ln_break = 0;
    return value;
  } else if (word.type == ln_word_args) {
    return LN_PTR_TO_VALUE(ln_bump_args);
  } else if (word.type == ln_word_type_number) {
    return LN_INT_TO_VALUE(ln_type_number);
  } else if (word.type == ln_word_type_pointer) {
    return LN_INT_TO_VALUE(ln_type_pointer);
  } else if (word.type == ln_word_type_error) {
    return LN_INT_TO_VALUE(ln_type_error);
  } else if (word.type == ln_word_func_type_of) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    ln_uint_t value = ln_eval_expr(exec);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    return LN_INT_TO_VALUE(LN_VALUE_TYPE(value));
  } else if (word.type == ln_word_func_size_of) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    ln_eval_expr(1);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    return LN_INT_TO_VALUE(sizeof(ln_uint_t));
  } else if (word.type == ln_word_func_to_type) {
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t value = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t type = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(type) != ln_type_number) return LN_INVALID_TYPE;
    
    return ln_cast(value, LN_VALUE_TO_INT(type));
  } else if (word.type == ln_word_func_get) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t index = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr), sizeof(ln_uint_t))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      ln_uint_t value = *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t))));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return value;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_set) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t index = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t value = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr), sizeof(ln_uint_t))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t)))) = value;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return value;
  } else if (word.type == ln_word_func_mem_read) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr), 1)) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      ln_uint_t value = LN_INT_TO_VALUE(ln_data[LN_VALUE_TO_PTR(ptr)]);
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return value;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_mem_write) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t value = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(value) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr), 1)) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      ln_data[LN_VALUE_TO_PTR(ptr)] = LN_VALUE_TO_INT(value);
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return value;
  } else if (word.type == ln_word_func_mem_copy) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t count = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr_1), LN_VALUE_TO_INT(count)) || !ln_check(LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      memcpy(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return count;
  } else if (word.type == ln_word_func_mem_move) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t count = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr_1), LN_VALUE_TO_INT(count)) || !ln_check(LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      memmove(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return count;
  } else if (word.type == ln_word_func_mem_test) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t count = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    int equal = 0;
    
    if (exec) {
      if (!ln_check(LN_VALUE_TO_PTR(ptr_1), LN_VALUE_TO_INT(count)) || !ln_check(LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      equal = !memcmp(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (exec) return LN_INT_TO_VALUE(equal);
    else return LN_NULL;
  } else if (word.type == ln_word_func_mem_alloc) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t size = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(size) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    ln_uint_t ptr = LN_NULL;
    if (exec && LN_VALUE_TO_INT(size) > 0) ptr = ln_heap_alloc(LN_VALUE_TO_INT(size));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return ptr;
  } else if (word.type == ln_word_func_mem_realloc) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t size = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if ((ptr != LN_NULL && LN_VALUE_TYPE(ptr) != ln_type_pointer) || LN_VALUE_TYPE(size) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    ln_uint_t new_ptr = LN_NULL;
    
    if (exec) {
      if (ptr != LN_NULL && !ln_check_heap(LN_VALUE_TO_PTR(ptr))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      new_ptr = ln_heap_realloc(ptr, LN_VALUE_TO_INT(size));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return new_ptr;
  } else if (word.type == ln_word_func_mem_free) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check_heap(LN_VALUE_TO_PTR(ptr))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      ln_heap_free(ptr);
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_str_copy) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      if (!ln_check_string(LN_VALUE_TO_PTR(ptr_2))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      size_t length = strlen((const char *)(ln_data + LN_VALUE_TO_PTR(ptr_2)));
      
      if (!ln_check(LN_VALUE_TO_PTR(ptr_1), length + 1)) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      strcpy((char *)(ln_data + LN_VALUE_TO_PTR(ptr_1)), (const char *)(ln_data + LN_VALUE_TO_PTR(ptr_2)));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return strlen((const char *)(ln_data + LN_VALUE_TO_PTR(ptr_1)));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_str_test) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_comma)) return ln_syntax_error(exec);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    int equal = 0;
    
    if (exec) {
      if (!ln_check_string(LN_VALUE_TO_PTR(ptr_1)) || !ln_check_string(LN_VALUE_TO_PTR(ptr_2))) {
        ln_context_offset = context_offset;
        ln_bump_offset = bump_offset;
        
        return LN_OUT_OF_BOUNDS;
      }
      
      equal = !strcmp((const char *)(ln_data + LN_VALUE_TO_PTR(ptr_1)), (const char *)(ln_data + LN_VALUE_TO_PTR(ptr_2)));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (exec) return LN_INT_TO_VALUE(equal);
    else return LN_NULL;
  } else if (word.type == ln_word_func_str_size) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (!ln_check_string(LN_VALUE_TO_PTR(ptr))) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_OUT_OF_BOUNDS;
    }
    
    if (exec) {
      int length = strlen((const char *)(ln_data + LN_VALUE_TO_PTR(ptr)));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INT_TO_VALUE(length);
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_eval) {
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) {
      const char *old_code = ln_code;
      ln_uint_t old_offset = ln_code_offset;
      
      ln_word_t old_last = ln_last;
      ln_uint_t old_last_curr = ln_last_curr;
      ln_uint_t old_last_next = ln_last_next;
      
      if (!ln_check_string(LN_VALUE_TO_PTR(ptr))) {
        return LN_OUT_OF_BOUNDS;
      }
      
      ln_code = (const char *)(ln_data + LN_VALUE_TO_PTR(ptr));
      ln_code_offset = 0;
      
      ln_last_curr = 0;
      ln_last_next = 0;
      
      ln_uint_t value = ln_eval(1);
      
      ln_code = old_code;
      ln_code_offset = old_offset;
      
      ln_last = old_last;
      ln_last_curr = old_last_curr;
      ln_last_next = old_last_next;
      
      return value;
    }
    
    return LN_NULL;
  } else if (word.type == ln_word_func_import) {
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (!ln_import_handle) return LN_NULL;
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) {
      const char *old_code = ln_code;
      ln_uint_t old_offset = ln_code_offset;
      
      ln_word_t old_last = ln_last;
      ln_uint_t old_last_curr = ln_last_curr;
      ln_uint_t old_last_next = ln_last_next;
      
      if (!ln_check_string(LN_VALUE_TO_PTR(ptr))) {
        return LN_OUT_OF_BOUNDS;
      }
      
      size_t length = strlen((const char *)(ln_data + LN_VALUE_TO_PTR(ptr)));
      ln_uint_t value = LN_NULL;
      
      char path[length + strlen(LN_PATH) + 4];
      
      strcpy(path, LN_PATH);
      strcat(path, (const char *)(ln_data + LN_VALUE_TO_PTR(ptr)));
      strcat(path, ".ln");
      
      if (!ln_import_handle(&value, path)) {
        strcpy(path, (const char *)(ln_data + LN_VALUE_TO_PTR(ptr)));
        ln_import_handle(&value, path);
      }
      
      ln_code = old_code;
      ln_code_offset = old_offset;
      
      ln_last = old_last;
      ln_last_curr = old_last_curr;
      ln_last_next = old_last_next;
      
      return value;
    }
    
    return LN_NULL;
  } else if (word.type == ln_word_give) {
    ln_uint_t value = ln_eval_expr(exec);
    
    if (exec) {
      ln_return = value;
      ln_back = 1;
    }
    
    return LN_NULL;
  } else if (word.type == ln_word_break) {
    if (exec) ln_break = 1;
    return LN_NULL;
  } else if (word.type == ln_word_true) {
    return LN_INT_TO_VALUE(1);
  } else if (word.type == ln_word_false) {
    return LN_INT_TO_VALUE(0);
  } else if (word.type == ln_word_ref) {
    if (!ln_expect(&word, ln_word_name)) return ln_syntax_error(exec);
    
    if (exec) {
      for (int i = ln_context_offset - 1; i >= 0; i--) {
        if (ln_context[i].name == word.data) {
          return LN_PTR_TO_VALUE(ln_context[i].offset);
        }
      }
      
      return LN_UNDEFINED;
    } else {
      return LN_NULL;
    }
  } else if (word.type == ln_word_func_str_format) {
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    ln_uint_t code_offset = ln_code_offset;
    
    int count = 0;
    
    for (;;) {
      ln_eval_expr(0);
      count++;
      
      if (ln_expect(NULL, ln_word_paren_right)) {
        break;
      } else if (!ln_expect(NULL, ln_word_comma)) {
        return ln_syntax_error(exec);
      }
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (!exec) return LN_NULL;
    ln_code_offset = code_offset;
    
    ln_uint_t args[count];
    count = 0;
    
    for (;;) {
      args[count++] = ln_eval_expr(1);
      
      if (ln_expect(NULL, ln_word_paren_right)) {
        break;
      } else if (!ln_expect(NULL, ln_word_comma)) {
        return ln_syntax_error(exec);
      }
    }
    
    ln_context_offset = context_offset;
    
    ln_uint_t offset = ln_bump_offset;
    count = 0;
    
    if (!ln_check_string(LN_VALUE_TO_PTR(args[count]))) {
      return LN_OUT_OF_BOUNDS;
    }
    
    const char *format = (const char *)(ln_data + LN_VALUE_TO_PTR(args[count]));
    count++;
    
    while (*format) {
      if (*format == '[') {
        format++;
        
        if (*format == '[') {
          ln_data[ln_bump_offset++] = '[';
        } else {
          ln_uint_t value = args[count++];
          int type = LN_VALUE_TYPE(value);
          
          if (type == ln_type_number) {
            ln_bump_text_fixed(LN_VALUE_TO_FIXED(value), 1);
          } else if (type == ln_type_pointer) {
            if (!ln_check_string(LN_VALUE_TO_PTR(value))) {
              ln_bump_text("[pointer ");
              ln_bump_offset--;
              
              ln_bump_text_int(LN_VALUE_TO_PTR(value), 1);
              ln_data[ln_bump_offset++] = ']';
            } else {
              ln_bump_text((const char *)(ln_data + LN_VALUE_TO_PTR(value)));
              ln_bump_offset--;
            }
          } else if (value == LN_NULL) {
            ln_bump_text("[null]");
            ln_bump_offset--;
          } else if (value == LN_UNDEFINED) {
            ln_bump_text("[undefined]");
            ln_bump_offset--;
          } else if (value == LN_DIVIDE_BY_ZERO) {
            ln_bump_text("[divide by zero]");
            ln_bump_offset--;
          } else if (value == LN_INVALID_TYPE) {
            ln_bump_text("[invalid type]");
            ln_bump_offset--;
          } else if (value == LN_OUT_OF_BOUNDS) {
            ln_bump_text("[out of bounds]");
            ln_bump_offset--;
          } else if (value == LN_SYNTAX_ERROR) {
            ln_bump_text("[syntax error]");
            ln_bump_offset--;
          } else if (type == ln_type_error) {
            ln_bump_text("[error ");
            ln_bump_offset--;
            
            ln_bump_text_int(LN_VALUE_TO_ERR(value), 1);
            ln_data[ln_bump_offset++] = ']';
          }
        }
      } else {
        ln_data[ln_bump_offset++] = *format;
      }
      
      format++;
    }
    
    ln_data[ln_bump_offset++] = '\0';
    
    memmove(ln_data + bump_offset, ln_data + offset, ln_bump_offset - offset);
    ln_bump_offset = bump_offset + (ln_bump_offset - offset);
    
    return LN_PTR_TO_VALUE(bump_offset);
  } else if (word.type == ln_word_func_str_parse) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (!ln_check_string(LN_VALUE_TO_PTR(ptr))) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_OUT_OF_BOUNDS;
    }
    
    if (exec) {
      ln_int_t fixed = ln_fixed((const char *)(ln_data + LN_VALUE_TO_PTR(ptr)));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_FIXED_TO_VALUE(fixed);
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (word.type == ln_word_func_str_hash) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    if (!ln_expect(NULL, ln_word_paren_left)) return ln_syntax_error(exec);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    if (!ln_expect(NULL, ln_word_paren_right)) return ln_syntax_error(exec);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (!ln_check_string(LN_VALUE_TO_PTR(ptr))) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_OUT_OF_BOUNDS;
    }
    
    if (exec) {
      ln_int_t hash = (ln_int_t)(ln_hash((const char *)(ln_data + LN_VALUE_TO_PTR(ptr))));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INT_TO_VALUE(hash);
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  }
  
  return ln_syntax_error(exec);
}

ln_uint_t ln_eval_1(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_word_t word;
  
  if (ln_expect_multi(&word, (const int[]){ln_word_minus, ln_word_exclam, 0})) {
    if (word.type == ln_word_minus) {
      ln_uint_t value = ln_eval_1(exec);
      return LN_VALUE_NEGATE(value);
    } else if (word.type == ln_word_exclam) {
      ln_uint_t value = ln_eval_1(exec);
      return LN_INT_TO_VALUE(!LN_VALUE_TO_FIXED(value));
    }
  }
  
  return ln_eval_0(exec);
}

ln_uint_t ln_eval_2(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_1(exec);
  ln_word_t word;
  
  while (ln_expect_multi(&word, (const int[]){ln_word_aster, ln_word_slash, ln_word_percent, 0})) {
    ln_uint_t right = ln_eval_1(exec);
    
    if (exec) {
      if (word.type == ln_word_aster) {
        left = LN_FIXED_TO_VALUE(ln_muliply(LN_VALUE_TO_FIXED(left), LN_VALUE_TO_FIXED(right)));
      } else if (word.type == ln_word_slash) {
        // TODO: support bigger numbers
        
        if (!LN_VALUE_TO_FIXED(right)) left = LN_DIVIDE_BY_ZERO;
        else left = LN_FIXED_TO_VALUE((LN_VALUE_TO_FIXED(left) << LN_FIXED_DOT) / LN_VALUE_TO_FIXED(right));
      } else if (word.type == ln_word_percent) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) % LN_VALUE_TO_FIXED(right));
      }
    }
  }
  
  return left;
}

ln_uint_t ln_eval_3(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_2(exec);
  ln_word_t word;
  
  int type = LN_VALUE_TYPE(left);
  int do_cast = 0;
  
  while (ln_expect_multi(&word, (const int[]){ln_word_plus, ln_word_minus, 0})) {
    if (!do_cast) {
      left = ln_cast(left, ln_type_number);
      do_cast = 1;
    }
    
    ln_uint_t right = ln_cast(ln_eval_2(exec), ln_type_number);
    
    if (exec) {
      if (word.type == ln_word_plus) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) + LN_VALUE_TO_FIXED(right));
      } else if (word.type == ln_word_minus) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) - LN_VALUE_TO_FIXED(right));
      }
    }
  }
  
  if (do_cast) return ln_cast(left, type);
  return left;
}

ln_uint_t ln_eval_4(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_3(exec);
  ln_word_t word;
  
  while (ln_expect_multi(&word, (const int[]){ln_word_bit_and, ln_word_bit_or, ln_word_bit_xor, 0})) {
    if (word.type == ln_word_bit_and) {
      ln_uint_t right = ln_eval_3(exec);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) & LN_VALUE_TO_FIXED(right));
    } else if (word.type == ln_word_bit_or) {
      ln_uint_t right = ln_eval_3(exec);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) | LN_VALUE_TO_FIXED(right));
    } else if (word.type == ln_word_bit_xor) {
      ln_uint_t right = ln_eval_3(exec);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) ^ LN_VALUE_TO_FIXED(right));
    } else {
      break;
    }
  }
  
  return left;
}

ln_uint_t ln_eval_5(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t left = ln_eval_4(exec);
  
  for (;;) {
    if (ln_expect(NULL, ln_word_equal)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((left == right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect_2(ln_word_exclam, ln_word_equal)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((left != right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect_2(ln_word_great, ln_word_equal)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((LN_VALUE_TO_FIXED(left) >= LN_VALUE_TO_FIXED(right)) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect_2(ln_word_less, ln_word_equal)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((LN_VALUE_TO_FIXED(left) <= LN_VALUE_TO_FIXED(right)) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect(NULL, ln_word_great)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((LN_VALUE_TO_FIXED(left) > LN_VALUE_TO_FIXED(right)) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect(NULL, ln_word_less)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((LN_VALUE_TO_FIXED(left) < LN_VALUE_TO_FIXED(right)) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else {
      break;
    }
  }
  
  return left;
}

ln_uint_t ln_eval_6(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_5(exec);
  ln_word_t word;
  
  while (ln_expect_multi(&word, (const int[]){ln_word_bool_and, ln_word_bool_or, ln_word_bool_xor, 0})) {
    if (word.type == ln_word_bool_and) {
      ln_uint_t right = ln_eval_5(exec);
      left = LN_INT_TO_VALUE(LN_VALUE_TO_FIXED(left) && LN_VALUE_TO_FIXED(right));
    } else if (word.type == ln_word_bool_or) {
      ln_uint_t right = ln_eval_5(exec);
      left = LN_INT_TO_VALUE(LN_VALUE_TO_FIXED(left) || LN_VALUE_TO_FIXED(right));
    } else if (word.type == ln_word_bool_xor) {
      ln_uint_t right = ln_eval_5(exec);
      left = LN_INT_TO_VALUE(!(!(LN_VALUE_TO_FIXED(left))) != !(!(LN_VALUE_TO_FIXED(right))));
    } else {
      break;
    }
  }
  
  return left;
}

ln_uint_t ln_eval_expr(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t value = ln_eval_6(exec);
  
  return value;
}

ln_uint_t ln_eval_stat(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t value = ln_eval_expr(exec);
  
  while (ln_expect(NULL, ln_word_comma)) {
    value = ln_eval_expr(exec);
  }
  
  return value;
}

ln_uint_t ln_eval(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t value = LN_NULL;
  
  while (!ln_expect(NULL, ln_word_end) && !ln_expect(NULL, ln_word_eof)) {
    value = ln_eval_stat(exec);
  }
  
  return value;
}
