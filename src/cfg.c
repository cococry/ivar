#include "cfg.h"
#include "base.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define INIT_EDGE_CAP 16
int8_t cfginitblock(struct BasicBlock* block) {
  if(!block) return 1;

  memset(block, 0, sizeof(*block));

  block->predecessors_cap = INIT_EDGE_CAP;  
  block->successors_cap   = INIT_EDGE_CAP;
  block->dfs_cap          = INIT_EDGE_CAP;
  block->label            = -1;

  block->successors = _malloc(sizeof(*block->successors) * block->successors_cap);
  assert(block->successors);
  block->predecessors = _malloc(sizeof(*block->predecessors) * block->predecessors_cap);
  assert(block->predecessors);
  block->dfs = _malloc(sizeof(*block->dfs) * block->dfs_cap);
  assert(block->dfs);

  assert(block->successors);

  return 0;
}

int8_t cfgaddedge(struct BasicBlock* from,
                  struct BasicBlock* to) {
  assert(from && to);
  if (from->successors_n >= from->successors_cap) {
    from->successors_cap *= 2;

    from->successors = _realloc(
      from->successors,
      sizeof(*from->successors) * from->successors_cap
    );
  }

  if (to->predecessors_n >= to->predecessors_cap) {
    to->predecessors_cap *= 2;

    to->predecessors = _realloc(
      to->predecessors,
      sizeof(*to->predecessors) * to->predecessors_cap
    );
  }

  from->successors[from->successors_n++] = to;
  to->predecessors[to->predecessors_n++] = from;

  return 0;
}



uint8_t cfgfindlabel(const IRValue* labels, const size_t labels_n, const IRValue label) {
  for(size_t i = 0; i < labels_n; i++) { if(labels[i] == label) return 1; }
  return 0;
}

struct BasicBlock* cfgblockbylabel(struct BasicBlock* blocks, size_t blocks_n, IRValue label) {
  for(size_t i = 0; i < blocks_n; i++) { if(blocks[i].label == label) return &blocks[i]; }
  assert(0 && "Label not found in CFG");
  return NULL;
}

int8_t cfgfindleaders(const struct IRFunction* func, size_t** o_leaders_indices, size_t* o_leaders_n) {
  assert(func);
  if(func->insts_n == 0) return 1;

  size_t n_leaders = 0;
  size_t* leaders = _malloc(func->insts_n * sizeof(*leaders));

  assert(leaders);

  // Case 1 Leader: First instruction in function
  leaders[n_leaders++] = 0;

  IRValue referenced_labels[func->insts_n];
  size_t referenced_n = 0;
  for(size_t i = 0; i < func->insts_n; i++) {
    struct IRInstruction* inst = &func->insts[i];

    // Accumulate referenced labels
    if(inst->type == IR_JUMP || inst->type == IR_JUMP_IF_FALSE) {
      referenced_labels[referenced_n++] = inst->label;
    }

    // Case 2 Leader: First instruction after jump instruction 
    else if(i != 0 && 
      (func->insts[i - 1].type == IR_JUMP || func->insts[i - 1].type == IR_JUMP_IF_FALSE)
    ) {
      leaders[n_leaders++] = i;
    }

    // Case 3 Leader: Referenced Label
    else if(inst->type == IR_LABEL && cfgfindlabel(referenced_labels, referenced_n, inst->label)) {
      leaders[n_leaders++] = i;
    }
  }

  *o_leaders_indices = leaders;
  *o_leaders_n = n_leaders;

  return 0;
}


