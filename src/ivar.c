#include "base.h"
#include "ast.h"
#include "ir.h"
#include "lex.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  if(argc < 2) {
    fprintf(stderr, "ivar: no filepath specified.\n");
    return 1;
  }

  char* buf = NULL;
  if(readfile(&buf, argv[1]) != 0) return 1;
 
  // Step 1 - Lexing
  struct Lexer lexer;
  if(lexinit(&lexer) != 0) return 1;
  lexlex(&lexer, buf);

  lexprintall(&lexer);

  if(lexer.toks_n == 0) exit(0);

  // Step 2 - Building AST
  struct Parser parser;
  if(parserinit(&parser, lexer.toks, lexer.toks_n) != 0) return 1;
  struct AstNode* astprogram = parserbuildast(&parser);
  
  astprint(astprogram, 0);

  // Step 3 - Semantically analyzing AST
  struct Scope* scope = _calloc(1, sizeof(*scope));
  semanticanalyze(astprogram, scope);
  free(scope);
 
  // Step 4 - IR generation
  struct IRProgram irprogram = {0};
  irprograminit(&irprogram);

  irgen(&irprogram, NULL, astprogram); 

  irprintall(&irprogram);
  return 0;
} 
