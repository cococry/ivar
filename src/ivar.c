

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
  IR_CONST,
  IR_DIV,
  IR_MUL,
  IR_SUB,
  IR_ADD
};

const char* irtypetostr(enum IRType type) {
  switch(type) {
    case IR_LOAD: return "IR_LOAD";
    case IR_STORE: return "IR_STORE";
    case IR_CONST: return "IR_CONST";
    case IR_MUL: return "IR_MUL";
    case IR_SUB: return "IR_SUB";
    case IR_DIV: return "IR_DIV";
    case IR_ADD: return "IR_ADD";
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

enum IRType irbinopfromtk(enum TokenType tk) {
  switch(tk) {
    case TK_MUL: return IR_MUL;
    case TK_DIV: return IR_DIV;
    case TK_PLUS: return IR_ADD;
    case TK_MINUS: return IR_SUB;
    default: {
      fprintf(stderr, "ivar: invalid operand '%s', expected +,-,* or /\n",
              lextktostr(tk));
      exit(1);
    }
  }
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

IRValue irgen(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  if(!program || !node) return 1;

  if(node->type == AST_BLOCK || node->type == AST_PROGRAM || node->type == AST_CALL) {
    for(size_t i = 0; i < node->list.childs_n; i++ ){
      irgen(program, func, node->list.childs[i]); 
    }
  }

  else if(node->type == AST_FUNCTION) {
    struct IRFunction* new_func = _calloc(1, sizeof(*new_func));
    assert(new_func);
    irfuncinit(new_func);

    irgen(program, new_func, node->function.body);

    irfuncadd(program, new_func);
  }
  else if(node->type == AST_BINOP) {
    IRValue op1 = irgen(program, func, node->binop.left);
    IRValue op2 = irgen(program, func, node->binop.right);

    IRValue dst = irnextreg(func);

    iremit(func, (struct IRInstruction){
      .type = irbinopfromtk(node->binop.op),
      .dst = dst, 
      .op1 = op1,
      .op2 = op2,
    });

    return dst;
  }
  else if(node->type == AST_NUMBER) {
    IRValue dst = irnextreg(func);

    iremit(func, (struct IRInstruction){
      .type = IR_CONST,
      .imm = node->number,
      .dst = dst
    }); 

    return dst;
  }

  else if (node->type == AST_VAR_DECL) {
    IRValue value = irgen(program, func, node->var_decl.val);

    iremit(func, (struct IRInstruction){
      .type = IR_STORE,
      .name = node->var_decl.name,
      .op1  = value
    });

    return value;  // optional, usually ignored
  }

  else if(node->type == AST_IDENT) {
    IRValue dst = irnextreg(func);

    iremit(func, (struct IRInstruction){
      .type = IR_LOAD,
      .name = node->var_decl.name,
      .dst = dst
    });

    return dst;
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
        case IR_CONST: printf("Instruction: %s: dst: v%li: %li\n", irtypetostr(inst->type), inst->dst, inst->imm); break;
        case IR_LOAD: printf("Instruction: %s: dst: v%li: %s\n", irtypetostr(inst->type), inst->dst, inst->name); break;
        case IR_STORE: printf("Instruction: %s: name: %s in v%li\n", irtypetostr(inst->type), 
                              inst->name,
                              inst->op1
                              ); break;
        case IR_ADD: 
        case IR_DIV: 
        case IR_MUL: 
        case IR_SUB: 
          printf("Instruction: %s: dst: v%li, op1: v%li, op2: v%li\n", irtypetostr(inst->type), 
                              inst->dst, inst->op1, inst->op2); break;
      }
    }
    printf("=========================\n");
  }

  return 0;
} 
