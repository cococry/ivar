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
#include <sys/types.h>

#define DIV_UP(x, y) (((x) + (y) - 1) / (y))

struct SSA {
  struct BasicBlock* blocks;
  size_t blocks_n;

  uint64_t** doms;
  size_t words_n;

  struct BasicBlock** idoms;
};

static int8_t ssaallocatedominatorwords(const size_t blocks_n, uint64_t*** o_dominators, size_t* words_n);

int8_t ssafinddominators(const struct BasicBlock* blocks, size_t blocks_n, size_t words_n, uint64_t** dominators);

int8_t ssafindidoms(struct SSA* ssa);

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

struct BasicBlock* ssaidom(struct SSA* ssa, const struct BasicBlock* b);

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

void ssaprintdominators(struct SSA* ssa) {
  assert(ssa);
  printf("========== Dominators ==========\n");

  for (size_t b = 0; b < ssa->blocks_n; b++) {

    printf("Block %li dominated by: ", b);

    for (size_t block = 0; block < ssa->blocks_n; block++) {

      size_t word = block / 64;
      size_t bit  = block % 64;

      if (ssa->doms[b][word] & (1ULL << bit)) {
        printf("%li ", block);
      }
    }

    const struct BasicBlock* idom = ssa->idoms[ssa->blocks[b].id]; 
    printf("Immidiate dominators: %li\n", idom ? idom->id : -1UL);

    printf("\n");
  }

  printf("================================\n");
}

void ssaprintdfs(struct SSA* ssa) {
  assert(ssa);
  printf("========== Dominance Frontiers ==========\n");

  for (size_t b = 0; b < ssa->blocks_n; b++) {

    printf("Block %li dominance frontiers: ", b);

    for (size_t df = 0; df < ssa->blocks[b].dfs_n; df++) {
        printf("%li ", ssa->blocks[b].dfs[df]->id);
    }

    if(ssa->blocks[b].dfs_n == 0) {
      printf("(none)");
    }
    printf("\n");
  }

  printf("================================\n");
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

int8_t ssafindidoms(struct SSA* ssa) {
  assert(ssa && ssa->idoms);
  for(size_t i = 0;  i < ssa->blocks_n; i++) {
      struct BasicBlock* b = &ssa->blocks[i];
      assert(b->id < ssa->blocks_n);
      ssa->idoms[b->id] = ssaidom(ssa, &ssa->blocks[i]); 
  }
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

    struct SSA ssa;
    ssainit(&ssa, blocks, blocks_n);
    ssagetdominancefrontiers(&ssa);

    ssaprintdominators(&ssa); 

    ssaprintdfs(&ssa); 
    
    printf("=========================\n"); 
  }


  return 0;
} 
