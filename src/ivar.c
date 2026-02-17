

#include "base.h"
#include "ast.h"
#include "lex.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void astprintindent(int indent) {
  for (int i = 0; i < indent; i++)
    printf("  ");
}

void astprint(struct AstNode* node, int indent) {
  if (!node) return;

  astprintindent(indent);

  switch (node->type) {

    case AST_FUNCTION:
      printf("Function: %s -> %s\n",
             node->function.name,
             node->function.type);

      astprint(node->function.body, indent + 1);
      break;

    case AST_VAR_DECL:
      printf("VarDecl: %s : %s\n",
             node->var_decl.name,
             node->var_decl.type);

      astprint(node->var_decl.val, indent + 1);
      break;

    case AST_BLOCK:
    case AST_CALL:
    case AST_PROGRAM:
      printf("Block/List: %s\n", node->type == AST_CALL ? node->call.name : (node->type == AST_FUNCTION ? node->function.name : "block/program"));
      for (size_t i = 0; i < node->list.childs_n; i++) {
        astprint(node->list.childs[i], indent + 1);
      }
      break;


    case AST_NUMBER:
      printf("Number: %lld\n", (long long)node->number);
      break;

    case AST_IDENT:
      printf("Ident: %s\n", node->ident);
      break;

    default:
      printf("Unknown AST Node\n");
      break;
  }
}

int main(int argc, char** argv) {
  if(argc < 2) {
    fprintf(stderr, "ivar: no filepath specified.\n");
    return 1;
  }

  char* buf = NULL;
  if(readfile(&buf, "examples/01.iv") != 0) return 1;

  struct Lexer lexer;
  if(lexinit(&lexer) != 0) return 1;
  lexlex(&lexer, buf);

  if(lexer.toks_n == 0) exit(0);

  struct Parser parser;
  if(parserinit(&parser, lexer.toks, lexer.toks_n) != 0) return 1;
  struct AstNode* program = parserbuildast(&parser);

  astprint(program, 0);

  return 0;
} 
