/** \file    bdd_reduce.h
 *  \author  Takahisa Toda
 */
#ifndef BDD_REDUCE_H
#define BDD_REDUCE_H

#if defined(REDUCTION)
#include "obdd.h"
#include "bdd_interface.h"

extern bddp bdd_reduce(obdd_t* f);
#endif
#endif /*BDD_REDUCE_H*/
