

#include "base.h"
#include "ast.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_INSTS_PER_FUNC 16
#define INIT_FUNCS_PER_PROGRAM 8

enum IRType {
  IR_LOAD,
  IR_STORE,
  IR_CONST
};

const char* irtypetostr(enum IRType type) {
  switch(type) {
    case IR_LOAD: return "IR_LOAD";
    case IR_STORE: return "IR_STORE";
    case IR_CONST: return "IR_CONST";
  };
}
typedef int64_t IRValue;

struct IRInstruction {
  enum IRType type;

  IRValue op1, op2, dst; 
  IRValue imm;
  char* name;
};

struct IRFunction {
  struct IRInstruction* insts;
  size_t insts_n, insts_cap;

  size_t curreg;
};

struct IRProgram {
  struct IRFunction** funcs;
  size_t funcs_n, funcs_cap;
};

IRValue irnextreg(struct IRFunction* func) {
  return func->curreg++; 
}

int8_t irfuncinit(struct IRFunction* func) {
  if(!func) return 1;
  func->insts_cap = INIT_INSTS_PER_FUNC; 
  func->insts = _malloc(sizeof(*func->insts) * func->insts_cap);

  return 0;
}

int8_t irprograminit(struct IRProgram* program) {
  if(!program) return 1;
  program->funcs_cap = INIT_FUNCS_PER_PROGRAM; 
  program->funcs = _malloc(sizeof(*program->funcs) * program->funcs_cap);

  return 0;
}

int8_t iremit(struct IRFunction* func, struct IRInstruction inst) {
  if(!func) return 1;
  if(func->insts_n >= func->insts_cap) {
    func->insts_cap *= 2;
    func->insts = _realloc(func->insts, func->insts_cap * sizeof(*func->insts));
    assert(func->insts);
  }

  func->insts[func->insts_n++] = inst;

  return 0;
}

int8_t irfuncadd(struct IRProgram* program, struct IRFunction* func) {
  if(!program || !func) return 1;

  if(program->funcs_n >= program->funcs_cap) {
    program->funcs_cap *= 2;
    program->funcs = _realloc(program->funcs, program->funcs_cap * sizeof(*program->funcs));
    assert(program->funcs);
  }
  program->funcs[program->funcs_n++] = func;

  return 0;
}

int8_t irgen(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  if(!program || !node) return 1;

  if(node->type == AST_BLOCK || node->type == AST_PROGRAM || node->type == AST_CALL) {
    for(size_t i = 0; i < node->list.childs_n; i++ ){
      irgen(program, func, node->list.childs[i]); 
    }
  }

  if(node->type == AST_FUNCTION) {
    struct IRFunction* new_func = _calloc(1, sizeof(*new_func));
    assert(new_func);
    irfuncinit(new_func);

    irgen(program, new_func, node->function.body);

    irfuncadd(program, new_func);
  }
  if(node->type == AST_NUMBER) {
    iremit(func, (struct IRInstruction){
      .type = IR_CONST,
      .imm = node->number
    }); 
  }

  if(node->type == AST_VAR_DECL) {
    irgen(program, func, node->var_decl.val);

    IRValue dst = irnextreg(func);

    iremit(func, (struct IRInstruction){
      .type = IR_STORE,
      .dst = dst,
      .name = node->var_decl.name 
    });
  }

  if(node->type == AST_IDENT) {
    iremit(func, (struct IRInstruction){
      .type = IR_LOAD,
      .name = node->var_decl.name 
    });
  }

  return 0;
}

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

  for(uint32_t i = 0; i < irprogram.funcs_n; i++) {
    printf("====== Function %i =======\n", i);
    for(uint32_t j = 0; j < irprogram.funcs[i]->insts_n; j++) {
      struct IRInstruction* inst = &irprogram.funcs[i]->insts[j];
      switch(inst->type) {
        case IR_CONST: printf("Instruction: %s: %li\n", irtypetostr(inst->type), inst->imm); break;
        case IR_LOAD: printf("Instruction: %s: %s\n", irtypetostr(inst->type), inst->name); break;
        case IR_STORE: printf("Instruction: %s: dst: %li, name: %s\n", irtypetostr(inst->type), 
                              inst->dst, inst->name); break;
      }
    }
    printf("=========================\n");
  }

  return 0;
} 
