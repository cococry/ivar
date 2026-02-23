#include "ssa.h"
#include "base.h"
#include "cfg.h"

#define STB_DS_IMPLEMENTATION
#include "../vendor/stb_ds.h"
#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DIV_UP(x, y) (((x) + (y) - 1) / (y))

#define arrtop(arr) (arr)[arrlen((arr)) > 0 ? arrlen((arr)) - 1 : arrlen(arr)]

struct Varstack {
  char** names;
  size_t counter;
};

struct VarstackMap {
  char* key;
  struct Varstack* value;
};

struct SSAVar {
  uint64_t id;
};

struct BlockEntryById {
  size_t key; 
  int value; // unused
};

typedef struct {
    void* key;
    int value;
} BlockSet;

struct DefsiteEntry {
  char* key;
  BlockSet* value; 
};

static int8_t           ssainit(struct SSA* ssa, struct BasicBlock* blocks, size_t blocks_n);
static int8_t           ssafinddominators(const struct BasicBlock* blocks, size_t blocks_n, size_t words_n, uint64_t** dominators);
static int8_t           ssaallocatedominatorwords(const size_t blocks_n, uint64_t*** o_dominators, size_t* words_n);
static int8_t           ssafindidoms(struct SSA* ssa);
struct BasicBlock*      ssaidom(struct SSA* ssa, const struct BasicBlock* b);
static uint8_t          ssadominates(struct SSA* ssa, const struct BasicBlock* a, const struct BasicBlock* b);
static int8_t           ssadominatorsof(struct SSA* ssa, const struct BasicBlock* a, uint64_t** o_dominators, size_t* o_dominators_n);
static int8_t           ssabuilddomtree(struct SSA* ssa);
static int8_t           ssainsertphinodes(struct SSA* ssa, struct IRFunction* func);
static int8_t           ssagetvardefsites(struct SSA* ssa, struct IRFunction* func, struct DefsiteEntry** o_defsites);
static int8_t           ssainsertinst(struct SSA* ssa, struct BasicBlock* b, struct IRInstruction inst, struct IRFunction* func);
static int8_t           ssainsertphinode(struct SSA* ssa, const char* result, struct BasicBlock* df, struct IRFunction* func);
static struct Varstack* getstack(struct VarstackMap** map, char* var);
static int8_t           ssarenameblock(struct SSA* ssa, struct BasicBlock* block, struct IRFunction* func, struct VarstackMap** map);
static int8_t           ssagetdominancefrontiers(struct SSA* ssa);

int8_t ssainit(struct SSA* ssa, struct BasicBlock* blocks, size_t blocks_n) {
  assert(ssa && blocks);

  memset(ssa, 0, sizeof(*ssa));

  ssa->blocks_n = blocks_n;
  ssa->blocks = blocks;

  if(ssaallocatedominatorwords(ssa->blocks_n, &ssa->doms, &ssa->words_n) != 0) {
    fprintf(stderr, "ivar: failed to allocate SSA dominator words.\n");
    return 1;
  }

  ssa->idoms = _malloc(sizeof(*ssa->idoms) * blocks_n);

  if(ssafinddominators(blocks, blocks_n, ssa->words_n, ssa->doms) != 0) {
    fprintf(stderr, "ivar: failed to find dominators for SSA.\n");
    return 1;
  }

  if(ssafindidoms(ssa) != 0) {
    fprintf(stderr, "ivar: failed to find immidiate dominators for SSA.\n");
    return 1;
  }
    
  if(ssabuilddomtree(ssa) != 0) {
    fprintf(stderr, "ivar: failed to build dominator tree for SSA.\n");
    return 1;
  }
    
  if(ssagetdominancefrontiers(ssa) != 0) {
    fprintf(stderr, "ivar: failed to get dominance frontiers for SSA.\n");
    return 1;
  }

  return 0;
}

int8_t ssaallocatedominatorwords(const size_t blocks_n, uint64_t*** o_dominators, size_t* words_n) {
  *words_n = DIV_UP(blocks_n, 64); 
  *o_dominators = _malloc(sizeof(uint64_t*) * blocks_n);
  assert(*o_dominators);

  for(size_t i = 0; i < blocks_n; i++) {
    (*o_dominators)[i] = _malloc(sizeof(uint64_t) * (*words_n));
    assert((*o_dominators)[i]);
  }

  return 0;
}

