#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <lann.h>

uint8_t ln_data[LN_BUMP_SIZE + LN_HEAP_SIZE];
ln_uint_t ln_bump_offset = 0, ln_bump_args = 0;

uint8_t *ln_heap = ln_data + LN_BUMP_SIZE;
ln_uint_t ln_heap_used = 0;
int ln_heap_inited = 0;

ln_entry_t ln_context[LN_CONTEXT_SIZE];
ln_uint_t ln_context_offset = 0;

const char *ln_code = NULL;
ln_uint_t ln_code_offset = 0;

ln_word_t ln_last;
ln_uint_t ln_last_curr = 0, ln_last_next = 0;

int ln_back = 0, ln_break = 0;
ln_uint_t ln_return = LN_NULL;

ln_uint_t ln_words_total = 0, ln_words_saved = 0;

const int ln_type_match[4] = {ln_type_number, ln_type_number, ln_type_pointer, ln_type_error};

#ifndef LN_HANDLE
static int ln_func_handle(ln_uint_t *value, ln_uint_t hash) { return 0; }
#endif

char ln_upper(char chr) {
  if (chr >= 'a' && chr <= 'z') return (chr - 'a') + 'A';
  return chr;
}

int ln_digit(char chr) {
  return (chr >= '0' && chr <= '9');
}

const char *ln_find_char(const char *str, char chr) {
  while (*str) {
    if (*str == chr) return str;
    str++;
  }
  
  return NULL;
}

int ln_equal(const char *str_1, const char *str_2) {
  for (;;) {
    if (*str_1 != *str_2) return 0;
    if (!(*str_1) || !(*str_2)) return 1;
    
    str_1++, str_2++;
  }
}

int ln_case_equal(const char *str_1, const char *str_2) {
  for (;;) {
    if (ln_upper(*str_1) != ln_upper(*str_2)) return 0;
    if (!(*str_1) || !(*str_2)) return 1;
    
    str_1++, str_2++;
  }
}

ln_uint_t ln_hash(const char *text) {
  ln_uint_t hash = 0;
  
  const char *old_text = text;
  
  while (*text) {
    hash += *(text++);
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  
  return hash;
}

ln_uint_t ln_fixed(const char *text) {
  ln_uint_t value = 0;
  int base = 10;
  
  if (*text == '0') text++;
  
  if (ln_upper(*text) == 'B') base = 2, text++;
  else if (ln_upper(*text) == 'O') base = 8, text++;
  else if (ln_upper(*text) == 'X') base = 16, text++;
  
  const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *ptr;
  
  while (ptr = ln_find_char(digits, ln_upper(*text))) {
    if (!(*ptr)) break;
    
    value = (value * base) + (ptr - digits);
    text++;
  }
  
  value <<= LN_FIXED_DOT;
  
  if (*text != '.') return value;
  text++;
  
  int factor = base;
  
  while (ptr = ln_find_char(digits, ln_upper(*text))) {
    if (!(*ptr)) break;
    
    value += ((1 << LN_FIXED_DOT) * (ptr - digits)) / factor;
    text++;
    
    factor *= base;
  }
  
  return value;
}

void ln_heap_init(void) {
  ln_heap_inited = 1;
  
  ln_heap_t *block = (ln_heap_t *)(ln_heap);
  
  block->size = LN_HEAP_SIZE - sizeof(ln_heap_t);
  block->free = 1;
  
  ln_heap_used = sizeof(ln_heap_t);
}

void ln_heap_defrag(void) {
  // TODO: defragmentation
}

ln_uint_t ln_heap_alloc(ln_uint_t size) {
  if (!ln_heap_inited) ln_heap_init();
  ln_heap_t *block = (ln_heap_t *)(ln_heap);
  
  while ((uint8_t *)(block) < ln_heap + LN_HEAP_SIZE) {
    if (block->free) {
      if (block->size == size) {
        block->free = 0;
        
        ln_heap_used += size;
        return LN_PTR_TO_VALUE(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t));
      } else if (block->size >= size + sizeof(ln_heap_t)) {
        ln_heap_t *new_block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + size);
        ln_uint_t old_size = block->size;
        
        block->size = size;
        block->free = 0;
        
        new_block->size = old_size - (block->size + sizeof(ln_heap_t));
        new_block->free = 1;
        
        ln_heap_used += size + sizeof(ln_heap_t);
        return LN_PTR_TO_VALUE(((uint8_t *)(block) - ln_data) + sizeof(ln_heap_t));
      }
    }
    
    block = (ln_heap_t *)((uint8_t *)(block) + sizeof(ln_heap_t) + block->size);
  }
  
  return LN_NULL;
}

