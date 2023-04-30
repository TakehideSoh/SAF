/** \file     bdd_interface.h
 *  \brief    Functions to access BDD libraries.
 *  \author   Takahisa Toda
 */
#ifndef BDD_INTERFACE_H
#define BDD_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#include "my_def.h"

#if defined(REDUCTION) // added
/*---------------------------------------------------------------------------*/
/* CUDD PACKAGE                                                  */
/*---------------------------------------------------------------------------*/
#include "util.h"
#include "cudd.h"
#include "cuddInt.h"

#define BDD_PACKAGE "CU Decision Diagram Package Release 2.5.0"
#define BDD_NULL  NULL
#define BDD_MAXITEMVAL  CUDD_MAXINDEX //!< maximum value that a BDD library can handle.

typedef DdNode *bddp; //!< pointer to a BDD node

extern DdManager *dd_mgr;


/* Common Operations */
static inline int bdd_init(itemval maxval, uintmax_t n)
{
  if(dd_mgr == NULL) {
    dd_mgr = Cudd_Init((DdHalfWord)maxval, (DdHalfWord)maxval, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, UINTMAX_C(1) << 34);
    //dd_mgr = Cudd_Init((DdHalfWord)maxval, (DdHalfWord)maxval, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    ENSURE_TRUE_MSG(dd_mgr != NULL, "BDD manager initialization failed.");
    Cudd_DisableGarbageCollection(dd_mgr);// disable GC since we never do dereference of nodes.
#ifdef MISC_LOG
    if(Cudd_GarbageCollectionEnabled(dd_mgr)) printf("GC\tenabled\n");
    else                                      printf("GC\tdisabled\n");    
#endif /*MISC_LOG*/

    return ST_SUCCESS;
  } else {
    ENSURE_TRUE_WARN(false, "BDD manager already initialized.");
    return ST_FAILURE;
  }
}

static inline int bdd_quit(void)
{
  if(dd_mgr != NULL) {
    Cudd_Quit(dd_mgr);
    dd_mgr = NULL;
    return ST_SUCCESS;
  } else {
    ENSURE_TRUE_WARN(false, "BDD manager does not exist.");
    return ST_FAILURE;
  }
}

/* BDD Operations*/
static inline itemval bdd_itemval(bddp f)
{  
  assert(dd_mgr != NULL);
  assert(f      != BDD_NULL);

  bddp t = Cudd_Regular(f);
  return (itemval)Cudd_NodeReadIndex(t);
}

static inline bddp bdd_top(void)
{
  assert(dd_mgr != NULL);

  return Cudd_ReadOne(dd_mgr);
}

static inline bddp bdd_bot(void)
{
  assert(dd_mgr != NULL);

  return Cudd_ReadLogicZero(dd_mgr);
}

static inline int bdd_isconst(bddp f)
{
  assert(f != BDD_NULL);

  return (f==bdd_top() || f==bdd_bot());
}

static inline uintmax_t bdd_size(bddp f)
{
  assert(dd_mgr != NULL);
  assert(f      != BDD_NULL);

  return (uintmax_t)Cudd_DagSize(f);
}

static inline bddp bdd_hi(bddp f)
{
  assert(dd_mgr != NULL);
  assert(f      != BDD_NULL);
  assert(!bdd_isconst(f));

  bddp t = Cudd_Regular(f);
  if (Cudd_IsComplement(f)) return Cudd_Not(cuddT(t));
  else                      return cuddT(t);
}

static inline bddp bdd_lo(bddp f)
{
  assert(dd_mgr != NULL);
  assert(f      != BDD_NULL);
  assert(!bdd_isconst(f));

  bddp t = Cudd_Regular(f);
  if (Cudd_IsComplement(f)) return Cudd_Not(cuddE(t));
  else                      return cuddE(t);
}


static inline bddp bdd_and(bddp f, bddp g)
{
  assert(dd_mgr != NULL);
  assert(f != BDD_NULL && g != BDD_NULL);

  return Cudd_bddAnd(dd_mgr, f, g);
}

static inline bddp bdd_node(itemval i, bddp lo, bddp hi)
{
  assert(dd_mgr != NULL);
  assert(!bdd_isconst(hi)? i < bdd_itemval(hi): true);
  assert(!bdd_isconst(lo)? i < bdd_itemval(lo): true);

  bddp f;
  if(lo == hi) {
    f = hi;
  } else {
    if (Cudd_IsComplement(hi)) {
      f = cuddUniqueInter(dd_mgr,(int)i,Cudd_Not(hi),Cudd_Not(lo));
      ENSURE_TRUE_MSG(f != BDD_NULL, "BDD operation failed");
      f = Cudd_Not(f);
    } else {
      f = cuddUniqueInter(dd_mgr,(int)i, hi, lo);
      ENSURE_TRUE_MSG(f != BDD_NULL, "BDD operation failed");
    }
  }
  return f;
}

#endif /*REDUCTION*/

#endif /*BDD_INTERFACE_H*/
