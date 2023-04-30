/** \file     obdd.h
 *  \brief    OBDD implementation, where OBDDs mean BDDs that are ordered, but need not be reduced.
 *  \author   Takahisa Toda
 *  \note     For details of BDDs (Binary Decision Diagrams), see
 *  - Bryant, R.E.: Graph-Based algorithm for Boolean function manipulation, IEEE Trans. Comput., Vol.35, pp.677-691 (1986)
 *  - Knuth, D.E.: The Art of Computer Programming Volume 4a, Addison-Wesley Professional, New Jersey, USA (2011) .
 */
#ifndef OBDD_H
#define OBDD_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#ifdef GMP
#include <gmp.h>
#endif

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef struct obdd_st obdd_t;

/** \brief  binary decidion diagram node*/
struct obdd_st {
  int              v;      //!< assigned label
  intptr_t       aux;      //!< an auxiliary field (of pointer size), which is introduced in order to facilitate implementation.
  struct obdd_st  *lo;      //!< low arc
  struct obdd_st  *hi;      //!< hi arc
  struct obdd_st  *nx;      //!< 
};


extern obdd_t *top_node; //!< top terminal node
extern obdd_t *bot_node; //!< bottom terminal node

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/


/* \brief   Obtain obdd node with specified field.
 * \param   v   variable index, which must be a non-zero positive interger.
 * \param   lo  lo child
 * \param   hi  hi child
 * \return  pointer to an obtained node
 */
extern obdd_t* obdd_node(int v, obdd_t* lo, obdd_t* hi);


/* \brief For all nodes in p, if their lo or hi field is NULL, set bottom terminal.
 * \return the number of nonterminal nodes
 * \note
 * - Since obdd constructed by sat solver may be imcomplete, call this function before applying other functions.
 * - That is, after this function finishes, all nodes below p are linked from p by nx field, which are assumed in executing other obdd function for the sake of efficiency.
 * - If it is called for different obdds, all nodes in the last obdd are linked, while those in other obdds may not! If necessary, call this function again.
 */
extern uintmax_t obdd_complete(obdd_t* p);


/* \brief Count the number of all nodes in p except for terminal nodes.
 */
extern uintmax_t obdd_size(obdd_t* p);

/* \brief Make a file in dot format that represents the graph structure of p.
 * \param n     the number of variables
 * \param p     root node of obdd
 * \param out   pointer to output file, which must be open in write mode.
 * \return ST_SUCCESS if successful; ST_FAILURE, otherwise.
 * \note    See www.graphviz.org/ .
 */
extern int obdd_to_dot(int n, obdd_t* p, FILE *out);


/* \brief Count the number of paths from the root to the top terminal in p, which corresponds to the number of total satisfying assignments, i.e. solutions.
 * \param n     the number of variables
 * \param p     root of obdd
 * \return the computed number
 */
extern intptr_t obdd_nsols(int n, obdd_t* p);


#ifdef GMP
/* \brief GMP version of obdd_nsols: this is useful if the number of solutions is too large to count.
 * \param result the computed number is stored here.
 * \param n     the number of variables
 * \param p     root of obdd
 * \note see The GNU MP Bignum Library: https://gmplib.org/ .
 */
extern void obdd_nsols_gmp(mpz_t result, int n, obdd_t* p);
#endif


/* \brief Compute all partial assignments by traversing obdd.
 * \param out   pointer to output file, which must be open in write mode.
 * \param n     the number of variables
 * \param p     root of obdd
 * \return The number of assignments.
 * \note
 * - when this function is called several times, results are appended to output file.
 * - Please take care that a huge number of assignments may be generated.
 */
extern uintptr_t obdd_decompose(FILE *out, int n, obdd_t* p);


/* \brief   Return the total number of obdd nodes that have been created so far.
 */
extern uintmax_t obdd_nnodes(void); 


/* \brief Delete p and all non-terminal nodes below p. 
 */
extern void obdd_delete_all(obdd_t* p);


// v field must be accessed by this function, because it may have negative value during traversal of obdd.
static inline int obdd_label(obdd_t* p)
{
  return abs(p->v);
}


// v field must be accessed by this function, because it may have negative value during traversal of obdd.
static inline void obdd_setlabel(int v, obdd_t* p)
{
  p->v = abs(v);
}


/* \brief   Obtain the top terminal node.
 */
static inline obdd_t* obdd_top()
{
    if(top_node == NULL)
        top_node = obdd_node(INT_MAX, NULL, NULL);

    return top_node;
}


/* \brief   Obtain the bottom terminal node.
 */
static inline obdd_t* obdd_bot()
{
    if(bot_node == NULL)
        bot_node = obdd_node(INT_MAX, NULL, NULL);

    return bot_node;
}


/* \brief Decide if p is a terminal node.
 * \return true if p is a terminal node; false, otherwise.
 */
static inline int obdd_const(obdd_t* p)
{
    return p == obdd_bot() || p == obdd_top();
}
#endif /*OBDD_H*/
