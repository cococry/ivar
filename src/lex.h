#include <stdint.h>
#include <unistd.h>

#define MAX_IDENT_LEN 1024
#define LEX_TOK_INIT 128 

#define TOKEN_LIST \
    X(TK_NONE,    "NONE") \
    X(TK_IDENT,   "IDENT") \
    X(TK_NUMBER,  "NUMBER") \
    X(TK_LPAREN,  "(") \
    X(TK_RPAREN,  ")") \
    X(TK_LCBRACE, "{") \
    X(TK_RCBRACE, "}") \
    X(TK_COLON,   ":") \
    X(TK_SEMI,    ";")

#define KEYWORD_LIST \
    X(TK_IF,    "if") \
    X(TK_ELSE,  "else") \
    X(TK_WHILE, "while") \
    X(TK_FOR,   "for") \
    X(TK_I8,    "i8") \
    X(TK_I16,   "i16") \
    X(TK_I32,   "i32") \
    X(TK_I64,   "i64") \
    X(TK_U8,    "u8") \
    X(TK_U16,   "u16") \
    X(TK_U32,   "u32") \
    X(TK_U64,   "u64")

enum TokenType {
  #define X(name, str) name,
  TOKEN_LIST
  KEYWORD_LIST
  #undef X
};

enum LexerState {
  LX_IDLE = 0,
  LX_ON_NUM, 
  LX_ON_IDENT,
};

struct Token {
  int64_t         i_val;
  char*           str_val;
  enum TokenType  type;
};

struct Lexer {
  struct Token*   toks;
  size_t          toks_n;
  size_t          toks_cap;
  char            cur_str[MAX_IDENT_LEN];
  int64_t         cur_num;
  size_t          cur_ptr;
  enum LexerState state;
};

uint8_t        lexinit(struct Lexer* lexer);
uint8_t        lexlex(struct Lexer* lexer, char* source);

