#ifndef __LANN_H__
#define __LANN_H__

#include <stdint.h>

#ifndef LN_UINT_TYPE
#define LN_UINT_TYPE uint64_t
#endif

#ifndef LN_INT_TYPE
#define LN_INT_TYPE int64_t
#endif

#ifndef LN_FIXED_DOT
#define LN_FIXED_DOT 28
#endif

#ifndef LN_PATH
#define LN_PATH "/usr/share/lann/"
#endif

#define LN_CHAR_DELIM ' '
#define LN_CHAR_EOF   '\0'

#define LN_VALUE_TYPE_RAW(x) ((x) >> (8 * sizeof(x) - 2))
#define LN_VALUE_TYPE(x)     (ln_type_match[LN_VALUE_TYPE_RAW((x))])
#define LN_VALUE_SIGN(x)     ((x) >> (8 * sizeof(x) - 2))
#define LN_VALUE_ABS(x)      (LN_FIXED_TO_VALUE(LN_FIXED_ABS(LN_VALUE_TO_FIXED((x)))))
#define LN_VALUE_TO_RAW(x)   ((x) & (((ln_uint_t)(1) << (8 * sizeof(x) - 1)) - 1))
#define LN_VALUE_TO_FIXED(x) ((ln_int_t)((x) | (LN_VALUE_SIGN((x)) << (8 * sizeof(x) - 1))))
#define LN_VALUE_TO_INT(x)   (LN_VALUE_TO_FIXED((x)) / ((ln_int_t)(1) << LN_FIXED_DOT))
#define LN_FIXED_SIGN(x)     ((ln_uint_t)((x)) >> (8 * sizeof(x) - 1))
#define LN_FIXED_ABS(x)      (LN_FIXED_SIGN((x)) ? -(x) : (x))
#define LN_VALUE_NEGATE(x)   (LN_FIXED_TO_VALUE(-LN_VALUE_TO_FIXED((x))))
#define LN_FIXED_TO_VALUE(x) (((ln_uint_t)((x)) & (((ln_uint_t)(1) << (8 * sizeof(x) - 2)) - 1)) | (LN_FIXED_SIGN((x)) << (8 * sizeof(x) - 2)))
#define LN_INT_TO_VALUE(x)   (LN_FIXED_TO_VALUE((ln_int_t)((x)) << LN_FIXED_DOT))
#define LN_VALUE_TO_PTR(x)   ((x) & (((ln_uint_t)(1) << (8 * sizeof(x) - 2)) - 1))
#define LN_PTR_TO_VALUE(x)   ((x) | ((ln_uint_t)(1) << (8 * sizeof(x) - 1)))
#define LN_VALUE_TO_ERR(x)   ((x) & (((ln_uint_t)(1) << (8 * sizeof(x) - 2)) - 1))
#define LN_ERR_TO_VALUE(x)   ((x) | ((ln_uint_t)(3) << (8 * sizeof(x) - 2)))

// warning: do not use
#define LN_VALUE_TO_FLOAT(x) ((float)(LN_VALUE_TO_FIXED((x))) / (float)(1 << LN_FIXED_DOT)) 
#define LN_FLOAT_TO_VALUE(x) (LN_FIXED_TO_VALUE((x) * (float)(1 << LN_FIXED_DOT)))

#define LN_NULL           (LN_ERR_TO_VALUE((ln_uint_t)(-1)))
#define LN_UNDEFINED      (LN_ERR_TO_VALUE((ln_uint_t)(-2)))
#define LN_DIVIDE_BY_ZERO (LN_ERR_TO_VALUE((ln_uint_t)(-3)))
#define LN_INVALID_TYPE   (LN_ERR_TO_VALUE((ln_uint_t)(-4)))
#define LN_OUT_OF_BOUNDS  (LN_ERR_TO_VALUE((ln_uint_t)(-5)))

typedef LN_INT_TYPE  ln_int_t;
typedef LN_UINT_TYPE ln_uint_t;

enum {
  ln_type_number,
  ln_type_pointer,
  ln_type_error,
};

typedef struct ln_word_t ln_word_t;
typedef struct ln_entry_t ln_entry_t;
typedef struct ln_heap_t ln_heap_t;

enum {
  ln_word_eof,
  
  ln_word_block,
  ln_word_begin,
  ln_word_end,
  ln_word_func,
  ln_word_let,
  ln_word_if,
  ln_word_else,
  ln_word_array,
  ln_word_while,
  ln_word_give,
  ln_word_break,
  ln_word_ref,
  
  ln_word_args,
  
  ln_word_type_number,
  ln_word_type_pointer,
  ln_word_type_error,
  