int8_t ssafinddominators(const struct BasicBlock* blocks, size_t blocks_n, size_t words_n, uint64_t** dominators) {  
  assert(dominators && blocks);

  const size_t entry_id = 0;
  for(size_t i = 0; i < blocks_n; i++) {
    if(blocks[i].id == entry_id) {
      for(size_t w = 0; w < words_n; w++) 
        dominators[i][w] = 1ULL << i;
    } else {
      for(size_t w = 0; w < words_n; w++) 
        dominators[i][w] = (1ULL << blocks_n) - 1;
    }
  }

  uint8_t changed = 1;
  uint64_t* newdom = _malloc(sizeof(*newdom) * words_n); 
  assert(newdom);

  while(changed) {
    changed = 0;

    for(size_t i = 0; i < blocks_n; i++) {
      size_t id = blocks[i].id;
      if(id == entry_id) continue;

      // 1. assume all other blocks dominate this block
      for(size_t w = 0; w < words_n; w++) {
        newdom[w] = ~0LL;
      }

      // 2. keep only the blocks that strictly dominate ALL predecessors
      // of this block
      for(size_t j = 0; j < blocks[i].predecessors_n; j++) {
        size_t pred = blocks[i].predecessors[j]->id;
        for(size_t w = 0; w < words_n; w++) 
          newdom[w] &= dominators[pred][w];
      }

      // 3. add self to list of dominators
      size_t word = id / 64;
      size_t bit = id % 64;
      newdom[word] |= (1ULL << bit);
      
      // add to dominator list and check if we stabilized 
      for(size_t w = 0; w < words_n; w++) {
        if(newdom[w] != dominators[id][w]) {

          dominators[id][w] = newdom[w];
          changed = 1;
        }
      }
    }
  }
  free(newdom);

  return 0;
}

int8_t ssafindidoms(struct SSA* ssa) {
  assert(ssa && ssa->idoms);
  for(size_t i = 0;  i < ssa->blocks_n; i++) {
      struct BasicBlock* b = &ssa->blocks[i];
      assert(b->id < ssa->blocks_n);
      ssa->idoms[b->id] = ssaidom(ssa, &ssa->blocks[i]); 
  }
  return 0;
} 

struct BasicBlock* ssaidom(struct SSA* ssa, const struct BasicBlock* b) {
  assert(ssa->blocks && b && ssa->doms);

  for(size_t i = 0; i < ssa->blocks_n; i++) {
    struct BasicBlock* a = &ssa->blocks[i];

    // A connot be the block itself
    if(a == b) continue;

    // Finding an immidiate dominator A of B:
    // 1. A dominates B
    if(!ssadominates(ssa, a, b)) continue;

    uint64_t* dominatorsofb = NULL;
    size_t dominatorsofb_n = 0;
    if(ssadominatorsof(ssa, b, &dominatorsofb, &dominatorsofb_n) != 0) {
      fprintf(stderr, "ivar: cannot get dominators of block %li", b->id);
      exit(1);
      return 0;
    }

    // 2. There is no other node C such that C dominates B, and 
    // C is strictly between A and B in the dominance relation. 
    uint8_t has_between = 0;
    for(size_t j = 0; j < dominatorsofb_n; j++) {
      // ...Such that c dominates b 
      const struct BasicBlock* c = &ssa->blocks[dominatorsofb[j]];
      // ...And C is strictly between A and B in the dominance relation. 
      if(c != a && c != b && ssadominates(ssa, a, c)) {
        has_between = 1;
        break;
      }
    }

    if(!has_between) {
      return a;
    }
  }

  return NULL;
}

uint8_t ssadominates(struct SSA* ssa, const struct BasicBlock* a, const struct BasicBlock* b) {
  for (size_t w = 0; w < ssa->words_n; w++) {
    if (ssa->doms[b->id][w] & (1ULL << a->id)) { 
      return 1;
    }
  }
  return 0;
}