ln_uint_t ln_heap_realloc(ln_uint_t ptr, ln_uint_t new_size) {
  if (!ln_heap_inited) ln_heap_init();
  if (ptr == LN_NULL) return ln_heap_alloc(new_size);
  
  ln_uint_t new_ptr = ln_heap_alloc(new_size);
  if (new_ptr == LN_NULL) return LN_NULL;
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + (LN_VALUE_TO_PTR(ptr) - sizeof(ln_heap_t)));
  memcpy(ln_data + LN_VALUE_TO_PTR(new_ptr), ln_data + LN_VALUE_TO_PTR(ptr), block->size < new_size ? block->size : new_size);
  
  ln_heap_free(ptr);
  return new_ptr;
}

void ln_heap_free(ln_uint_t ptr) {
  if (!ln_heap_inited) ln_heap_init();
  if (ptr == LN_NULL) return;
  
  ln_heap_t *block = (ln_heap_t *)(ln_data + (LN_VALUE_TO_PTR(ptr) - sizeof(ln_heap_t)));
  block->free = 1;
  
  ln_heap_used -= block->size;
  ln_heap_defrag();
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
  
  fixed = (fixed % (1 << LN_FIXED_DOT)) * 10;
  
  if (!fixed) return;
  ln_data[ln_bump_offset++] = '.';
  
  for (int i = 0; i < 7; i++) {
    digit = (fixed >> LN_FIXED_DOT) % 10;
    ln_data[ln_bump_offset++] = '0' + digit;
    
    if (!(fixed % (1 << LN_FIXED_DOT))) break;
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
  
  if (old_type == ln_type_number) {
    if (type == ln_type_pointer) return LN_PTR_TO_VALUE((ln_uint_t)(LN_VALUE_TO_INT(value)));
    else if (type == ln_type_error) return LN_ERR_TO_VALUE((ln_uint_t)(LN_VALUE_TO_INT(value)));
  } else if (old_type == ln_type_pointer) {
    if (type == ln_type_number) return LN_INT_TO_VALUE(LN_VALUE_TO_PTR(value));
    else if (type == ln_type_error) LN_ERR_TO_VALUE(LN_VALUE_TO_PTR(value));
  } else if (old_type == ln_type_error) {
    if (type == ln_type_number) return LN_INT_TO_VALUE(LN_VALUE_TO_ERR(value));
    else if (type == ln_type_pointer) LN_PTR_TO_VALUE(LN_VALUE_TO_ERR(value));
  }
  
  return LN_NULL;
}

static int ln_space(char chr) {
  return (chr && !(!ln_find_char(": \t\r\n", chr)));
}

char ln_keep(int in_string) {
  if (!in_string) {
    int is_delim = 0;
    
    for (;;) {
      if (ln_code[ln_code_offset] == '#') {
        while (ln_code[ln_code_offset] && ln_code[ln_code_offset] != '\n') ln_code_offset++;
        is_delim = 1;
      } else if (ln_space(ln_code[ln_code_offset])) {
        ln_code_offset++;
        is_delim = 1;
      } else {
        break;
      }
    }
    
    if (is_delim) return LN_CHAR_DELIM;
  }
  
  if (!ln_code[ln_code_offset]) return LN_CHAR_EOF;
  return ln_code[ln_code_offset];
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
  char *token = ln_data + offset;
  
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
          
          if (!(ptr = ln_find_char(digits, chr))) break;
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
          
          if (!(ptr = ln_find_char(digits, chr))) break;
          
          value = (value * 8) + (ptr - digits);
          ln_skip(chr, in_string);
        }
        
        ln_data[ln_bump_offset++] = (char)(value);
      } else {
        ln_data[ln_bump_offset++] = chr;
      }
      
      continue;
    }
    
    if (in_string && ln_find_char("\r\n", chr)) {
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
    
    if (!in_string && ln_find_char(",()+-*/%&|^!<=>", chr)) {
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
  
  if (!token[0]) {
    return (ln_word_t){.type = ln_word_eof};
  } else if (ln_case_equal(token, "null")) {
    return (ln_word_t){.type = ln_word_null};
  } else if (ln_case_equal(token, "true")) {
    return (ln_word_t){.type = ln_word_true};
  } else if (ln_case_equal(token, "false")) {
    return (ln_word_t){.type = ln_word_false};
  } else if (ln_case_equal(token, "block")) {
    return (ln_word_t){.type = ln_word_block};
  } else if (ln_case_equal(token, "begin")) {
    return (ln_word_t){.type = ln_word_begin};
  } else if (ln_case_equal(token, "end")) {
    return (ln_word_t){.type = ln_word_end};
  } else if (ln_case_equal(token, "func")) {
    return (ln_word_t){.type = ln_word_func};
  } else if (ln_case_equal(token, "let")) {
    return (ln_word_t){.type = ln_word_let};
  } else if (ln_case_equal(token, "if")) {
    return (ln_word_t){.type = ln_word_if};
  } else if (ln_case_equal(token, "else")) {
    return (ln_word_t){.type = ln_word_else};
  } else if (ln_case_equal(token, "array")) {
    return (ln_word_t){.type = ln_word_array};
  } else if (ln_case_equal(token, "while")) {
    return (ln_word_t){.type = ln_word_while};
  } else if (ln_case_equal(token, "give")) {
    return (ln_word_t){.type = ln_word_give};
  } else if (ln_case_equal(token, "break")) {
    return (ln_word_t){.type = ln_word_break};
  } else if (ln_case_equal(token, "ref")) {
    return (ln_word_t){.type = ln_word_ref};
  } else if (ln_case_equal(token, "args")) {
    return (ln_word_t){.type = ln_word_args};
  } else if (ln_case_equal(token, "number")) {
    return (ln_word_t){.type = ln_word_type_number};
  } else if (ln_case_equal(token, "pointer")) {
    return (ln_word_t){.type = ln_word_type_pointer};
  } else if (ln_case_equal(token, "error")) {
    return (ln_word_t){.type = ln_word_type_error};
  } else if (ln_case_equal(token, "type_of")) {
    return (ln_word_t){.type = ln_word_func_type_of};
  } else if (ln_case_equal(token, "size_of")) {
    return (ln_word_t){.type = ln_word_func_size_of};
  } else if (ln_case_equal(token, "to_type")) {
    return (ln_word_t){.type = ln_word_func_to_type};
  } else if (ln_case_equal(token, "get")) {
    return (ln_word_t){.type = ln_word_func_get};
  } else if (ln_case_equal(token, "set")) {
    return (ln_word_t){.type = ln_word_func_set};
  } else if (ln_case_equal(token, "mem_read")) {
    return (ln_word_t){.type = ln_word_func_mem_read};
  } else if (ln_case_equal(token, "mem_write")) {
    return (ln_word_t){.type = ln_word_func_mem_write};
  } else if (ln_case_equal(token, "mem_copy")) {
    return (ln_word_t){.type = ln_word_func_mem_copy};
  } else if (ln_case_equal(token, "mem_move")) {
    return (ln_word_t){.type = ln_word_func_mem_move};
  } else if (ln_case_equal(token, "mem_test")) {
    return (ln_word_t){.type = ln_word_func_mem_test};
  } else if (ln_case_equal(token, "mem_alloc")) {
    return (ln_word_t){.type = ln_word_func_mem_alloc};
  } else if (ln_case_equal(token, "mem_realloc")) {
    return (ln_word_t){.type = ln_word_func_mem_realloc};
  } else if (ln_case_equal(token, "mem_free")) {
    return (ln_word_t){.type = ln_word_func_mem_free};
  } else if (ln_case_equal(token, "str_copy")) {
    return (ln_word_t){.type = ln_word_func_str_copy};
  } else if (ln_case_equal(token, "str_test")) {
    return (ln_word_t){.type = ln_word_func_str_test};
  } else if (ln_case_equal(token, "str_size")) {
    return (ln_word_t){.type = ln_word_func_str_size};
  } else if (ln_case_equal(token, "str_format")) {
    return (ln_word_t){.type = ln_word_func_str_format};
  } else if (ln_case_equal(token, "eval")) {
    return (ln_word_t){.type = ln_word_func_eval};
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
  } else if (ln_case_equal(token, "and")) {
    return (ln_word_t){.type = ln_word_bool_and};
  } else if (ln_case_equal(token, "or")) {
    return (ln_word_t){.type = ln_word_bool_or};
  } else if (ln_case_equal(token, "xor")) {
    return (ln_word_t){.type = ln_word_bool_xor};
  } else if (ln_digit(token[0])) {
    return (ln_word_t){.type = ln_word_number, .data = ln_fixed(token)};
  } else if (in_string && string_chr == '\'') {
    return (ln_word_t){.type = ln_word_number, .data = ((ln_uint_t)(token[0]) << LN_FIXED_DOT)};
  } else {
    return (ln_word_t){.type = ln_word_name, .data = ln_hash(ln_data + offset)};
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

ln_uint_t ln_eval_0(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_word_t word;
  
  if (ln_expect(&word, ln_word_number)) {
    return word.data;
  } else if (ln_expect(&word, ln_word_string)) {
    if (exec) return LN_PTR_TO_VALUE(word.data);
    
    ln_bump_offset = word.data;
    return LN_NULL;
  } else if (ln_expect(&word, ln_word_name)) {
    if (ln_expect(NULL, ln_word_paren_left)) {
      ln_uint_t context_offset = ln_context_offset;
      ln_uint_t bump_offset = ln_bump_offset;
      ln_uint_t code_offset = ln_code_offset;
      
      int count = 0;
      
      while (!ln_expect(NULL, ln_word_paren_right)) {
        ln_eval_expr(0);
        ln_expect(NULL, ln_word_comma);
        
        count++;
      }
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      if (!exec) return LN_NULL;
      
      ln_code_offset = code_offset;
      
      ln_uint_t args[count];
      count = 0;
      
      while (!ln_expect(NULL, ln_word_paren_right)) {
        args[count++] = ln_eval_expr(1);
        ln_expect(NULL, ln_word_comma);
      }
      
      ln_context_offset = context_offset;
      
      ln_uint_t bump_args = ln_bump_args;
      ln_bump_args = ln_bump_offset;
      
      for (int i = 0; i < count; i++) {
        ln_bump_value(args[i]);
      }
      
      ln_bump_value(LN_NULL);
      ln_uint_t value = LN_UNDEFINED;
      
      if (!ln_func_handle(&value, word.data)) {
        for (int i = ln_context_offset - 1; i >= 0; i--) {
          if (ln_context[i].name == word.data) {
            const char *old_code = ln_code;
            ln_uint_t old_offset = ln_code_offset;
            
            ln_word_t old_last = ln_last;
            ln_uint_t old_last_curr = ln_last_curr;
            ln_uint_t old_last_next = ln_last_next;
            
            ln_code = ln_data + LN_VALUE_TO_PTR(*((ln_uint_t *)(ln_data + ln_context[i].offset)));
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
          if (ln_context[i].name == word.data) {
            return *((ln_uint_t *)(ln_data + ln_context[i].offset));
          }
        }
        
        return LN_UNDEFINED;
      } else {
        return LN_NULL;
      }
    }
  } else if (ln_expect(NULL, ln_word_paren_left)) {
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    return value;
  } else if (ln_expect(NULL, ln_word_block)) {
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
  } else if (ln_expect(NULL, ln_word_begin)) {
    return ln_eval(exec);
  } else if (ln_expect(NULL, ln_word_let)) {
    ln_expect(&word, ln_word_name);
    ln_uint_t value = ln_eval_expr(exec);
    
    if (exec) {
      int found = 0;
      
      for (int i = ln_context_offset - 1; i >= 0; i--) {
        if (ln_context[i].name == word.data) {
          *((ln_uint_t *)(ln_data + ln_context[i].offset)) = value;
          found = 1;
          
          break;
        }
      }
      
      if (!found) {
        ln_context[ln_context_offset++] = (ln_entry_t){
          .name = word.data,
          .offset = ln_bump_value(value)
        };
      }
    }
    
    return value;
  } else if (ln_expect(NULL, ln_word_func)) {
    ln_expect(&word, ln_word_name);
    
    ln_uint_t code_start = ln_code_offset;
    while (ln_space(ln_code[code_start])) code_start++;
    
    ln_eval_stat(0);
    
    ln_uint_t code_end = ln_code_offset;
    while (code_end && ln_space(ln_code[code_end - 1])) code_end--;
    
    ln_uint_t offset = ln_bump_offset;
    
    if (exec) {
      for (int i = code_start; i < code_end; i++) {
        ln_data[ln_bump_offset++] = ln_code[i];
      }
      
      ln_data[ln_bump_offset++] = '\0';
      
      ln_context[ln_context_offset++] = (ln_entry_t){
        .name = word.data,
        .offset = ln_bump_value(LN_PTR_TO_VALUE(offset))
      };
    }
    
    return LN_PTR_TO_VALUE(offset);
  } else if (ln_expect(NULL, ln_word_array)) {
    ln_expect(&word, ln_word_name);
    
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
  } else if (ln_expect(NULL, ln_word_null)) {
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_if)) {
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
    
    ln_expect(NULL, ln_word_comma);
    
    if (ln_expect(NULL, ln_word_else)) {
      if (!cond && exec) value = ln_eval_expr(1);
      else ln_eval_expr(0);
    }
    
    return value;
  } else if (ln_expect(NULL, ln_word_while)) {
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
  } else if (ln_expect(NULL, ln_word_args)) {
    return LN_PTR_TO_VALUE(ln_bump_args);
  } else if (ln_expect(NULL, ln_word_type_number)) {
    return LN_INT_TO_VALUE(ln_type_number);
  } else if (ln_expect(NULL, ln_word_type_pointer)) {
    return LN_INT_TO_VALUE(ln_type_pointer);
  } else if (ln_expect(NULL, ln_word_type_error)) {
    return LN_INT_TO_VALUE(ln_type_error);
  } else if (ln_expect(NULL, ln_word_func_type_of)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    ln_uint_t value = ln_eval_expr(exec);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    ln_expect(NULL, ln_word_paren_right);
    return LN_INT_TO_VALUE(LN_VALUE_TYPE(value));
  } else if (ln_expect(NULL, ln_word_func_size_of)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    ln_eval_expr(1);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    ln_expect(NULL, ln_word_paren_right);
    return LN_INT_TO_VALUE(sizeof(ln_uint_t));
  } else if (ln_expect(NULL, ln_word_func_to_type)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t type = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(type) != ln_type_number) return LN_INVALID_TYPE;
    
    return ln_cast(value, LN_VALUE_TO_INT(type));
  } else if (ln_expect(NULL, ln_word_func_get)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t index = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      ln_uint_t value = *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t))));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return value;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_set)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t index = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) *((ln_uint_t *)(ln_data + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t)))) = value;
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return value;
  } else if (ln_expect(NULL, ln_word_func_mem_read)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      ln_uint_t value = LN_INT_TO_VALUE(ln_data[LN_VALUE_TO_PTR(ptr)]);
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return value;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_mem_write)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(value) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) ln_data[LN_VALUE_TO_PTR(ptr)] = LN_VALUE_TO_INT(value);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return value;
  } else if (ln_expect(NULL, ln_word_func_mem_copy)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t count = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) memcpy(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return count;
  } else if (ln_expect(NULL, ln_word_func_mem_move)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t count = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) memmove(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return count;
  } else if (ln_expect(NULL, ln_word_func_mem_test)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t count = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    int equal = 0;
    if (exec) equal = !memcmp(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (exec) return LN_INT_TO_VALUE(equal);
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_mem_alloc)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t size = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(size) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    ln_uint_t ptr = LN_NULL;
    if (exec) ptr = ln_heap_alloc(LN_VALUE_TO_INT(size));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return ptr;
  } else if (ln_expect(NULL, ln_word_func_mem_realloc)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t size = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(size) != ln_type_number) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    ln_uint_t new_ptr = LN_NULL;
    if (exec) new_ptr = ln_heap_realloc(ptr, LN_VALUE_TO_INT(size));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return new_ptr;
  } else if (ln_expect(NULL, ln_word_func_mem_free)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) ln_heap_free(ptr);
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_copy)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    if (exec) {
      strcpy(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2));
      
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return strlen(ln_data + LN_VALUE_TO_PTR(ptr_1));
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_test)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) {
      ln_context_offset = context_offset;
      ln_bump_offset = bump_offset;
      
      return LN_INVALID_TYPE;
    }
    
    int equal = 0;
    if (exec) equal = ln_equal(ln_data + LN_VALUE_TO_PTR(ptr_1), ln_data + LN_VALUE_TO_PTR(ptr_2));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (exec) return LN_INT_TO_VALUE(equal);
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_size)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    int length = strlen(ln_data + LN_VALUE_TO_PTR(ptr));
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (exec) return LN_INT_TO_VALUE(length);
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_eval)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) {
      const char *old_code = ln_code;
      ln_uint_t old_offset = ln_code_offset;
      
      ln_word_t old_last = ln_last;
      ln_uint_t old_last_curr = ln_last_curr;
      ln_uint_t old_last_next = ln_last_next;
      
      ln_code = ln_data + LN_VALUE_TO_PTR(ptr);
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
  } else if (ln_expect(NULL, ln_word_give)) {
    ln_uint_t value = ln_eval_expr(exec);
    
    if (exec) {
      ln_return = value;
      ln_back = 1;
    }
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_break)) {
    if (exec) ln_break = 1;
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_true)) {
    return LN_INT_TO_VALUE(1);
  } else if (ln_expect(NULL, ln_word_false)) {
    return LN_INT_TO_VALUE(0);
  } else if (ln_expect(NULL, ln_word_ref)) {
    ln_expect(&word, ln_word_name);
    
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
  } else if (ln_expect(NULL, ln_word_func_str_format)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    ln_uint_t code_offset = ln_code_offset;
    
    int count = 0;
    
    while (!ln_expect(NULL, ln_word_paren_right)) {
      ln_uint_t value = ln_eval_expr(0);
      ln_expect(NULL, ln_word_comma);
      
      count++;
    }
    
    ln_context_offset = context_offset;
    ln_bump_offset = bump_offset;
    
    if (!exec) return LN_NULL;
    ln_code_offset = code_offset;
    
    ln_uint_t args[count];
    count = 0;
    
    while (!ln_expect(NULL, ln_word_paren_right)) {
      args[count++] = ln_eval_expr(1);
      ln_expect(NULL, ln_word_comma);
    }
    
    ln_context_offset = context_offset;
    
    ln_uint_t offset = ln_bump_offset;
    count = 0;
    
    char *format = ln_data + LN_VALUE_TO_PTR(args[count]);
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
            ln_bump_text(ln_data + LN_VALUE_TO_PTR(value));
            ln_bump_offset--;
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
  }
  
  return LN_NULL;
}