  ln_word_func_type_of,
  ln_word_func_size_of,
  ln_word_func_to_type,
  ln_word_func_get,
  ln_word_func_set,
  ln_word_func_mem_read,
  ln_word_func_mem_write,
  ln_word_func_mem_copy,
  ln_word_func_mem_move,
  ln_word_func_mem_test,
  ln_word_func_mem_alloc,
  ln_word_func_mem_realloc,
  ln_word_func_mem_free,
  ln_word_func_str_copy,
  ln_word_func_str_test,
  ln_word_func_str_size,
  ln_word_func_str_format,
  ln_word_func_eval,
  ln_word_func_import,
  
  ln_word_comma,
  ln_word_paren_left,
  ln_word_paren_right,
  ln_word_plus,
  ln_word_minus,
  ln_word_aster,
  ln_word_slash,
  ln_word_percent,
  ln_word_exclam,
  ln_word_equal,
  ln_word_great,
  ln_word_less,
  ln_word_bit_and,
  ln_word_bit_or,
  ln_word_bit_xor,
  ln_word_bool_and,
  ln_word_bool_or,
  ln_word_bool_xor,
  
  ln_word_number,
  ln_word_name,
  ln_word_string,
  
  ln_word_null,
  ln_word_true,
  ln_word_false,
};

struct ln_word_t {
  uint8_t type;
  
  union {
    ln_uint_t data;
    uint32_t hash;
  };
};

struct ln_entry_t {
  uint32_t name;
  ln_uint_t offset;
};

struct ln_heap_t {
  ln_uint_t size: (sizeof(ln_uint_t) << 3) - 1;
  ln_uint_t free: 1;
  
  uint8_t data[];
};

extern int (*ln_func_handle)(ln_uint_t *, ln_uint_t);
extern int (*ln_import_handle)(ln_uint_t *, const char *);

extern uint8_t *ln_data;
extern ln_uint_t ln_bump_size, ln_bump_offset, ln_bump_args;

extern ln_uint_t ln_heap_size, ln_heap_used;
extern int ln_heap_inited;

extern ln_entry_t *ln_context;
extern ln_uint_t ln_context_size, ln_context_offset;

extern const char *ln_code;
extern ln_uint_t ln_code_offset;

extern ln_word_t ln_last;
extern ln_uint_t ln_last_curr, ln_last_next;

extern int ln_back, ln_break;
extern ln_uint_t ln_return;

extern ln_uint_t ln_words_total, ln_words_saved;

extern const int ln_type_match[];

int ln_check_heap(ln_uint_t offset);
int ln_check(ln_uint_t offset, ln_uint_t size);

uint32_t  ln_hash(const char *text);
ln_uint_t ln_fixed(const char *text);

void ln_init(void *buffer, ln_uint_t size, int (*func_handle)(ln_uint_t *, ln_uint_t), int (*import_handle)(ln_uint_t *, const char *));

void      ln_heap_init(void);
void      ln_heap_defrag(void);
ln_uint_t ln_heap_alloc(ln_uint_t size);
ln_uint_t ln_heap_realloc(ln_uint_t ptr, ln_uint_t new_size);
void      ln_heap_free(ln_uint_t ptr);

ln_uint_t ln_bump_value(ln_uint_t value);
ln_uint_t ln_bump_text(const char *text);
void      ln_bump_text_fixed(ln_int_t fixed, int first);
void      ln_bump_text_int(ln_int_t value, int first);

ln_uint_t ln_get_arg(int index);
ln_uint_t ln_cast(ln_uint_t value, int type);

char      ln_keep(int in_string);
void      ln_skip(char chr, int in_string);
char      ln_read(int in_string);
ln_word_t ln_take(void);
ln_word_t ln_peek(void);
int       ln_expect(ln_word_t *ptr, uint8_t type);
int       ln_expect_multi(ln_word_t *ptr, const int *types);
int       ln_expect_2(uint8_t type_1, uint8_t type_2);

ln_int_t ln_multiply(ln_int_t x, ln_int_t y);
ln_int_t ln_divide(ln_int_t x, ln_int_t y);

ln_uint_t ln_eval_0(int exec);
ln_uint_t ln_eval_1(int exec); // (-), !
ln_uint_t ln_eval_2(int exec); // *, /
ln_uint_t ln_eval_3(int exec); // +, -
ln_uint_t ln_eval_4(int exec); // &, |, ^
ln_uint_t ln_eval_5(int exec); // =, !=, >, >=, <, <=
ln_uint_t ln_eval_6(int exec); // and, or, xor

ln_uint_t ln_eval_expr(int exec);
ln_uint_t ln_eval_stat(int exec);
ln_uint_t ln_eval(int exec);

#endif
