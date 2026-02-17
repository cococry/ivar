#include "ast.h"
#include "base.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

static struct Token*    parserpeek(struct Parser* parser);
static struct Token*    parserprev(struct Parser* parser);
static uint8_t          parseratend(struct Parser* parser);
static struct Token*    parseradvance(struct Parser* parser);
static uint8_t          parserhave(struct Parser* parser, enum TokenType type);
static struct Token*    parserconsume(struct Parser* parser, enum TokenType type);
static uint8_t          parsermatch(struct Parser* parser, enum TokenType type);

static struct AstNode*  astemitnode(enum AstNodeType type);
static struct AstNode*  astemitfuncnode(char* name, char* type, struct AstNode* body);
static struct AstNode*  astemitvarnode(char* name, char* type, struct AstNode* val);
static struct AstNode*  astemitnumbernode(int64_t number);
static struct AstNode*  astemitidentnode(char* ident);
static int8_t           astaddchild(struct AstNode* parent, struct AstNode* child);
struct AstNode*         astfinishcall(struct Parser* parser, char* name);

struct AstNode* parserparseexpr(struct Parser* parser);

struct AstNode* parserfinishcall(struct Parser* parser, char* name);
struct AstNode* parserparseident(struct Parser* parser);
struct AstNode* parserparsestmt(struct Parser* parser);
struct AstNode* parserparseblock(struct Parser* parser);
struct AstNode* parserparsefunc(struct Parser* parser);

static struct Token* parserpeek(struct Parser* parser) {
  return &parser->toks[parser->cur];
}

struct Token* parserprev(struct Parser* parser) {
  return &parser->toks[parser->cur - 1];
}

uint8_t parseratend(struct Parser* parser) {
  return parser->cur >= parser->toks_n; 
}

struct Token* parseradvance(struct Parser* parser) {
  if(!parseratend(parser)) parser->cur++; 
  return parserprev(parser);
}

uint8_t parserhave(struct Parser* parser, enum TokenType type) {
  if(parseratend(parser)) return 0;
  return parserpeek(parser)->type == type;
}

struct Token* parserconsume(struct Parser* parser, enum TokenType type) {
  if(parserhave(parser, type)) return parseradvance(parser);

  fprintf(stderr, "ivar: expected token %s (got '%s').\n", lextktostr(type), lextktostr(
    parserpeek(parser)->type
  ));
  exit(1);

  return NULL;
}

uint8_t parsermatch(struct Parser* parser, enum TokenType type) {
  if(parserhave(parser, type)) {
    parseradvance(parser);
    return 1;
  }
  return 0;
}

struct AstNode* astemitnode(enum AstNodeType type) {
  struct AstNode* node = _malloc(sizeof(*node));
  assert(node);

  memset(node, 0, sizeof(*node));

  node->type = type;

  return node;
}

struct AstNode* astemitfuncnode(char* name, char* type, struct AstNode* body) {
  struct AstNode* n = astemitnode(AST_FUNCTION);
  if(!n) return NULL;

  n->function.name = name; 
  n->function.type = type;
  n->function.body = body;
  return n;
}
struct AstNode* astemitvarnode(char* name, char* type, struct AstNode* val) {
  struct AstNode* n = astemitnode(AST_VAR_DECL);
  if(!n) return NULL;

  n->var_decl.name = name; 
  n->var_decl.type = type;
  n->var_decl.val = val;
  return n;
}

struct AstNode* astemitnumbernode(int64_t number) {
  struct AstNode* n = astemitnode(AST_NUMBER);
  if(!n) return NULL;

  n->number = number; 
  return n;
}

struct AstNode* astemitidentnode(char* ident) {
  struct AstNode* n = astemitnode(AST_IDENT);
  if(!n) return NULL;

  n->ident = ident; 
  return n;
}

int8_t astaddchild(struct AstNode* parent, struct AstNode* child) {
  if(!parent || !child) return 1;

  if(parent->list.childs_n >= parent->list.childs_cap) {
    uint32_t new_cap = parent->list.childs_cap == 0 ? 2 : parent->list.childs_cap * 2;
    parent->list.childs = _realloc(parent->list.childs, sizeof(*parent->list.childs) * new_cap);
    assert(parent->list.childs);
    parent->list.childs_cap = new_cap;
  }
  parent->list.childs[parent->list.childs_n++] = child;

  return 0;
}

