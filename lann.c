#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <lann.h>

uint8_t ln_bump[LN_BUMP_SIZE];
ln_uint_t ln_bump_offset = 0, ln_bump_args = 0;

ln_entry_t ln_context[LN_CONTEXT_SIZE];
ln_uint_t ln_context_offset = 0;

const char *ln_code = NULL;
ln_uint_t ln_code_offset = 0;

int ln_back = 0, ln_break = 0;
ln_uint_t ln_return = LN_NULL;

const int ln_type_match[4] = {ln_type_number, ln_type_number, ln_type_pointer, ln_type_error};

#ifndef LN_HANDLE

static int ln_func_handle(ln_uint_t *value, ln_uint_t hash) {
  return 0;
}

#endif

char ln_upper(char chr) {
  if (chr >= 'a' && chr <= 'z') return (chr - 'a') + 'A';
  return chr;
}

int ln_digit(char chr) {
  return (chr >= '0' && chr <= '9');
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
  char *ptr;
  
  while (ptr = strchr(digits, ln_upper(*text))) {
    if (!(*ptr)) break;
    
    value = (value * base) + (ptr - digits);
    text++;
  }
  
  value <<= LN_FIXED_DOT;
  
  if (*text != '.') return value;
  text++;
  
  int factor = base;
  
  while (ptr = strchr(digits, ln_upper(*text))) {
    if (!(*ptr)) break;
    
    value += ((1 << LN_FIXED_DOT) * (ptr - digits)) / factor;
    text++;
    
    factor *= base;
  }
  
  return value;
}

ln_uint_t ln_bump_value(ln_uint_t value) {
  memcpy(ln_bump + ln_bump_offset, &value, sizeof(ln_uint_t));
  
  ln_uint_t offset = ln_bump_offset;
  ln_bump_offset += sizeof(ln_uint_t);
  
  return offset;
}

ln_uint_t ln_bump_text(const char *text) {
  ln_uint_t length = strlen(text);
  
  memcpy(ln_bump + ln_bump_offset, text, length + 1);
  
  ln_uint_t offset = ln_bump_offset;
  ln_bump_offset += length + 1;
  
  return offset;
}

ln_uint_t ln_get_arg(int index) {
  return ((ln_uint_t *)(ln_bump + ln_bump_args))[index];
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
  return (chr && !(!strchr(": \t\r\n", chr)));
}

char ln_read(int in_string) {
  if (!in_string) {
    if (ln_code[ln_code_offset] == '#') {
      while (ln_code[ln_code_offset] && ln_code[ln_code_offset] != '\n') ln_code_offset++;
    }
    
    if (ln_space(ln_code[ln_code_offset])) {
      while (ln_code[ln_code_offset + 1] && ln_space(ln_code[ln_code_offset + 1])) ln_code_offset++;
      ln_code_offset++;
      
      return LN_CHAR_DELIM;
    }
  }
  
  if (!ln_code[ln_code_offset]) return LN_CHAR_EOF;
  return ln_code[ln_code_offset++];
}

ln_word_t ln_take(void) {
  ln_uint_t offset = ln_bump_offset;
  char *token = ln_bump + offset;
  
  int in_string = 0;
  
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
        ln_bump[ln_bump_offset++] = '\r';
      } else if (chr == 'n') {
        ln_bump[ln_bump_offset++] = '\n';
      } else if (chr == 't') {
        ln_bump[ln_bump_offset++] = '\t';
      } else {
        ln_bump[ln_bump_offset++] = chr;
      }
      
      continue;
    }
    
    if (in_string && strchr("\r\n", chr)) {
      continue;
    }
    
    if (chr == '"') {
      if (in_string) {
        break;
      } else {
        in_string = 1;
        continue;
      }
    }
    
    if (!in_string && strchr(",()+-*/%&|^!<=>", chr)) {
      if (ln_bump_offset - offset) {
        ln_code_offset--;
      } else {
        ln_bump[ln_bump_offset++] = chr;
      }
      
      break;
    }
    
    ln_bump[ln_bump_offset++] = chr;
  }
  
  ln_bump[ln_bump_offset++] = '\0';
  
  if (in_string) {
    for (int i = 0; i < offset; i++) {
      if (!strcmp(ln_bump + i, ln_bump + offset)) {
        ln_bump_offset = offset;
        offset = i;
        
        token = ln_bump + offset;
        break;
      }
    }
    
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
  } else if (ln_case_equal(token, "lambda")) {
    return (ln_word_t){.type = ln_word_lambda};
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
  } else if (ln_case_equal(token, "back")) {
    return (ln_word_t){.type = ln_word_back};
  } else if (ln_case_equal(token, "break")) {
    return (ln_word_t){.type = ln_word_break};
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
  } else if (ln_case_equal(token, "mem_test")) {
    return (ln_word_t){.type = ln_word_func_mem_test};
  } else if (ln_case_equal(token, "str_copy")) {
    return (ln_word_t){.type = ln_word_func_str_copy};
  } else if (ln_case_equal(token, "str_test")) {
    return (ln_word_t){.type = ln_word_func_str_test};
  } else if (ln_case_equal(token, "str_size")) {
    return (ln_word_t){.type = ln_word_func_str_size};
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
  } else {
    return (ln_word_t){.type = ln_word_name, .data = ln_hash(ln_bump + offset)};
  }
}

