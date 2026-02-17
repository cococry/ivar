#include "lex.h"
#include "base.h"

#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static struct Token*  lexemit(struct Lexer* lexer, enum TokenType type);
static uint8_t        lexdone(struct Lexer* lexer);
static uint8_t        lexappendstr(struct Lexer* lexer, char c); 
static uint8_t        lexisidentchar(char c);
static enum TokenType lexpuncttotk(char c);
static enum TokenType lexkeywordtotok(struct Lexer* lexer, const char* keyword);
static uint8_t        lexappend(struct Lexer* lexer, char c);
static uint8_t        lexstartnum(struct Lexer* lexer, char c);
static uint8_t        lexstartident(struct Lexer* lexer, char c);

static const char*    tktostr(enum TokenType type);

struct {
  const char *str;
  enum TokenType type;
} keywords[] = {
#define X(name, str) { str, name },
  KEYWORD_LIST
  #undef X
};

struct Token* lexemit(struct Lexer* lexer,enum TokenType type) {
  if(!lexer) return NULL;
  if(lexer->toks_n >=  lexer->toks_cap - 1) {
    lexer->toks_cap *= 2;
    lexer->toks = _realloc(lexer->toks, sizeof(*lexer->toks) * lexer->toks_cap);
    assert(lexer->toks);
  }
  lexer->toks[lexer->toks_n] = (struct Token){
    .type     = type,
    .i_val    = type == TK_NUMBER ? lexer->cur_num          : 0,
    .str_val  = type == TK_IDENT  ? strdup(lexer->cur_str)  : NULL
  };
  return &lexer->toks[lexer->toks_n++];
}

uint8_t lexdone(struct Lexer* lexer) {
  switch(lexer->state) {
    case LX_ON_IDENT: { 
      if(lexer->cur_ptr >= MAX_IDENT_LEN - 1) return 1;
      lexer->cur_str[lexer->cur_ptr] = '\0';

      enum TokenType emit = lexkeywordtotok(lexer, lexer->cur_str); 

      lexer->cur_ptr = 0;
      lexer->state = LX_IDLE;
      struct Token* tk = lexemit(lexer, emit);
      if(!tk) {
        fprintf(stderr, "ivar: failed to emit token '%s'.\n", tktostr(emit));
        return 1;
      }

      printf("Emitted: %s: '%s'\n", tktostr(tk->type), tk->str_val); 
      break;
    }
    case LX_ON_NUM: {
      struct Token* tk = lexemit(lexer, TK_NUMBER);
      if(!tk) {
        fprintf(stderr, "ivar: failed to emit token '%s'.\n", tktostr(TK_NUMBER));
        return 1;
      }

      lexer->state = LX_IDLE;
      lexer->cur_num = 0;
      printf("Emitted: %s: %li\n", tktostr(tk->type), tk->i_val); 

      break;
    }
    default:
      break;
  }
  return 0;
}

uint8_t lexappendstr(struct Lexer* lexer, char c) {
  if(!lexer) return 1;

  if(lexer->cur_ptr >= MAX_IDENT_LEN - 1) return 1;
  lexer->cur_str[lexer->cur_ptr++] = c;
  return 0;
}

uint8_t lexisidentchar(char c) {
  return isalpha(c) || c == '_';
}

enum TokenType lexpuncttotk(char c) {
  switch (c) {
    case '{': return TK_LCBRACE;
    case '}': return TK_RCBRACE;
    case '(': return TK_LPAREN;
    case ')': return TK_RPAREN;
    case ';': return TK_SEMI;
    case ':': return TK_COLON;
    default:  return TK_NONE;
  }
}

enum TokenType lexkeywordtotok(struct Lexer* lexer, const char* keyword) {
  enum TokenType type = TK_IDENT;

  for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++) {
    if (strcmp(lexer->cur_str, keywords[i].str) == 0) {
      type = keywords[i].type;
      break;
    }
  }
  return type;
}

uint8_t lexappend(struct Lexer* lexer, char c) {
  if(!lexer) return 1;

  if(lexappendstr(lexer, c) != 0) {
    lexer->cur_str[MAX_IDENT_LEN - 1] = '\0';
    fprintf(
      stderr, 
      "ivar: identifier %s is too long," 
      "identifiers may not exceed %i characters in length.\n", 
      lexer->cur_str, MAX_IDENT_LEN);

    return 1;
  }
  return 0;
}

uint8_t lexstartnum(struct Lexer* lexer, char c) {
  if(!lexer) return 1;

  lexer->state = LX_ON_NUM;
  lexer->cur_num = lexer->cur_num * 10 + (c - '0');

  return 0;
}

uint8_t lexstartident(struct Lexer* lexer, char c) {
  if(!lexer) return 1;

  lexer->state = LX_ON_IDENT;
  if(lexappend(lexer, c) != 0) return 1;

  return 0;
}

const char *tktostr(enum TokenType type) {
  switch (type) {
#define X(name, str) case name: return str;
    TOKEN_LIST
    KEYWORD_LIST
    #undef X
    default: return "UNKNOWN_TOKEN";
  }
}

// ======= PUBLIC API ========

uint8_t lexinit(struct Lexer* lexer) {
  if(!lexer) return 1;

  memset(lexer, 0, sizeof(*lexer));
  memset(lexer->cur_str, 0, sizeof(lexer->cur_str));

  lexer->toks     = _malloc(sizeof(*lexer->toks) * LEX_TOK_INIT);
  assert(lexer->toks);

  lexer->toks_cap = LEX_TOK_INIT;
  lexer->state    = LX_IDLE;

  return 0;
}
uint8_t lexlex(struct Lexer* lexer, char* source) {
  char* p = source;
  while(*p) {
    switch(lexer->state) {
      case LX_IDLE: 
        if(lexisidentchar(*p)) {
          if(lexstartident(lexer, *p) != 0) return 1;
        } else if(isdigit(*p)) {
          if(lexstartnum(lexer, *p)   != 0) return 1;
        } else if(lexpuncttotk(*p) != TK_NONE) {
          lexemit(lexer, lexpuncttotk(*p));
        }
        break;

      case LX_ON_IDENT: 
        if(lexisidentchar(*p) || isdigit(*p)) {
          if(lexappend(lexer, *p) != 0) return 1;
        } else {
          lexdone(lexer);
          continue;
        }
        break;

      case LX_ON_NUM: 
        if(isdigit(*p)) {
          lexer->cur_num = lexer->cur_num * 10 + (*p - '0');
        } else {
          lexdone(lexer);
          continue;
        }
        break;
    }
    p++;
  }

  return 0;
}