ln_uint_t ln_eval_1(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  if (ln_expect(NULL, ln_word_minus)) {
    ln_uint_t value = ln_eval_1(exec);
    return LN_VALUE_NEGATE(value);
  } else if (ln_expect(NULL, ln_word_exclam)) {
    ln_uint_t value = ln_eval_1(exec);
    return LN_INT_TO_VALUE(!LN_VALUE_TO_FIXED(value));
  }
  
  return ln_eval_0(exec);
}

ln_uint_t ln_eval_2(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_1(exec);
  ln_word_t word;
  
  int type = LN_VALUE_TYPE(left);
  left = ln_cast(left, ln_type_number);
  
  while (ln_expect(&word, ln_word_aster) || ln_expect(&word, ln_word_slash) || ln_expect(&word, ln_word_percent)) {
    ln_uint_t right = ln_cast(ln_eval_1(exec), ln_type_number);
    
    if (exec) {
      if (word.type == ln_word_aster) {
        // TODO: support bigger numbers
        left = LN_FIXED_TO_VALUE((LN_VALUE_TO_FIXED(left) * LN_VALUE_TO_FIXED(right)) >> LN_FIXED_DOT);
      } else if (word.type == ln_word_slash) {
        // TODO: support bigger numbers
        
        if (!LN_VALUE_TO_FIXED(right)) left = LN_DIVIDE_BY_ZERO;
        else left = LN_FIXED_TO_VALUE((LN_VALUE_TO_FIXED(left) << LN_FIXED_DOT) / LN_VALUE_TO_FIXED(right));
      } else if (word.type == ln_word_percent) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) % LN_VALUE_TO_FIXED(right));
      }
    }
  }
  
  return ln_cast(left, type);
}

