#pragma once

#include "ir.h"
#include <stddef.h>
#include <stdint.h>

struct SSA {
  struct BasicBlock* blocks;
  size_t blocks_n;

  uint64_t** doms;
  size_t words_n;

  struct BasicBlock** idoms;
};

int8_t ssafromtac(struct SSA* ssa, struct BasicBlock* blocks, size_t blocks_n, struct IRFunction* func);