ln_word_t ln_peek(void) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word = ln_take();
  
  ln_code_offset = offset;
  return word;
}

int ln_expect(ln_word_t *ptr, uint8_t type) {
  ln_uint_t offset = ln_code_offset;
  ln_word_t word = ln_take();
  
  if (type != word.type) {
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
    ln_code_offset = offset;
    return 0;
  }
  
  ln_word_t word_2 = ln_take();
  
  if (type_2 != word_2.type) {
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
    return LN_PTR_TO_VALUE(word.data);
  } else if (ln_expect(&word, ln_word_name)) {
    if (ln_expect(NULL, ln_word_paren_left)) {
      ln_uint_t bump_offset = ln_bump_offset;
      ln_uint_t code_offset = ln_code_offset;
      
      if (exec) {
        while (!ln_expect(NULL, ln_word_paren_right)) {
          ln_uint_t value = ln_eval_expr(0);
          ln_expect(NULL, ln_word_comma);
        }
        
        ln_code_offset = code_offset;
      }
      
      ln_uint_t bump_args = ln_bump_args;
      ln_bump_args = ln_bump_offset;
      
      while (!ln_expect(NULL, ln_word_paren_right)) {
        ln_uint_t value = ln_eval_expr(exec);
        if (exec) ln_bump_value(value);
        
        ln_expect(NULL, ln_word_comma);
      }
      
      ln_uint_t value = LN_NULL;
      
      if (exec) {
        ln_bump_value(LN_NULL);
        value = LN_UNDEFINED;
        
        if (!ln_func_handle(&value, word.data)) {
          for (int i = ln_context_offset - 1; i >= 0; i--) {
            if (ln_context[i].name == word.data) {
              const char *old_code = ln_code;
              ln_uint_t old_offset = ln_code_offset;
              
              ln_code = ln_bump + LN_VALUE_TO_PTR(ln_context[i].data);
              ln_code_offset = 0;
              
              ln_return = LN_NULL;
              
              value = ln_eval_stat(1);
              ln_back = 0;
              
              ln_code = old_code;
              ln_code_offset = old_offset;
              
              break;
            }
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
            return ln_context[i].data;
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
  } else if (ln_expect(NULL, ln_word_lambda)) {
    ln_uint_t context_offset = ln_context_offset;
    ln_uint_t bump_offset = ln_bump_offset;
    
    ln_uint_t value = ln_eval_expr(exec);
    
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
          ln_context[i].data = value;
          found = 1;
          
          break;
        }
      }
      
      if (!found) {
        ln_context[ln_context_offset++] = (ln_entry_t){
          .name = word.data,
          .data = value
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
        ln_bump[ln_bump_offset++] = ln_code[i];
      }
      
      ln_bump[ln_bump_offset++] = '\0';
      
      ln_context[ln_context_offset++] = (ln_entry_t){
        .name = word.data,
        .data = LN_PTR_TO_VALUE(offset)
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
        .data = LN_PTR_TO_VALUE(offset)
      };
      
      return LN_PTR_TO_VALUE(offset);
    }
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_null)) {
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_if)) {
    ln_uint_t value = ln_eval_expr(exec);
    int cond = LN_VALUE_TO_FIXED(value);
    
    if (exec) {
      if (LN_VALUE_TYPE(value) == ln_type_pointer) cond = ln_bump[LN_VALUE_TO_PTR(value)];
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
    
    for (;;) {
      value = ln_eval_expr(exec);
      int cond = LN_VALUE_TO_FIXED(value);
      
      if (LN_VALUE_TYPE(value) == ln_type_pointer) cond = ln_bump[LN_VALUE_TO_PTR(value)];
      if (LN_VALUE_TYPE(value) == ln_type_error) cond = 0;
      
      value = LN_NULL;
      
      if (cond && exec) {
        value = ln_eval_expr(1);
        ln_code_offset = code_offset;
        
        if (ln_back || ln_break) break;
      } else {
        ln_eval_expr(0);
        break;
      }
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
    ln_expect(NULL, ln_word_paren_left);
    ln_uint_t value = ln_eval_expr(exec);
    
    ln_expect(NULL, ln_word_paren_right);
    return LN_INT_TO_VALUE(LN_VALUE_TYPE(value));
  } else if (ln_expect(NULL, ln_word_func_size_of)) {
    ln_expect(NULL, ln_word_paren_left);
    ln_eval_expr(1);
    
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
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t index = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) return LN_INVALID_TYPE;
    
    if (exec) return *((ln_uint_t *)(ln_bump + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t))));
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_set)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t index = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(index) != ln_type_number) return LN_INVALID_TYPE;
    
    if (exec) *((ln_uint_t *)(ln_bump + LN_VALUE_TO_PTR(ptr) + (LN_VALUE_TO_INT(index) * sizeof(ln_uint_t)))) = value;
    return value;
  } else if (ln_expect(NULL, ln_word_func_mem_read)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) return LN_INT_TO_VALUE(ln_bump[LN_VALUE_TO_PTR(ptr)]);
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_mem_write)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t value = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer || LN_VALUE_TYPE(value) != ln_type_number) return LN_INVALID_TYPE;
    
    if (exec) ln_bump[LN_VALUE_TO_PTR(ptr)] = LN_VALUE_TO_INT(value);
    return value;
  } else if (ln_expect(NULL, ln_word_func_mem_copy)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t count = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) return LN_INVALID_TYPE;
    
    if (exec) memcpy(ln_bump + LN_VALUE_TO_PTR(ptr_1), ln_bump + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count));
    return count;
  } else if (ln_expect(NULL, ln_word_func_mem_test)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t count = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer || LN_VALUE_TYPE(count) != ln_type_number) return LN_INVALID_TYPE;
    
    if (exec) return LN_INT_TO_VALUE(!memcmp(ln_bump + LN_VALUE_TO_PTR(ptr_1), ln_bump + LN_VALUE_TO_PTR(ptr_2), LN_VALUE_TO_INT(count)));
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_copy)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) {
      strcpy(ln_bump + LN_VALUE_TO_PTR(ptr_1), ln_bump + LN_VALUE_TO_PTR(ptr_2));
      return strlen(ln_bump + LN_VALUE_TO_PTR(ptr_1));
    }
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_test)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr_1 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_comma);
    
    ln_uint_t ptr_2 = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr_1) != ln_type_pointer || LN_VALUE_TYPE(ptr_2) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) return LN_INT_TO_VALUE(!strcmp(ln_bump + LN_VALUE_TO_PTR(ptr_1), ln_bump + LN_VALUE_TO_PTR(ptr_2)));
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_str_size)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) return LN_INT_TO_VALUE(strlen(ln_bump + LN_VALUE_TO_PTR(ptr)));
    else return LN_NULL;
  } else if (ln_expect(NULL, ln_word_func_eval)) {
    ln_expect(NULL, ln_word_paren_left);
    
    ln_uint_t ptr = ln_eval_expr(exec);
    ln_expect(NULL, ln_word_paren_right);
    
    if (LN_VALUE_TYPE(ptr) != ln_type_pointer) return LN_INVALID_TYPE;
    
    if (exec) {
      const char *old_code = ln_code;
      ln_uint_t old_offset = ln_code_offset;
      
      ln_code = ln_bump + LN_VALUE_TO_PTR(ptr);
      ln_code_offset = 0;
      
      ln_uint_t value = ln_eval(1);
      
      ln_code = old_code;
      ln_code_offset = old_offset;
      
      return value;
    }
    
    return LN_NULL;
  } else if (ln_expect(NULL, ln_word_back)) {
    if (exec) ln_back = 1;
    return ln_return;
  } else if (ln_expect(NULL, ln_word_break)) {
    if (exec) ln_break = 1;
    return ln_return;
  } else if (ln_expect(NULL, ln_word_true)) {
    return LN_INT_TO_VALUE(1);
  } else if (ln_expect(NULL, ln_word_false)) {
    return LN_INT_TO_VALUE(0);
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
  
  if (exec) ln_return = value;
  return value;
}

ln_uint_t ln_eval_stat(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t value = ln_eval_expr(exec);
  
  while (ln_expect(NULL, ln_word_comma)) {
    value = ln_eval_expr(exec);
  }
  
  return ln_return;
}

ln_uint_t ln_eval(int exec) {
  if (ln_back || ln_break) exec = 0;
  ln_uint_t value;
  
  while (!ln_expect(NULL, ln_word_end) && !ln_expect(NULL, ln_word_eof)) {
    value = ln_eval_stat(exec);
  }
  
  return ln_return;
}
