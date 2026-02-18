#include "ir.h"
#include "ast.h"
#include "lex.h"
#include "base.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

static struct {
  const char *str;
  enum IRType type;
} irstrings[] = {
#define X(name, str) { str, name },
  IR_LIST 
  #undef X
};

static IRValue      irnextreg(struct IRFunction* func);
static IRValue      irnextlabel(struct IRFunction* func);
static enum IRType  irbinopfromtk(enum TokenType tk);
static int8_t       irfuncinit(struct IRFunction* func);
static int8_t       irfuncadd(struct IRProgram* program, struct IRFunction* func);
static int8_t       iremit(struct IRFunction* func, struct IRInstruction inst);

static const char* irtypetostr(enum IRType type) {
  for (size_t i = 0; i < sizeof(irstrings)/sizeof(irstrings[0]); i++) {
    if (irstrings[i].type == type)
      return irstrings[i].str;
  }
  return NULL;
}


IRValue irnextreg(struct IRFunction* func) {
  return func->curreg++; 
}

IRValue irnextlabel(struct IRFunction* func) {
  return func->curlabel++; 
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

IRValue irgenfunc(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(program && node); 

  struct IRFunction* new_func = _calloc(1, sizeof(*new_func));
  assert(new_func);
  irfuncinit(new_func);

  IRValue value = irgen(program, new_func, node->function.body);

  irfuncadd(program, new_func);

  return value; 
}

IRValue irgenbinop(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(program && node && func); 

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

IRValue irgenconst(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(node && func); 

  IRValue dst = irnextreg(func);

  iremit(func, (struct IRInstruction){
    .type = IR_CONST,
    .imm = node->number,
    .dst = dst
  }); 

  return dst;
}

IRValue irgenvardecl(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(program && node && func); 

  IRValue value = irgen(program, func, node->var_decl.val);

  iremit(func, (struct IRInstruction){
    .type = IR_STORE,
    .name = node->var_decl.name,
    .op1  = value
  });

  return value;  // optional, usually ignored
}

IRValue irgenident(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(node && func); 

  IRValue dst = irnextreg(func);

  iremit(func, (struct IRInstruction){
    .type = IR_LOAD,
    .name = node->var_decl.name,
    .dst = dst
  });

  return dst;
}

IRValue irgenif(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  assert(program && node && func); 

  IRValue cond = irgen(program, func, node->ifstmt.cond); 

  IRValue endlabel = irnextlabel(func);
  IRValue elselabel = irnextlabel(func);

  iremit(func, (struct IRInstruction){
    .type = IR_JUMP_IF_FALSE,
    .label = node->ifstmt.elseblock ? elselabel : endlabel, 
    .op1 = cond
  });

  irgen(program, func, node->ifstmt.thenblock);

  if(node->ifstmt.elseblock) {
    iremit(func, (struct IRInstruction){
      .type = IR_JUMP,
      .label = endlabel 
    });
    iremit(func, (struct IRInstruction){
      .type = IR_LABEL,
      .label = elselabel 
    });
    irgen(program, func, node->ifstmt.elseblock);
  }

  iremit(func, (struct IRInstruction){
    .type = IR_LABEL,
    .label = endlabel
  });

  return 0;
}
IRValue irgen(struct IRProgram* program, struct IRFunction* func, struct AstNode* node) {
  if(!program || !node) {
    fprintf(stderr, "ivar: error in IR generation.\n");
    exit(1);
  }

  switch(node->type) {
    case AST_BLOCK:
    case AST_PROGRAM:
    case AST_CALL:
      for(size_t i = 0; i < node->list.childs_n; i++ ){
        irgen(program, func, node->list.childs[i]); 
      }
      break;
    case AST_FUNCTION:  return irgenfunc(program, func, node);
    case AST_BINOP:     return irgenbinop(program, func, node);
    case AST_NUMBER:    return irgenconst(program, func, node);
    case AST_VAR_DECL:  return irgenvardecl(program, func, node);
    case AST_IDENT:     return irgenident(program, func, node);
    case AST_IF:        return irgenif(program, func, node);
  }
  
  return 0;
}

int8_t irprintall(struct IRProgram* program) {
  if(!program) return 1;

  for(uint32_t i = 0; i < program->funcs_n; i++) {
    printf("====== Function %i =======\n", i);
    for(uint32_t j = 0; j < program->funcs[i]->insts_n; j++) {
      struct IRInstruction* inst = &program->funcs[i]->insts[j];
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
        case IR_JUMP_IF_FALSE: 
          printf("Instruction: %s: dst: v%li, label: l%li\n", irtypetostr(inst->type), 
                 inst->op1, inst->label); break;
        case IR_JUMP: 
          printf("Instruction: %s: label: l%li\n", irtypetostr(inst->type), 
                 inst->label); break;
        case IR_LABEL: 
          printf("Instruction: %s: label: l%li\n", irtypetostr(inst->type), 
                 inst->label); break;
      }
    }
    printf("=========================\n");
  }

  return 0;
}

int8_t irprograminit(struct IRProgram* program) {
  if(!program) return 1;
  program->funcs_cap = INIT_FUNCS_PER_PROGRAM; 
  program->funcs = _malloc(sizeof(*program->funcs) * program->funcs_cap);

  return 0;
}