int8_t cfgbuildblocks(
  const struct IRFunction* func,
  const size_t* leaders, struct BasicBlock** o_blocks,
  const size_t leaders_n) {

  assert(leaders);
  if(leaders_n == 0) return 1;

  struct BasicBlock* ret_blocks = _malloc(sizeof(*ret_blocks) * leaders_n);
  assert(ret_blocks);

  for(size_t i = 0; i < leaders_n; i++) {
    cfginitblock(&ret_blocks[i]);

    ret_blocks[i].begin = leaders[i];
    ret_blocks[i].end = i + 1 < leaders_n ? leaders[i + 1] : func->insts_n;
    ret_blocks[i].id = i;


    const struct IRInstruction* firstinst = &func->insts[ret_blocks[i].begin];
    if(firstinst->type == IR_LABEL) {
      ret_blocks[i].label = firstinst->label; 
    }
  }

  *o_blocks = ret_blocks;

  return 0;
}

int8_t cfgmakeedges(const struct IRFunction* func, struct BasicBlock* blocks, size_t blocks_n) {
  assert(func && blocks);

  if(blocks_n == 0) return 1;

  for(size_t i = 0; i < blocks_n; i++) {
    struct BasicBlock* cur = &blocks[i];
    assert(cur->end != 0 && cur->end - 1 < func->insts_n);

    struct IRInstruction lastinst = func->insts[cur->end - 1]; 
    switch(lastinst.type) {
      case IR_JUMP: {
        // Add an edge to the block this block wants to jump to  
        cfgaddedge(cur, cfgblockbylabel(blocks, blocks_n, lastinst.label));
        break;
      }
      case IR_JUMP_IF_FALSE: {
        // Add an edge to the block this block wants to jump to  
        cfgaddedge(cur, cfgblockbylabel(blocks, blocks_n, lastinst.label));
        // Add fallthrough edge 
        if(i + 1 < blocks_n) {
          cfgaddedge(cur, &blocks[i + 1]);
        }
        break;
      }
      default: {
        // Add fallthrough edge 
        if(i + 1 < blocks_n) {
          cfgaddedge(cur, &blocks[i + 1]);
        }
        break;
      } 
    }
  }

  return 0;
}


int8_t cfgbuild(const struct IRFunction* func, struct BasicBlock** o_blocks, size_t* o_blocks_n) {
  assert(func && o_blocks_n);

  size_t* leaders = NULL;
  if(cfgfindleaders(func, &leaders, o_blocks_n) != 0) {
    fprintf(stderr, "ivar: failed to build CFG leaders.\n");
    return 1;
  }
  assert(leaders);

  if(cfgbuildblocks(func, leaders, o_blocks, *o_blocks_n) != 0) {
    fprintf(stderr, "ivar: failed to build CFG blocks.\n");
    return 1;
  }

  if(cfgmakeedges(func, *o_blocks, *o_blocks_n) != 0) {
    fprintf(stderr, "ivar: failed to make CFG edges.\n");
    return 1;
  }

  return 0;
}

void cfgprint(const struct BasicBlock *blocks, const size_t blocks_n) {
  if(!blocks) return;

  printf("========== CFG ==========\n");

  for (size_t i = 0; i < blocks_n; i++) {
    const struct BasicBlock* b = &blocks[i];

    printf("Block %zu\n", b->id);
    
    printf("  Predecessors: ");
    if (b->predecessors_n == 0) {
      printf("(none)");
    } else {
      for (size_t j = 0; j < b->predecessors_n; j++) {
        printf("%zu ", b->predecessors[j]->id);
      }
    }
    printf("\n");

    printf("  Successors:   ");
    if (b->successors_n == 0) {
      printf("(none)");
    } else {
      for (size_t j = 0; j < b->successors_n; j++) {
        printf("%zu ", b->successors[j]->id);
      }
    }
    printf("\n");

    printf("--------------------------\n");
  }

  printf("==========================\n");
}

int8_t 
cfgpushdf(struct BasicBlock* a, struct BasicBlock *b) {
  assert(a && b);

  if(a->dfs_cap == 0) return 1;

  if(a->dfs_n >= a->dfs_cap) {
    a->dfs_cap *= 2;
    a->dfs = _realloc(a->dfs, sizeof(*a->dfs) * a->dfs_cap);
    assert(a->dfs);
  }
  a->dfs[a->dfs_n++] = b;

  return 0;
}
