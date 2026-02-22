#pragma once

#include "ast.h"

#include <stdint.h>
#include <stddef.h>

#define INIT_INSTS_PER_FUNC 16
#define INIT_FUNCS_PER_PROGRAM 8

#define IR_LIST \
  X(IR_LOAD, "IR_LOAD") \
  X(IR_STORE, "IR_STORE") \
  X(IR_CONST, "IR_CONST") \
  X(IR_DIV, "IR_DIV") \
  X(IR_MUL, "IR_MUL") \
  X(IR_SUB, "IR_SUB") \
  X(IR_ADD, "IR_ADD") \
  X(IR_JUMP_IF_FALSE, "IR_JUMP_IF_FALSE") \
  X(IR_JUMP, "IR_JUMP") \
  X(IR_ASSIGN, "IR_ASSIGN") \
  X(IR_LABEL, "IR_LABEL") \
  X(IR_PHI, "IR_PHI") \

enum IRType {
  #define X(name, str) name,
  IR_LIST 
  #undef X
};

typedef int64_t IRValue;

struct IRInstruction {
  enum IRType type;

  IRValue op1, op2, dst; 
  IRValue imm;
  char* name;

  struct {
    char* result; 
    char** args;
    int args_n;

    struct BasicBlock** phi_preds; 
  } phi;

  IRValue label;
};

struct IRFunction {
  struct IRInstruction* insts;
  size_t insts_n, insts_cap;

  IRValue curreg, curlabel;

  size_t idx;
};

struct IRProgram {
  struct IRFunction** funcs;
  size_t funcs_n, funcs_cap;
};

IRValue irgen(struct IRProgram* program, struct IRFunction* func, struct AstNode* node);

int8_t irprintall(struct IRProgram* program);

int8_t irprintinst(struct IRInstruction* inst); 

int8_t irprograminit(struct IRProgram* program);

int8_t irinstinsertat(struct IRFunction* func, struct IRInstruction inst, size_t idx);