int8_t ssadominatorsof(
  struct SSA* ssa,
  const struct BasicBlock* a, 
  uint64_t** o_dominators, size_t* o_dominators_n) {
  assert(ssa->blocks && a && ssa->doms && o_dominators_n);

  if(a->id >= ssa->blocks_n) return 1;

  uint64_t* mydoms = _malloc(sizeof(*mydoms) * ssa->blocks_n);
  assert(mydoms);

  size_t doms_n = 0;
  for (size_t block = 0; block < ssa->blocks_n; block++) {

    size_t word = block / 64;
    size_t bit  = block % 64;

    if (ssa->doms[a->id][word] & (1ULL << bit)) {
      mydoms[doms_n++] = ssa->blocks[block].id; 
    }
  }

  *o_dominators = mydoms;
  *o_dominators_n = doms_n;

  return 0;
}

int8_t ssabuilddomtree(struct SSA* ssa) {
  assert(ssa);

  for(size_t i = 0; i < ssa->blocks_n; i++) {
    struct BasicBlock* b = &ssa->blocks[i];
      struct BasicBlock* parent = ssa->idoms[b->id];
    if(parent) {
      arrput(parent->domchilds, b); 
    }
  }
  return 0;
}

int8_t ssainsertphinodes(struct SSA* ssa, struct IRFunction* func) {
  struct DefsiteEntry* defsites = NULL;
  ssagetvardefsites(ssa, func, &defsites);
  if(!defsites) return 0;

  for(size_t i = 0; i < hmlen(defsites); i++) {
    // initialize worklist 
    struct BasicBlock** worklist = NULL;
    for (size_t b = 0; b < hmlen(defsites[i].value); b++) {
      arrput(worklist, defsites[i].value[b].key); 
    }

    // insert phi nodes to all dominance frontiers of the blocks 
    // that define the variable we currently iterate.
    uint8_t* phisinserted = calloc(ssa->blocks_n, 1);
    while(arrlen(worklist) > 0) {
      struct BasicBlock* b = arrpop(worklist); 

      // for each dominance frontier .. insert phi node at the top 
      // if a phi node does not exist yet.
      for(size_t j = 0; j < b->dfs_n; j++) {
        struct BasicBlock* df = b->dfs[j];
        if(!phisinserted[df->id]) {
          phisinserted[df->id] = 1;

          ssainsertphinode(ssa, defsites[i].key, df, func);

          if(!hmget(defsites[i].value, df)) {
            arrput(worklist, df);
          }
        }
      }
    }

    free(phisinserted);
    arrfree(worklist);
  }

  return 0;
}

int8_t ssagetvardefsites(struct SSA* ssa, struct IRFunction* func, struct DefsiteEntry** o_defsites) {
  assert(ssa && func);

  for(size_t i = 0; i < ssa->blocks_n; i++) {
    // iterate every block and find assign instructions
    for(size_t j = ssa->blocks[i].begin; j < ssa->blocks[i].end; j++) {
      struct IRInstruction inst = func->insts[j];
      if(inst.type == IR_ASSIGN) {
        struct DefsiteEntry* entry = hmgetp_null(*o_defsites, inst.name);
        if (!entry) {
          // NULL to init the array 
          BlockSet* new = NULL; 
          hmput(*o_defsites, inst.name, new);
          entry = hmgetp(*o_defsites, inst.name);
        }
        hmput(entry->value, &ssa->blocks[i], 1);
      }
    }
  }

  return 0;
}

int8_t ssainsertinst(struct SSA* ssa, struct BasicBlock* b, struct IRInstruction inst, struct IRFunction* func) {
  if(irinstinsertat(func, inst, b->begin) != 0) return 1;

  // shift the window of all other blocks after the insert
  for (size_t i = 0; i < ssa->blocks_n; i++) {
    if (ssa->blocks[i].begin > b->begin)
      ssa->blocks[i].begin++;

    if (ssa->blocks[i].end > b->begin)
      ssa->blocks[i].end++;
  }

  return 0;
}

int8_t ssainsertphinode(struct SSA* ssa, const char* result, struct BasicBlock* df, struct IRFunction* func) {
  assert(ssa && result && df && func);

  struct IRInstruction phi = {0};
  phi.type = IR_PHI;
  phi.phi.result = strdup(result);
  assert(phi.phi.result);
  phi.phi.args = NULL;

  if(ssainsertinst(ssa, df, phi, func) != 0) return 1;

  return 0;
}

