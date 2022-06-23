#ifndef __LANN_H__
#define __LANN_H__

// BUILT-IN FUNCTIONS:
// type_of(value) -> 0 = number, 1 = pointer, 2 = error
// size_of(type) -> always returns sizeof(ln_uint_t)
// to_type(value, type)
// get(ptr, index)
// set(ptr, index, value)
// mem_read(ptr)
// mem_write(ptr, byte)
// mem_copy(ptr_1, ptr_2, count)
// mem_test(ptr_1, ptr_2, count)
// str_copy(ptr_1, ptr_2)
// str_test(ptr_1, ptr_2)
// str_size(ptr)

// args -> built-in pointer :D

#include <stdint.h>

#ifndef LN_UINT_TYPE
#define LN_UINT_TYPE uint64_t
#endif

#ifndef LN_INT_TYPE
#define LN_INT_TYPE int64_t
#endif

#ifndef LN_FIXED_DOT
#define LN_FIXED_DOT 24
#endif

#ifndef LN_BUMP_SIZE
#define LN_BUMP_SIZE 4096
#endif

#ifndef LN_CONTEXT_SIZE
#define LN_CONTEXT_SIZE 512
#endif

#define LN_CHAR_DELIM ' '
#define LN_CHAR_EOF   '\0'

#define LN_VALUE_TYPE_RAW(x) ((x) >> (8 * sizeof(x) - 2))
#define LN_VALUE_TYPE(x) (ln_type_match[LN_VALUE_TYPE_RAW((x))])

#define LN_VALUE_SIGN(x)   ((x) >> (8 * sizeof(x) - 2))
#define LN_VALUE_ABS(x)    ((x) & (((ln_uint_t)(1) << (8 * sizeof(x) - 2)) - 1))

#define LN_VALUE_TO_RAW(x)   ((x) & (((ln_uint_t)(1) << (8 * sizeof(x) - 1)) - 1))
#define LN_VALUE_TO_FIXED(x) ((ln_int_t)(LN_VALUE_ABS((x))) * (LN_VALUE_SIGN(LN_VALUE_TO_RAW((x))) ? -1 : 1))
#define LN_VALUE_TO_INT(x)   (LN_VALUE_TO_FIXED((x)) / (1 << LN_FIXED_DOT))

#define LN_FIXED_SIGN(x) ((ln_uint_t)((x) < 0))
#define LN_FIXED_ABS(x)  ((x) < 0 ? -(x) : (x))

#define LN_VALUE_NEGATE(x) (LN_FIXED_TO_VALUE(-LN_VALUE_TO_FIXED((x))))

#define LN_FIXED_TO_VALUE(x) ((ln_uint_t)(LN_FIXED_ABS((x))) | (LN_FIXED_SIGN((x)) << (8 * sizeof(x) - 2)))
#define LN_INT_TO_VALUE(x)   (LN_FIXED_TO_VALUE((ln_int_t)((x)) * (1 << LN_FIXED_DOT)))

#define LN_VALUE_TO_PTR(x) (LN_VALUE_ABS((x)))
#define LN_PTR_TO_VALUE(x) ((x) | ((ln_uint_t)(1) << (8 * sizeof(x) - 1)))

#define LN_VALUE_TO_ERR(x) (LN_VALUE_ABS((x)))
#define LN_ERR_TO_VALUE(x) (LN_PTR_TO_VALUE((x)) | ((ln_uint_t)(1) << (8 * sizeof(x) - 2)))

// note: floating point operations are not recommended, as it requires a software floating point implementation or an FPU, and not everyone has those :p
#define LN_VALUE_TO_FLOAT(x) ((float)(LN_VALUE_TO_FIXED((x))) / (float)(1 << LN_FIXED_DOT)) 
#define LN_FLOAT_TO_VALUE(x) (LN_FIXED_TO_VALUE((x) * (float)(1 << LN_FIXED_DOT)))

#define LN_NULL           (LN_ERR_TO_VALUE((ln_uint_t)(-1)))
#define LN_UNDEFINED      (LN_ERR_TO_VALUE((ln_uint_t)(-2)))
#define LN_DIVIDE_BY_ZERO (LN_ERR_TO_VALUE((ln_uint_t)(-3)))
#define LN_INVALID_TYPE   (LN_ERR_TO_VALUE((ln_uint_t)(-4)))

typedef LN_INT_TYPE  ln_int_t;
typedef LN_UINT_TYPE ln_uint_t;

enum {
  ln_type_number,
  ln_type_pointer,
  ln_type_error,
};

typedef struct ln_word_t ln_word_t;

typedef struct ln_entry_t ln_entry_t;

enum {
  ln_word_eof,
  
  ln_word_begin,
  ln_word_end,
  ln_word_func,
  ln_word_let,
  ln_word_if,
  ln_word_else,
  ln_word_array,
  ln_word_while,
  ln_word_back,
  ln_word_break,
  
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
  ln_word_func_mem_test,
  ln_word_func_str_copy,
  ln_word_func_str_test,
  ln_word_func_str_size,
  
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
};

struct ln_word_t {
  uint8_t type;
  ln_uint_t data; // offsets for strings and names, value for numbers
};

struct ln_entry_t {
  ln_uint_t name, data;
};

extern uint8_t ln_bump[LN_BUMP_SIZE];
extern ln_uint_t ln_bump_offset, ln_bump_args;

extern ln_entry_t ln_context[LN_CONTEXT_SIZE];
extern ln_uint_t ln_context_offset;

extern const char *ln_code;
extern ln_uint_t ln_code_offset;

extern int ln_back, ln_break;

extern const int ln_type_match[];

char ln_upper(char chr);
int  ln_digit(char chr);

int ln_case_equal(const char *str_1, const char *str_2);

ln_uint_t ln_hash(const char *text);
ln_uint_t ln_fixed(const char *text);

ln_uint_t ln_bump_value(ln_uint_t value);
ln_uint_t ln_bump_text(const char *text);

ln_uint_t ln_get_arg(int index);
ln_uint_t ln_cast(ln_uint_t value, int type);

char      ln_read(int in_string);
ln_word_t ln_take(void);
ln_word_t ln_peek(void);
int       ln_expect(ln_word_t *ptr, uint8_t type);
int       ln_expect_2(uint8_t type_1, uint8_t type_2);

#define ln_eval_expr(exec) ln_eval_6(exec)

ln_uint_t ln_eval_0(int exec);
ln_uint_t ln_eval_1(int exec); // (-), !
ln_uint_t ln_eval_2(int exec); // *, /
ln_uint_t ln_eval_3(int exec); // +, -
ln_uint_t ln_eval_4(int exec); // &, |, ^
ln_uint_t ln_eval_5(int exec); // =, !=, >, >=, <, <=
ln_uint_t ln_eval_6(int exec); // and, or, xor

ln_uint_t ln_eval_stat(int exec);
ln_uint_t ln_eval(int exec);

#ifdef LN_HANDLE
extern int ln_func_handle(ln_uint_t *value, ln_uint_t hash);
#endif

#endif
