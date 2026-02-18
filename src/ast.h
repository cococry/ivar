#pragma once

#include <unistd.h>
#include <stdint.h>

#include "lex.h"

enum AstNodeType {
  AST_PROGRAM,
  AST_BLOCK,
  AST_FUNCTION,
  AST_VAR_DECL,
  AST_CALL,
  AST_NUMBER,
  AST_BINOP,
  AST_IDENT
};

struct AstNode {
  enum AstNodeType type;

  struct {
    struct AstNode**  childs;
    size_t            childs_n;
    size_t            childs_cap;
  } list;


  union {
    struct {
      char*           name;
      char*           type;
      struct AstNode* body;
    } function;
    
    struct {
      struct AstNode* left;
      struct AstNode* right;
      enum TokenType op;
    } binop;

    struct {
      char*             name;
      char*             type;
      struct AstNode*   val;
    } var_decl;
    struct {
      char* name;
    } call;

    int64_t number;
    char* ident;
  };
};

struct Scope {
  struct Symbol* syms;
  size_t syms_cap;
  size_t syms_n;
  struct Scope* parent;
};

struct Parser {
  struct Token* toks;
  size_t        toks_n;
  size_t        cur;
};

int8_t            parserinit(struct Parser* parser, struct Token* toks, size_t toks_n);

struct AstNode*   parserbuildast(struct Parser* parser);

uint8_t           semanticanalyze(struct AstNode* node, struct Scope* scope);

void              astprint(struct AstNode* node, int indent);