char* ssavarstacknewver(struct Varstack** stack, const char* var) {
  const size_t maxdigits = 32;
  size_t n = strlen(var) + 1 + maxdigits;
  char* buf = _malloc(n);
  assert(buf);

  snprintf(buf, n, "%s%li", var, (*stack)->counter++);

  return buf;
};


struct Varstack* getstack(struct VarstackMap** map, char* var) {
  struct Varstack* stack = shget(*map, var);
  if(!stack) {
    stack = _calloc(1, sizeof(*stack));
    assert(stack);
    shput(*map, var, stack);
  } 
  return stack;
}

int8_t ssarenameblock(struct SSA* ssa, struct BasicBlock* block, struct IRFunction* func, struct VarstackMap** map) {
  assert(ssa && func);

  if(!block) {
    return 0;
  }

  char** definedhere = NULL;
  for(size_t i = block->begin; i < block->end; i++) {
    struct IRInstruction* inst = &func->insts[i]; 
    if(inst->type == IR_PHI) {
      char* var = inst->phi.result;  

      struct Varstack* stack = getstack(map, var);
      char* newver = ssavarstacknewver(&stack, var);

      arrput(stack->names, newver);
      arrput(definedhere, var); 

      inst->phi.resultversioned = newver;
    }
  }
  for(size_t i = block->begin; i < block->end; i++) {
    struct IRInstruction* inst = &func->insts[i]; 
    if(inst->type == IR_LOAD) {
      char* var = inst->name;
      struct Varstack* stack = getstack(map, var);
      if(stack->names)
        inst->nameversioned = arrtop(stack->names);
    }
    else if(inst->type == IR_ASSIGN || inst->type == IR_STORE) {
      char* var = inst->name;

      struct Varstack* stack = getstack(map, var);
      char* newver = ssavarstacknewver(&stack, var);

      arrput(stack->names, newver);
      arrput(definedhere, var); 

      inst->nameversioned = newver;
    }
  }

  for(size_t i = 0; i < block->successors_n; i++) {
    struct BasicBlock* s = block->successors[i];
    for(size_t j = s->begin; j < s->end; j++) {
    struct IRInstruction* inst = &func->insts[j]; 
      if(inst->type == IR_PHI) {
        struct Varstack* origstack = getstack(map, inst->phi.result);
        if(origstack->names) {
          char* namefromblock = arrtop(origstack->names);

          hmput(inst->phi.args, block, namefromblock);
        }
      }
    }
  }
 
  for(size_t i = 0; i < arrlen(block->domchilds); i++) {
    ssarenameblock(ssa, block->domchilds[i], func, map);
  }

  for(size_t i = 0; i < arrlen(definedhere); i++) {
    struct Varstack* stack = shget(*map, definedhere[i]);
    arrpop(stack->names);
  } 
  
  arrfree(definedhere);

  return 0;
}

int8_t ssagetdominancefrontiers(struct SSA* ssa) {
  assert(ssa->blocks && ssa->doms);

  for(size_t i = 0; i < ssa->blocks_n; i++) {
    struct BasicBlock* b = &ssa->blocks[i];

    // only blocks that are merge points can have dominance "leaks" 
    // somewhere up the tree.
    if(b->predecessors_n >= 2) {
      for(size_t j = 0; j < b->predecessors_n; j++) {
        struct BasicBlock* runner = b->predecessors[j];
        // walk up the dominance hierarchy until the point where dominance 
        // "leaks". that point is the immidiate dominator of B. 
        //
        // the blocks where this block had dominance are added to the DF list of 
        // all blocks that we passed during the walk.
        while(runner && runner != ssa->idoms[b->id]) {
          cfgpushdf(runner, b);
          runner = ssa->idoms[runner->id]; 
        }
      }
    }
  }
  return 0;
}

int8_t 
ssafromtac(struct SSA* ssa, struct BasicBlock* blocks, size_t blocks_n, struct IRFunction* func) {
  if(ssainit(ssa, blocks, blocks_n) != 0) return 1;
  if(ssainsertphinodes(ssa, func) != 0) return 1;

  struct VarstackMap* map = NULL;
  if(ssarenameblock(ssa, &blocks[0], func, &map) != 0) return 1;

  return 0;
}