struct AstNode* parserparseexpr(struct Parser* parser) {
  if(!parser) return NULL;

  if(parserhave(parser, TK_NUMBER)) {
    struct Token* num = parserconsume(parser, TK_NUMBER);
    return astemitnumbernode(num->i_val);
  } 
  if(parserhave(parser, TK_IDENT)) {
    struct Token* num = parserconsume(parser, TK_IDENT);
    return astemitidentnode(num->str_val);
  } 
  fprintf(stderr, "ivar: invalid expression.\n");
  exit(1);
  return NULL;
}

struct AstNode* parserfinishcall(struct Parser* parser, char* name) {
  if(!parser || !name) return NULL;

  struct AstNode* call = astemitnode(AST_CALL);
  call->call.name = name; 
  while(!parserhave(parser, TK_RPAREN)) {
    struct AstNode* expr = parserparseexpr(parser);
    if(astaddchild(call, expr) != 0) {
      fprintf(stderr, "ivar: cannot add expression to call.\n");
      exit(1);
    }
    if(parserpeek(parser)->type != TK_RPAREN) {
      parserconsume(parser, TK_COMMA);
    }
  }
  parserconsume(parser, TK_RPAREN);
  return call; 
}

struct AstNode* parserparseident(struct Parser* parser) {
  if(!parser) return NULL;

  struct Token* name = parserconsume(parser, TK_IDENT);

  if(parsermatch(parser, TK_COLON)) {
    struct Token* type = parserconsume(parser, TK_IDENT);
    parserconsume(parser, TK_ASSIGN);
    struct AstNode* val = parserparseexpr(parser); 
    parserconsume(parser, TK_SEMI);
    return astemitvarnode(name->str_val, type->str_val, val);
  }

  if(parsermatch(parser, TK_LPAREN)) {
    struct AstNode* call = parserfinishcall(parser, name->str_val);
    parserconsume(parser, TK_SEMI);
    return call;
  } 

  fprintf(stderr, "ivar: unexpected token after identifier.\n");
  exit(1);
}

struct AstNode* parserparsestmt(struct Parser* parser) {
  if(!parser) return NULL;

  if(parserhave(parser, TK_IDENT)) {
    return parserparseident(parser);
  }
  if(parserhave(parser, TK_LCBRACE)) {
    return parserparseblock(parser);
  } 

  fprintf(stderr, "ivar: unexpected token.\n");
  exit(1);
  return NULL;
}

struct AstNode* parserparseblock(struct Parser* parser) {
  if(!parser) return NULL;

  parserconsume(parser, TK_LCBRACE);

  struct AstNode* block = astemitnode(AST_BLOCK);
  while(!parserhave(parser, TK_RCBRACE)) {
    struct AstNode* stmt = parserparsestmt(parser);
    if(astaddchild(block, stmt) != 0) {
      fprintf(stderr, "ivar: failed to add statement to block.\n");
      exit(1);
    }
  }

  parserconsume(parser, TK_RCBRACE);

  return block;
}

struct AstNode* parserparsefunc(struct Parser* parser) {
  if(!parser) return NULL;

  struct Token* name = parserconsume(parser, TK_IDENT);
  parserconsume(parser, TK_LPAREN);
  parserconsume(parser, TK_RPAREN);
  parserconsume(parser, TK_COLON);

  struct Token* type = parserconsume(parser, TK_IDENT);

  struct AstNode* body = parserparseblock(parser);

  return astemitfuncnode(name->str_val, type->str_val, body);
} 


int8_t parserinit(struct Parser* parser, struct Token* toks, size_t toks_n) {
  if(!parser || !toks) return 1;

  memset(parser, 0, sizeof(*parser));
  parser->toks = toks;
  parser->toks_n = toks_n;

  return 0;
}

struct AstNode* parserbuildast(struct Parser* parser) {
  if(!parser) return NULL;

  struct AstNode* program = astemitnode(AST_PROGRAM);

  while(!parseratend(parser)) {
    struct AstNode* func= parserparsefunc(parser);
    if(astaddchild(program, func) != 0) {
      fprintf(stderr, "ivar: failed to add function to program.\n");
      exit(1);
    }
  }

  return program;
}