ln_uint_t ln_eval_3(int exec) {
  if (ln_back || ln_break) exec = 0;
  
  ln_uint_t left = ln_eval_2(exec);
  ln_word_t word;
  
  int type = LN_VALUE_TYPE(left);
  left = ln_cast(left, ln_type_number);
  
  while (ln_expect(&word, ln_word_plus) || ln_expect(&word, ln_word_minus)) {
    ln_uint_t right = ln_cast(ln_eval_2(exec), ln_type_number);
    
    if (exec) {
      if (word.type == ln_word_plus) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) + LN_VALUE_TO_FIXED(right));
      } else if (word.type == ln_word_minus) {
        left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) - LN_VALUE_TO_FIXED(right));
      }
    }
  }
  
  return ln_cast(left, type);
}

ln_uint_t ln_eval_4(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t left = ln_eval_3(exec);
  
  int type = LN_VALUE_TYPE(left);
  left = ln_cast(left, ln_type_number);
  
  for (;;) {
    if (ln_expect(NULL, ln_word_bit_and)) {
      ln_uint_t right = ln_cast(ln_eval_3(exec), ln_type_number);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) & LN_VALUE_TO_FIXED(right));
    } else if (ln_expect(NULL, ln_word_bit_or)) {
      ln_uint_t right = ln_cast(ln_eval_3(exec), ln_type_number);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) | LN_VALUE_TO_FIXED(right));
    } else if (ln_expect(NULL, ln_word_bit_xor)) {
      ln_uint_t right = ln_cast(ln_eval_3(exec), ln_type_number);
      left = LN_FIXED_TO_VALUE(LN_VALUE_TO_FIXED(left) ^ LN_VALUE_TO_FIXED(right));
    } else {
      break;
    }
  }
  
  return ln_cast(left, type);
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
      left = ((left >= right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect_2(ln_word_less, ln_word_equal)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((left <= right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect(NULL, ln_word_great)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((left > right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else if (ln_expect(NULL, ln_word_less)) {
      ln_uint_t right = ln_eval_4(exec);
      left = ((left < right) ? LN_INT_TO_VALUE(1) : LN_INT_TO_VALUE(0));
    } else {
      break;
    }
  }
  
  return left;
}

ln_uint_t ln_eval_6(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t left = ln_eval_5(exec);
  
  for (;;) {
    if (ln_expect(NULL, ln_word_bool_and)) {
      ln_uint_t right = ln_eval_5(exec);
      left = LN_INT_TO_VALUE(LN_VALUE_TO_FIXED(left) && LN_VALUE_TO_FIXED(right));
    } else if (ln_expect(NULL, ln_word_bool_or)) {
      ln_uint_t right = ln_eval_5(exec);
      left = LN_INT_TO_VALUE(LN_VALUE_TO_FIXED(left) || LN_VALUE_TO_FIXED(right));
    } else if (ln_expect(NULL, ln_word_bool_xor)) {
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
  ln_uint_t value;
  
  while (!ln_expect(NULL, ln_word_end) && !ln_expect(NULL, ln_word_eof)) {
    value = ln_eval_stat(exec);
  }
  
  return value;
}
