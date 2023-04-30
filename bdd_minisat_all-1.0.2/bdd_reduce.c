/** \file     bdd_reduce.c
 *  \brief    Reduce OBDDs.
 *  \note     Please be careful of stack overflow becase bdd_reduce is implemented in a recursive manner.
 *  \author   Takahisa Toda
 */


#include "bdd_reduce.h"
#include "my_hash.h"

#if defined(REDUCTION)

static bddp bdd_reduce_rec(obdd_t* f, my_hash *h);

static uintmax_t recdepth = 0; //!< recursion depth


bddp bdd_reduce(obdd_t* f)
{
  my_hash *h = ht_create(0);
  ENSURE_TRUE_MSG(h != NULL, "hash table creation failed");

  assert(recdepth == 0);
  bddp r = bdd_reduce_rec(f, h);
  ENSURE_TRUE(r != BDD_NULL);
  assert(recdepth == 0);

  ht_destroy(h);
  return r;
}

static bddp bdd_reduce_rec(obdd_t* f, my_hash *h)
{
  if(f == obdd_top()) return bdd_top();
  if(f == obdd_bot()) return bdd_bot();

  bddp r;
  if(ht_search((uintptr_t)f, (uintptr_t*)&r, h)) return r;

  INC_RECDEPTH(recdepth);
  bddp lo = bdd_reduce_rec(f->lo, h);  
  bddp hi = bdd_reduce_rec(f->hi, h);
  DEC_RECDEPTH(recdepth);

  r = bdd_node(obdd_label(f), lo, hi);
  ENSURE_TRUE_MSG(r != BDD_NULL, "BDD operation failed");

  ht_insert((uintptr_t)f, (uintptr_t)r, h);
  
  return r;
}
#endif
