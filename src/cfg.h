#include <stddef.h>
#include <stdint.h>

#include "ir.h"

struct BasicBlock {

  struct BasicBlock** predecessors, **successors;
  size_t predecessors_n, successors_n;
  size_t predecessors_cap, successors_cap;

  struct BasicBlock** dfs;
  size_t dfs_n, dfs_cap;

  int64_t label;

  size_t id;

  size_t func_idx;

  size_t begin, end;
};


int8_t cfgbuild(const struct IRFunction* func, struct BasicBlock** o_blocks, size_t* o_blocks_n);

void cfgprint(const struct BasicBlock *blocks, const size_t blocks_n);

int8_t cfgpushdf(struct BasicBlock* a, struct BasicBlock *b);
