#include "base.h"
#include "ast.h"
#include "ir.h"
#include "lex.h"
#include "cfg.h"

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
  if(semanticanalyze(astprogram, scope) != 0) {
    fprintf(stderr, "ivar: semantic analysis failed to execute properly.\n");
    return 1;
  }
  free(scope);
 
  // Step 4 - IR generation
  struct IRProgram irprogram = {0};
  irprograminit(&irprogram);

  if(irgen(&irprogram, NULL, astprogram) != 0) {
    fprintf(stderr, "ivar: failed to generate IR.\n");
    return 1;
  }
  
  irprintall(&irprogram);

  for(size_t i = 0; i < irprogram.funcs_n; i++) {
    struct IRFunction* func = irprogram.funcs[i];
    printf("====== FUNCTION %li ======\n", i);

    struct BasicBlock* blocks = NULL;
    size_t blocks_n = 0; 
    if(cfgbuild(func, &blocks, &blocks_n) != 0) {
      fprintf(stderr, "ivar: failed to build CFG for function '%li'.\n", i); 
      exit(1);
    } 
    cfgprint(blocks, blocks_n);
    
    printf("=========================\n"); 
  }

  return 0;
} 
