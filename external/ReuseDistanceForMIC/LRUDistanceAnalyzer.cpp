/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <LRUDistanceAnalyzer.hpp>
#include <iostream>
#include <map>
//#include "pin.H"
using namespace std;
//#define ASSERT_ENABLED
namespace LRUDistanceAnalyzer
{
#define LONG signed long int
#define ULONG unsigned long int
#define ULLINT unsigned long long int

/********************RANGE_TREE CLASS*****************/
#define PAGESIZEX 12
#define min(a, b) ((a)<(b)? (a): (b))
#define max(a, b) ((a)<(b)? (b): (a))
// #define VOID void


typedef uintptr_t memaddr_t;


typedef struct _mem_range_t {
  memaddr_t base;
  size_t size;
} mem_range_t;


/* the data structure for a memory range */
typedef struct _range_node_t {
  struct _range_node_t *lc, *rc;
  mem_range_t r;
} range_node_t;


class range_tree {
public:
  char *unit_name;
  ULONG element_size;
  void *starting_addr;
  void *ending_addr;
  range_node_t *root;
  size_t range_counter;
  range_tree(char *name, ULONG size, void *addr1, void *addr2);
  range_tree(class range_tree *ref_tree);
  ~range_tree();
  char overlap(mem_range_t *b, mem_range_t *r, memaddr_t *base, size_t *size);
  mem_range_t *add_range(memaddr_t base, size_t size);
  int size_in_bytes();
  void to_array(mem_range_t **ranges, ULONG *size);
  void clear();
  unsigned inspect(FILE *result_file);
  unsigned count_bytes(range_node_t *range);
  void check(unsigned nodes, memaddr_t bytes);
  void check_range(mem_range_t *x, memaddr_t base, size_t size);
  void do_intersect(class range_tree *ref_tree);
  void do_union(class range_tree *ref_tree);
  void do_subtract(class range_tree *ref_tree);
private:
  range_node_t* splay (range_node_t * t, memaddr_t key);
  range_node_t* new_node(memaddr_t base, size_t size);
  void union_ranges(mem_range_t *r1, mem_range_t *r2, memaddr_t *ubase, size_t *usize);
  void free_subtree(range_node_t *t);
  range_node_t* absorb_left(range_node_t *node, mem_range_t *range);
  range_node_t *absorb_right(range_node_t *node, mem_range_t *range);
  int subtree_size( range_node_t *n );
  void subtree_to_array(range_node_t *node, mem_range_t **ranges, unsigned *indx);
  char subtree_overlaps(range_node_t *n, mem_range_t *inp, memaddr_t *base, size_t *size);
  char overlaps(mem_range_t *inp, mem_range_t *c_range);
};

range_tree::range_tree(char *name, ULONG size, void *addr1, void *addr2) {
  this->root = NULL;
  this->range_counter = 0;
  this->unit_name = name;
  this->element_size = size;
  this->starting_addr = addr1;
  this->ending_addr = addr2;
}

range_tree::range_tree(class range_tree *ref_tree) {
  this->root = NULL;
  this->range_counter = 0;
  this->unit_name = ref_tree->unit_name;
  this->element_size = ref_tree->element_size;
  this->starting_addr = ref_tree->starting_addr;
  this->ending_addr = ref_tree->ending_addr;
  this->do_union(ref_tree);
}

range_tree::~range_tree() {
  free_subtree(this->root);
}

/* The code for this function is adapted from
   http://www.link.cs.cmu.edu/link/ftp-site/splaying/top-down-splay.c */
range_node_t* range_tree::splay (range_node_t * t, memaddr_t key) {
    range_node_t N, *l, *r, *y;
    if (t == NULL) return t;
    N.lc = N.rc = NULL;
    l = r = &N;

    for (;;) {
	if (key < t->r.base) {
	    if (t->lc != NULL && key < t->lc->r.base) {
		y = t->lc; t->lc = y->rc; y->rc = t; t = y;
	    }
	    if (t->lc == NULL) break;
	    r->lc = t; r = t; t = t->lc;
	} else if (key > t->r.base) {
	    if (t->rc != NULL && key > t->rc->r.base) {
		y = t->rc; t->rc = y->lc; y->lc = t; t = y;
	    }
	    if (t->rc == NULL) break;
	    l->rc = t; l = t; t = t->rc;
	} else break;
    }
    l->rc=t->lc; r->lc=t->rc; t->lc=N.rc; t->rc=N.lc;
    return t;
}


char range_tree::overlap(mem_range_t *b, mem_range_t *r, memaddr_t *base, size_t *size) {
  if (b->base <= r->base) {
    if (b->base+b->size > r->base) {
      if (b->base+b->size <= r->base+r->size) {
        *base = r->base;
        *size = b->base+b->size-r->base;
      } else {
        *base = r->base;
        *size = r->size;
      }
      return 1;  /* true */
    } else {
      return 0;  /* false */
    }
  } else {
    if (r->base+r->size > b->base) {
      if (r->base+r->size <= b->base+b->size) {
        *base = b->base;
        *size = r->base+r->size-b->base;
      } else {
        *base = b->base;
        *size = b->size;
      }
      return 1;  /* true */
    } else {
      return 0;  /* false */
    }
  }
}


range_node_t* range_tree::new_node(memaddr_t base, size_t size) {
  range_node_t *n = (range_node_t *)calloc(1, sizeof(range_node_t));
  n->r.base = base;
  n->r.size = size;
  n->lc = n->rc = NULL;
  return n;
}


void range_tree::union_ranges(mem_range_t *r1, mem_range_t *r2, memaddr_t *ubase, size_t *usize) {
  *ubase = min(r1->base, r2->base);
  *usize = max(r1->base+r1->size, r2->base+r2->size) - *ubase;
  return;
}


void range_tree::free_subtree(range_node_t *t) {
  if (t == NULL)
    return;
  free_subtree(t->lc);
  free_subtree(t->rc);
  t->lc = NULL;
  t->rc = NULL;
  free(t);
  range_counter--;
}


range_node_t* range_tree::absorb_left(range_node_t *node, mem_range_t *range) {
  if (node == NULL)
    return NULL;

  if (range->base > node->r.base+node->r.size) {
    node->rc = absorb_left(node->rc, range);
    return node;
  } else {
    memaddr_t oldbase = range->base;
    range->base = min(node->r.base, range->base);
    range->size = oldbase + range->size - range->base;
    range_node_t *ret = node->lc;

    node->lc = NULL;
    free_subtree(node);

    return absorb_left(ret, range);
  }
}


range_node_t* range_tree::absorb_right(range_node_t *node, mem_range_t *range) {
  if (node == NULL)
    return NULL;

  if (range->base + range->size < node->r.base) {
    node->lc = absorb_right(node->lc, range);
    return node;
  } else {
    memaddr_t rb = max(range->base+range->size, node->r.base+node->r.size);
    range->size = rb - range->base;
    range_node_t *ret = node->rc;
    
    node->rc = NULL;
    free_subtree(node);

    return absorb_right(ret, range);
  }
}


mem_range_t* range_tree::add_range(memaddr_t base, size_t size) {
#ifdef ASSERT_ENABLED
  assert(size > 0);
#endif

  range_node_t *n = new_node(base, size);

  if (root == NULL) {
    root = n;
    range_counter = 1;
    return & root->r;
  }
  
  range_node_t *top = splay(root, base);

  /* if there is overlap, expand the root node */
  memaddr_t cbase;
  size_t csize;
  char has_overlap = overlap( &n->r, &top->r, &cbase, &csize);
  char bordering = 0;  /* false */
  if (n->r.base < top->r.base)
    bordering = (n->r.base + n->r.size == top->r.base);
  else
    bordering = (top->r.base + top->r.size == n->r.base);

  if (has_overlap || bordering) {
    memaddr_t ubase; size_t usize;
    union_ranges(&n->r, &top->r, &ubase, &usize);
    top->r.base = ubase;
    top->r.size = usize;
    top->lc = absorb_left(top->lc, &top->r);
    top->rc = absorb_right(top->rc, &top->r);

    root = top;
    free(n);
    return &root->r;
  }

  if (base < top->r.base) {
    n->lc = top->lc;
    n->rc = top;
    top->lc = NULL;
    range_counter++;
    top = n;
    top->lc = absorb_left(top->lc, &top->r);
  } else if (base > top->r.base) {
    n->rc = top->rc;
    n->lc = top;
    top->rc = NULL;
    range_counter++;
    top = n;
    top->rc = absorb_right(top->rc, &top->r);
  }
    
  root = top;
  return &root->r;
}


int range_tree::subtree_size( range_node_t *n ) {
  if (n == NULL)
    return 0;
  return n->r.size + subtree_size(n->lc) + subtree_size(n->rc);
}


int range_tree::size_in_bytes() {
  return subtree_size(root);
}


void range_tree::subtree_to_array(range_node_t *node, mem_range_t **ranges, unsigned *indx) {
  if (node == NULL)
    return;
  subtree_to_array(node->lc, ranges, indx);
  (*ranges)[*indx] = node->r;
  *indx = *indx + 1;
  subtree_to_array(node->rc, ranges, indx);
}


void range_tree::to_array(mem_range_t **ranges, ULONG *size) {
  *ranges = (mem_range_t*)calloc(range_counter, sizeof(mem_range_t));
  *size = range_counter;

  unsigned indx = 0;
  subtree_to_array(root, ranges, &indx);
  assert(indx == *size);
}


void range_tree::clear() {
  free_subtree(root);
  root = NULL;
}


unsigned range_tree::inspect(FILE *result_file) {
  if(range_counter == 0)
    return 0;

  ULONG i;

//  fprintf(result_file, "==>unit name: %s\taddr: [%p, %p]\nnum. of elements: %lu (element size is %lu)\nnum. of ranges: %lu\n", unit_name, starting_addr, ending_addr, ((ULONG)ending_addr-(ULONG)starting_addr)/element_size, element_size, (unsigned long)range_counter);

  mem_range_t *ranges;
  ULONG num;
  to_array(&ranges, &num);

  for(i = 0; i < num; ++i)
    //fprintf(result_file, "\t(array index: %lu - %lu, addr: %p - %p, page %lu, size %lu)\n", (ranges[i].base - (ULONG)starting_addr)/element_size, (ranges[i].base + ranges[i].size - (ULONG)starting_addr)/element_size, (void *)ranges[i].base, (void *)(ranges[i].base + ranges[i].size), (unsigned long)ranges[i].base >> PAGESIZEX, (unsigned long)ranges[i].size);

  return range_counter;
}


unsigned range_tree::count_bytes(range_node_t *range) {
  unsigned byte_counter = 0;
  if (range->lc != NULL)
    byte_counter += count_bytes(range->lc);
  byte_counter += range->r.size;
  if (range->rc != NULL)
    byte_counter += count_bytes(range->rc);
  return byte_counter;
}


void range_tree::check(unsigned nodes, memaddr_t bytes) {
  printf("--------------------------\n");
  inspect(stderr);
  assert(range_counter == nodes);
  assert(count_bytes(root) == bytes);
}


void range_tree::check_range(mem_range_t *x, memaddr_t base, size_t size) {
  assert(x->base == base);
  assert(x->size == size);
}


char range_tree::subtree_overlaps(range_node_t *n, mem_range_t *inp, memaddr_t *base, size_t *size) {
  if (n == NULL) return 0;  /* false */

  if ( n->r.base + n->r.size <= inp->base )
    return subtree_overlaps( n->rc, inp, base, size );

  if ( n->r.base > inp->base + inp->size )
    return subtree_overlaps( n->lc, inp, base, size );

  if ( n->r.base > inp->base ) {
    mem_range_t head;
    head.base = inp->base;
    head.size = n->r.base - inp->base;
    char overlap = subtree_overlaps( n->lc, &head, base, size );
    if ( overlap ) return overlap;
  }
  
  return overlap( & n->r, inp, base, size );
}

/* return the smallest overlap.  This is useful for enumerating all overlaps by repeatedly calling this function. */

char range_tree::overlaps(mem_range_t *inp, mem_range_t *c_range) {
  memaddr_t base; size_t size;
  char is_overlap = subtree_overlaps( this->root, inp, &base, &size );
  if (is_overlap && c_range != NULL) {
    c_range->base = base;
    c_range->size = size;
  }
  return is_overlap;
}


void range_tree::do_intersect(class range_tree *ref_tree) {  
  if( this->root == NULL ) return;

  mem_range_t *ranges; ULONG num, i;
  to_array(&ranges, &num);

  clear();
  if ( !ref_tree || ref_tree->root == NULL ) return;

  for ( i = 0; i < num; ++ i ) {
    mem_range_t range = ranges[ i ];
    mem_range_t orange;
    char overlap;
    do {
      overlap = ref_tree->overlaps(&range, &orange );
      if (overlap) {
	add_range(orange.base, orange.size);
	memaddr_t old_bound = range.base + range.size;
	range.base = orange.base + orange.size;
	range.size = old_bound - range.base;
      }
    } while (overlap && range.size != 0);
  }

  free(ranges);
  return;
}

void range_tree::do_union(class range_tree *ref_tree) {
  if (ref_tree->root == NULL ) return;

  mem_range_t *ranges; ULONG num, i;
  ref_tree->to_array(&ranges, &num);

  for ( i = 0; i < num; ++ i ) {
    mem_range_t range = ranges[ i ];
#if 0 /* support overlap */
    mem_range_t c_range;
    if ( overlaps(&range, &c_range ) ) {
      /* No support for partial overlap yet.  In this case, range is
	 completely covered in the base_map  */
      assert( range.base == c_range.base && range.size == c_range.size );
      continue;
    }
#endif
    add_range(range.base, range.size);
  }

  free(ranges);
  return;
}

void range_tree::do_subtract(class range_tree *ref_tree) {
  if(!ref_tree || ref_tree->root == NULL) return;
  if(this->root == NULL) return;
  
  /* make copy_map be the intersection */
  class range_tree *copy_tree = new range_tree(this);
  copy_tree->do_intersect(ref_tree);
  
  /* do the subtraction => base_map-copy_map */
  mem_range_t *ranges; ULONG num, i;
  to_array(&ranges, &num);
  
  clear();
  
  for (i=0;i<num;i++) {
    mem_range_t range = ranges[i];
    mem_range_t orange;
    char overlap;
    do {
      overlap = copy_tree->overlaps(&range, &orange);
      if (overlap) {
	if (orange.base > range.base)
	  add_range(range.base, orange.base-range.base);
	memaddr_t old_bound = range.base + range.size;
	range.base = orange.base + orange.size;
	range.size = old_bound - range.base;
      }
    } while (overlap && range.size != 0);
    if (range.size != 0)
      add_range(range.base, range.size);
  }
  
  /* free the space */
  free(ranges);
  copy_tree->~range_tree();
  
  return;
}

/****************END OF RANGE_TREE CLASS*****************/

// CAUTION: In the long run these parameters should be configurable! 



typedef long long hrtime_t;

/* get the number of CPU cycles per microsecond - from Linux /proc filesystem return<0 on error
 */
double getMHZ_x86(void) {
  double mhz = -1;
  char line[1024], *s, search_str[] = "cpu MHz";
  FILE *fp; 
  
  /* open proc/cpuinfo */
  if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
    return -1;
  
  /* ignore all lines until we reach MHz information */
  while (fgets(line, 1024, fp) != NULL) { 
    if (strstr(line, search_str) != NULL) {
      /* ignore all characters in line up to : */
      for (s = line; *s && (*s != ':'); ++s);
      /* get MHz number */
      if (*s && (sscanf(s+1, "%lf", &mhz) == 1)) 
	break;
    }
  }
  
  if (fp!=NULL) fclose(fp);
  
  return mhz;
}

/* get the number of CPU cycles since startup */
hrtime_t gethrcycle_x86(void) {
  ULONG tmp[2];
  
  __asm__ ("rdtsc"
	   : "=a" (tmp[1]), "=d" (tmp[0])
	   : "c" (0x10) );
  
  return ( ((hrtime_t)tmp[0] << 32 | tmp[1]) );
}

hrtime_t total_time;
hrtime_t analysis_time;
ULLINT processing_times;

typedef struct _access_entry {
  ADDR addr;
  uint64_t* PINStats;
} access_entry;

access_entry buffer[BUFFER_SIZE];
ULONG buf_size;
ULLINT counter;
ULLINT hist[BIN_SIZE];
ULLINT upper_boundary[BIN_SIZE];
string upper_boundary_strings[BIN_SIZE] = {"64B", "128B", "256B", "512B", "1KB", "2KB", "4KB", "8KB", "16KB", "32KB", 
				"64KB", "128KB", "256KB", "512KB", "1MB", "2MB", "4MB", "8MB", "16MB", "32MB",
				"64MB", "128MB", "256MB", "512MB", "1GB", "2GB", "4GB", "8GB", "16GB", "32GB",
                                "64GB", "INFINITE"};

typedef struct _rtree_node {
  class range_tree *rtree;
  struct _rtree_node *next;
} rtree_node;

rtree_node* rtree_node_list[BIN_SIZE];

typedef struct _hash_node {
  ADDR addr;
  ULLINT last_acc_time;
  struct _hash_node *next;
} hash_node;

hash_node *hash_table[HASH_TABLE_SIZE];
hash_node *hash_node_list = NULL;
ULONG hash_used = 0;

typedef struct _tree {
  struct _tree *left, *right;
  ULLINT priority;    /* last access time of this node */
  ULONG node_weight;  /* weight of the node */
  ULLINT weight;  /* weight of the entire subtree, including this node */
  ULONG max_size;    /* maximal size of the node */
  struct _tree *prev, *next; /* nodes that are next to this node in the sorted order */
} tree;

tree *trace;
ULLINT trace_size = 0;
double error_rate = 0.001;
ULLINT data_num = 0;
ULONG power = 1;
tree *free_node_list = NULL;

FILE *output_file = NULL;
FILE *result_file = NULL;

void free_node(tree *node) {
  node->next = free_node_list;
  free_node_list = node;                     
}   

tree* compact_scale_tree(tree *root) {
  tree *cur, *prev;
  ULLINT prior_weight;
  ULLINT total_weight;
  if (root == NULL)
    return root;
  prior_weight = 0;
  total_weight = root->weight;
  /* from the end of trace forward, we set each node with correct 
     maxSize and merge with its left neighbor if possible */
#ifdef DEBUG
  assert(root->right==NULL);  /* root is the most recent */
#endif
  cur = root;
  while (cur!=NULL) {
    cur->right = (tree*)NULL;  /* make it a list */
    cur->weight = total_weight - prior_weight;
    cur->max_size = (ULONG) (prior_weight * error_rate);
    if (cur->max_size==0) cur->max_size = 1;
    if (cur->prev!=NULL && cur->prev->node_weight + cur->node_weight <= cur->max_size) {
      prev = cur->prev;
      cur->node_weight += prev->node_weight;
      cur->prev = prev->prev;
      if (cur->prev!=NULL)
	cur->prev->next = cur;
      cur->left = cur->prev;
      free_node(prev);
      trace_size--;
    } else {
      cur->left = cur->prev;
      prior_weight += cur->node_weight;
      cur = cur->left;
    }
  }
  return root;
}

void allocate_hash_nodes() {
  hash_node_list = (hash_node*)malloc(sizeof(hash_node)*BATCH_ALLOC_SIZE);
  if (hash_node_list == NULL) {
    trace = compact_scale_tree(trace);
    printf("Tree is compacted because of insufficient memory for hash\n");
  }
  hash_used=0;
}

/* addr is accessed at cyc, Cycle cyc means never accessed before
 * insert addr at the head if it is not found 
 */
ULLINT hash_search_update(ADDR addr, ULLINT cyc) {
  ULONG hash_key;
  hash_node *node;
  ULLINT old_cycle;
  hash_key = ((ULONG)addr) % HASH_TABLE_SIZE;
  node = hash_table[hash_key];
  
  while ((node != NULL) && (node->addr != addr)) {
    node = node->next;
  }
  if (node != NULL) {
    old_cycle = node->last_acc_time;
    node->last_acc_time = cyc;
    return old_cycle;
  } else {
    node = &hash_node_list[hash_used++];
    if (hash_used == BATCH_ALLOC_SIZE)
      allocate_hash_nodes();
    node->addr = addr;
    node->last_acc_time = cyc;
    node->next = hash_table[hash_key];
    hash_table[hash_key] = node;
    return cyc;  /* means that a previous record is not found */
  }
}

tree* malloc_node() {
  tree *t;
  if (!free_node_list) {
    tree *l = (tree *)malloc(sizeof(tree)*BATCH_ALLOC_SIZE);
    if (l == NULL) {
      trace = compact_scale_tree(trace);
      printf("Tree is compacted because of insufficient memory for scaletree\n");
    } else {
      ULONG i;
      for (i=0;i<BATCH_ALLOC_SIZE-1;i++)
	l[i].next = &(l[i+1]);
      l[BATCH_ALLOC_SIZE-1].next = free_node_list;
      free_node_list = l;
    }
  }
  t = free_node_list;
  free_node_list = free_node_list->next;
  return t;
}

tree* scale_tree_splay(ULLINT i, tree* t, ULLINT *dis) {
  tree n, *l, *r, *y;
  ULLINT left = 0, right = 0;
  if (t == NULL)
    return t;
  n.left = n.right = (tree*)NULL;
  l = r = &n;
  n.weight = t->weight;
  
  y = t;
  for (;;) {
    if ((i < y->priority) && (y->prev!=NULL && i<=y->prev->priority)) {
#ifdef DEBUG
      assert(i<=y->prev->priority);
#endif
      if (y->right != NULL)
	right += y->right->weight;
      if (y->left == NULL)
	break;
      right += y->node_weight;
      y = y->left;
    } else if (i > y->priority) {
      if (y->left != NULL)
	left += y->left->weight;
      if (y->right == NULL)
	break;
      left += y->node_weight;
      y = y->right;
    } else {
      /* i is within the block of y */
      if (y->right != NULL)
	right += y->right->weight;
      if (y->left != NULL)
	left += y->left->weight;
      break;
    }      
  }
  for (;;) {
    if ((i < t->priority) && (t->prev!=NULL && i<=t->prev->priority)) {
      if (t->left == NULL)
	break;
      if ((i < t->left->priority) && (t->left->prev!=NULL && i<=t->left->prev->priority)) {
	y = t->left;     /* rotate right */
	t->left = y->right;
	y->right = t;
	/* t->weight--; */
	t->weight -= y->node_weight;
	t = y;
	if (t->left == NULL)
	  break;
	t->right->weight -= t->left->weight;                
      }
      t->weight = right;
      /* right--; */
      right -= t->node_weight;
      if (t->right != NULL)
	right -= t->right->weight;
      r->left = t;                               /* link right */
      r = t;
      t = t->left;
    } else if (i > t->priority) {
      if (t->right == NULL)
	break;
      if (i > t->right->priority) {
	y = t->right;                          /* rotate left */
	t->right = y->left;
	y->left = t;
	/* t->weight--; */
	t->weight -= y->node_weight;
	t = y;
	if (t->right == NULL)
	  break;
	t->left->weight -= t->right->weight;                
      }
      t->weight = left;
      /* left--; */
      left -= t->node_weight;
      if (t->left != NULL)
	left -= t->left->weight;
      l->right = t;                              /* link left */
      l = t;
      t = t->right;
    } else {
      break;
    }
  }
  l->right = t->left;                                /* assemble */
  r->left = t->right;
  t->left = n.right;
  t->right = n.left;
  t->weight = n.weight;
  
  *dis = t->node_weight/2;
  if (t->right != NULL)
    *dis += t->right->weight;
  
  return t;
}

tree* scale_tree_insert_at_front(ULLINT block_end, tree *t, tree *new_node) {
  tree *new_one, *prev;
  if (new_node==NULL) {
    new_one = malloc_node(); /* by Zhangchl */
    if (new_one == NULL) {
      printf("Ran out of space\n");
      exit(1);
    }
    trace_size++;
  } else
    new_one = new_node;

  new_one->priority = block_end;
  new_one->node_weight = 1;
  new_one->max_size = 1;
  if (t == NULL) {
    new_one->left = new_one->right = (tree*)NULL;
    new_one->weight = 1;
    new_one->prev = new_one->next = (tree*)NULL;
    return new_one;
  }
  
  /* Insert at the front of the tree */
  //fprintf(stderr, "block_end: %lld\tpriority: %lld\n", block_end, t->priority);
#ifdef DEBUG
  assert(block_end > t->priority); 
#endif
  new_one->weight = t->weight + 1;
  new_one->left = t;
  new_one->right = (tree*)NULL;
  
  /* find prev and next */
  new_one->next = (tree*)NULL;
  prev = new_one->left;
  if (prev != NULL)
    while (prev->right != NULL) 
      prev = prev->right;
  new_one->prev = prev;
  if (prev != NULL)
    prev->next = new_one;
  
  /* printf("insert: new %d, prev %d, next %d\n", new->priority, 
     new->prev!=NULL?new->prev->priority: -1, 
     new->next!=NULL? new->next->priority: -1); */
  
  return new_one;
}

tree* query_scale_tree(ULLINT old_cycle, ULLINT new_cycle, tree *t, ULLINT *dis) {
  ULLINT useless;
  tree *tmp, *recycle_node = (tree*)NULL;
  ULLINT right_child_weight;
  if (old_cycle == new_cycle) {
    t = scale_tree_insert_at_front(new_cycle, t, (tree*)NULL);
    return t;
  }
  t = scale_tree_splay(old_cycle, t, dis);
#ifdef DEBUG
  /* make sure the being accessed data is at the root */
  assert(old_cycle <= t->priority);
  if ((t->prev !=NULL) && (old_cycle <= t->prev->priority))
    assert(0);
#endif
  /* set the size of t */
  if (t->right != NULL)
    right_child_weight = t->right->weight;
  else
    right_child_weight = 0;
    
  t->max_size = (ULONG)(right_child_weight * error_rate);
  if (t->max_size==0)
    t->max_size = 1;
    
  /* delete the old_cycle, merge nodes if necessary */
  t->node_weight--;
  t->weight--;
#ifdef DEBUG
  assert((LONG)t->node_weight>=0);
#endif
  if (t->node_weight <= (t->max_size >> 1)) {
    if ((t->prev != NULL) && (t->prev->node_weight + t->node_weight <= t->max_size)) {
      t->left = scale_tree_splay(t->prev->priority, t->left, &useless);
#ifdef DEBUG
      assert(t->left->right==NULL);  /* largest of the left subtree */
      assert(t->left==t->prev);
#endif
      t->left->right = t->right;  /* new tree */
      if (t->node_weight > 0)   /* otherwise, t is empty */
	t->left->priority = t->priority;    /* merge  */  
      t->left->node_weight += t->node_weight;
      t->left->weight += t->node_weight;
      if (t->right != NULL)
	t->left->weight += t->right->weight;
      t->left->next = t->next;    /* new neighbors */
      if (t->next != NULL) 
	t->next->prev = t->left;
      tmp = t;
      t = t->left;
      if (recycle_node == NULL)
	recycle_node = tmp;
      else {
	free_node(tmp);
	trace_size--;
      }
    }
    if (t->prev != NULL) {
      t->prev->max_size = (ULONG) ((right_child_weight + t->node_weight) * error_rate);
      if (t->prev->max_size==0)
	t->prev->max_size = 1;
    }
    if (t->next != NULL) {
      t->next->max_size = (ULONG) ((right_child_weight - t->next->node_weight) * error_rate);
      if (t->next->max_size==0)
	t->next->max_size = 1;
    }
    if ((t->next != NULL) && (t->next->node_weight + t->node_weight <= t->next->max_size)) {
      /* merge next with me */
      t->right = scale_tree_splay(t->next->priority, t->right, &useless);
#ifdef DEBUG
      assert(t->right->left==NULL);
      assert(t->right==t->next);
#endif
      t->right->left = t->left;   /* new tree */
      t->right->node_weight += t->node_weight; /* merge    */      
      t->right->weight += t->node_weight;
      if (t->left!=NULL) t->right->weight += t->left->weight;
      t->right->prev = t->prev;    /* new neighbors */
      if (t->prev!=NULL) 
	t->prev->next = t->right;
      tmp = t;
      t = t->right;
      if (recycle_node == NULL) recycle_node = tmp;
      else {
	free_node(tmp);
	trace_size--;
      }
    }      
  }
  
  /* ytzhong: only one address has been accessed and re-accessed,
     the nodeWt could be zero after compaction
  */
  /*  if (t->nodeWt == 0) 
      assert(0);
  */  
    
  /* insert newCyc */
  t = scale_tree_insert_at_front(new_cycle, t, recycle_node);
  
  return t;
}

ULLINT get_lru_reuse_dis(ADDR addr) {
  ULLINT last_acc_time;
  ULLINT dis = 0;
  last_acc_time = hash_search_update(addr, counter);
  //fprintf(stderr, "last_acc_time: %lld\n", last_acc_time);
  trace = query_scale_tree(last_acc_time, counter, trace, &dis);
  if (last_acc_time == counter) { 
    /* a new element */
    dis = INFINITE;
    data_num++;
    if (data_num-((data_num>>10)<<10) == 0) {
      ULONG tmp = data_num;
      power = 1;
      while (tmp > 0) {
	tmp = tmp >> 6;
	power ++;
      }
    }
  }

  /* check after every 65K accesses */
  if(((trace_size & 0xffff) == 0) && (trace_size > (power<<13))) /* xiaoming: why we need power? */
    /* compress at size 4*log_{1/errorRare}^M + 4 */
    /* see DingZhong:PLDI03, pp 247 */
    if(trace_size > 4*log((double)data_num)/log((double)1.0/(1-error_rate))+4)
      trace = compact_scale_tree(trace);

  counter++;  
  return dis;
}


void try_to_add_range(void *data, ULONG bin_pos) {
  rtree_node *node = rtree_node_list[bin_pos];
  while (node != NULL) {
    if (((ULONG)data >= (ULONG)node->rtree->starting_addr) && ((ULONG)data <= (ULONG)node->rtree->ending_addr)) {
      //fprintf(result_file, "accessing addr: %p\n", data);
      node->rtree->add_range((memaddr_t)data, 64);
      return;
    }
    node = node->next;
  }
}


void record_distance(ULLINT dis,void *data,uint64_t* BBStats) {
  ULONG i = 0;
  for (;i<BIN_SIZE-1;i++) {
    if (dis <= upper_boundary[i]) {
      ++hist[i];
      ++BBStats[i];
      try_to_add_range(data, i);
      return;
    }
  }
  ++hist[BIN_SIZE-1];
  try_to_add_range(data, BIN_SIZE-1);
}

void comp_lru_reuse_dis() {
  ULONG i;
  hrtime_t temp;
  if (processing_times % 128 == 0)
    fprintf(stderr, "doing No.%llu buffer processing\n", processing_times);
  //printf("doing No.%llu buffer processing\n", processing_times);
  ++processing_times;
  temp = gethrcycle_x86();
  for (i=0;i<buf_size;i++) {
#ifdef DEBUG
    fprintf(stderr, "No.%lu\t%lu\n", i, (unsigned long) buffer[i].addr);
#endif
    ULLINT lru_reuse_dis = get_lru_reuse_dis(buffer[i].addr);
    if (lru_reuse_dis != INFINITE)
      lru_reuse_dis++;
#ifdef DEBUG
    if(lru_reuse_dis == INFINITE)
      fprintf(stderr, "LRU reuse dis: INFINITE\n");
    else
      fprintf(stderr, "LRU reuse dis: %llu\n", lru_reuse_dis);
#endif
    record_distance(lru_reuse_dis, buffer[i].addr,buffer[i].PINStats);
#ifdef DEBUG
    fprintf(stderr, "------------\n");
#endif
  }
  buf_size = 0;
  analysis_time += gethrcycle_x86() - temp;
}

static int RecordMemAccessNotcalled=1;
static int init_notcalled=1;

// Print a memory read record
VOID RecordMemAccess(VOID * addr,uint64_t* BBStats) {
  addr = (ADDR)((((ULONG)addr)>>OFFSET_SIZE)<<OFFSET_SIZE);
  buffer[buf_size].addr = addr;
  buffer[buf_size].PINStats=BBStats;
  buf_size++;
  if (buf_size == BUFFER_SIZE) {
    comp_lru_reuse_dis();
  }
}
 
#define MALLOC "malloc"
#define MEMALIGN "memalign"
#define FREE "free"
#define MEM_UNIT_NAME "mem_unit_name"
#define MEM_UNIT_ELEMENT_SIZE "mem_unit_element_size"

#define MEM_UNIT_MINIMAL_THRESHOLD 65536

typedef enum {PLAIN,
	      MALLOC_BEGINNING,
	      MEMALIGN_BEGINNING,
	      MEM_UNIT_NAME_BEGINNING,
	      MEM_UNIT_ELEMENT_SIZE_BEGINNING,
	      FREE_BEGINNING,
	      FREE_END} state;

state current_state;

typedef struct _mem_unit {
  char *name;
  ULONG element_size;
  void *starting_addr;
  void *ending_addr;
  struct _mem_unit *next;
} mem_unit;

mem_unit *mem_unit_list;
#define SHORT_DISTANCE_THRESHOLD 10 //64K

void get_exclusive_MRU_data(ULONG bin_num_for_csz) {
  //fprintf(result_file, "******cache size: %s******\n", upper_boundary_strings[bin_num_for_csz].c_str());
  ULONG i, MRU_start, LRU_start;
  rtree_node *iter, *iter2, *last;
  rtree_node *MRU_rtree_list, *LRU_rtree_list;

  /* get the MRU data set */
  MRU_rtree_list = NULL;
  MRU_start = bin_num_for_csz+1;
  iter  = rtree_node_list[MRU_start];
  last = NULL;
  while (iter != NULL) {
    rtree_node *node = (rtree_node *)malloc(sizeof(rtree_node));
    node->rtree = new range_tree(iter->rtree);
    node->next = NULL;
    if (MRU_rtree_list == NULL)
      MRU_rtree_list = node;
    else
      last->next = node;
    last = node;
    iter = iter->next;
  }
  for (i=MRU_start+1;i<BIN_SIZE-1;i++) {
    iter = MRU_rtree_list;
    iter2 = rtree_node_list[i];
    while (iter != NULL) {
      iter->rtree->do_union(iter2->rtree);
      iter = iter->next;
      iter2 = iter2->next;
    }
  }

  /* get the LRU data set */
  LRU_rtree_list = NULL;
  LRU_start = SHORT_DISTANCE_THRESHOLD+1;
  iter = rtree_node_list[LRU_start];
  last = NULL;
  while (iter != NULL) {
    rtree_node *node = (rtree_node *)malloc(sizeof(rtree_node));
    node->rtree = new range_tree(iter->rtree);
    node->next = NULL;
    if (LRU_rtree_list == NULL)
      LRU_rtree_list = node;
    else
      last->next = node;
    last = node;
    iter = iter->next;
  }
  for (i=LRU_start+1;i<MRU_start;i++) {
    iter = LRU_rtree_list;
    iter2 = rtree_node_list[i];
    while (iter != NULL) {
      iter->rtree->do_union(iter2->rtree);
      iter = iter->next;
      iter2 = iter2->next;
    }
  }

  /* do the subtraction to get the exclusive MRU data set */
  iter = MRU_rtree_list;
  iter2 = LRU_rtree_list;
  while (iter != NULL) {
    iter->rtree->do_subtract(iter2->rtree);
    iter = iter->next;
    iter2 = iter2->next;
  }

  /* output the exclusive MRU data set */
  iter = MRU_rtree_list;
  while (iter != NULL) {
   // iter->rtree->inspect(result_file);
    iter = iter->next;
  }
}
 
void OutputResults() {

//printf("\n\t In OutputResults() \n --- \n\n");
  ULONG i;
  comp_lru_reuse_dis();
  for (i=0;i<BIN_SIZE;i++) {
    // fprintf(result_file, "**BIN %lu at %s ** => %llu\n", i+1, upper_boundary_strings[i].c_str(), hist[i]);
    rtree_node *node = rtree_node_list[i];
    while (node != NULL) {
      // node->rtree->inspect(result_file);
      node = node->next;
    }
  }

  total_time = gethrcycle_x86() - total_time;
  mem_unit *unit = mem_unit_list;
  
    //14=>1M 15=>2M 16=>4M 17=>8M 18=>16M 19=>32M
  get_exclusive_MRU_data(14);
  get_exclusive_MRU_data(15);
  get_exclusive_MRU_data(16);
  get_exclusive_MRU_data(17);
  get_exclusive_MRU_data(18);
  get_exclusive_MRU_data(19); 
 
}
 


void Init() {
   ULONG i;

  total_time = gethrcycle_x86();
  analysis_time = 0;
  processing_times = 0;
  counter = 0;
  buf_size = 0;
  for (i=0;i<BIN_SIZE;i++) {
    hist[i] = 0;
    rtree_node_list[i] = NULL;
  }
  upper_boundary[0] = 1;
  upper_boundary[BIN_SIZE-1] = INFINITE;
  for (i=1;i<BIN_SIZE-1;i++)
    upper_boundary[i] = upper_boundary[i-1]*2;
  for (i=0;i<HASH_TABLE_SIZE;i++)
    hash_table[i] = NULL;
  allocate_hash_nodes();


  current_state = PLAIN;
  mem_unit_list = NULL;
}
}

