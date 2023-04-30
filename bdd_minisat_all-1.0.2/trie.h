/** \file       trie.h
 *  \brief      trie
 *  \author     Takahisa Toda
 *  \note
 *  - Iterative implementation is selected in default setting.
 *  - Define TRIE_REC if recursive implementation is preferable. In that case, please take care of stack overflow.
 */
#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>
#include <assert.h>

#define WORDSIZE   (sizeof(unsigned int)*8)
static inline int  GET_NWORDS       (int len)                {return len > 0? (len-1)/WORDSIZE + 1: 0;}  // len: #bits
static inline int  DIGIT            (unsigned int *v, int i) {return (v[i/WORDSIZE] >> (i%WORDSIZE))%2;} // 0 <= i < #bits
static inline void SET_DIGIT        (unsigned int *v, int i) {v[i/WORDSIZE] |=   1U << (i%WORDSIZE);}
static inline void UNSET_DIGIT      (unsigned int *v, int i) {v[i/WORDSIZE] &= ~(1U << (i%WORDSIZE));}
static inline void UNSET_ALL_DIGIT  (unsigned int *v, int len) {int size = GET_NWORDS(len); for(int j = 0; j < size; j++) v[j] = 0;}

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/


typedef struct trie_node st_node;

/* \brief  trie*/
typedef struct st_trie {
    int             len;    //!< length of a bitvector
    st_node*        root;   //!< root node of a trie
    struct st_trie* nx;     //!< used for memory management purpose
    struct st_trie* pv;     //!< used for memory management purpose
} trie_t;


/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

/* \brief Finalize node management.
 * \note
 * - trie nodes and bit vectors are cleared.
 * - trie_t data structure is not cleared for a later use!
 * - Call trie_delete to destroy trie_t data structure.
 */
extern void     trie_finalize(void);

/* \brief Setup node management. If tries are already created, they are initialized.
 * \note 
 * - Call pior to any other function calls. 
 * - This can be also used to clear all existing tries, where length of each trie will not be changed.
 */
extern void     trie_initialize(void);

/* \brief   Create an empty trie.
 * \param   n   Length of a bitvector
 * \return  Created trie.
 */
extern trie_t*  trie_create  (int n);

/* \brief delete a specified trie.
 * \note to finish the usage of trie completely, call trie_finalize.
 */
extern void     trie_delete(trie_t *t);

/*  \brief  Insert a new node in a trie.
 *  \param  k   Bitvector(key), where the first bit is k[0] and the final bit is k[t->len-1].
 *  \param  v   Value associated with the bitvector
 *  \param  t   Trie
 */
extern void     trie_insert   (unsigned int *k, uintptr_t v, trie_t *t);


/*  \brief  Search a node with a specified key.
 *  \param  k   Bitvector(key), where the first bit is k[0] and the final bit is k[t->len-1].
 *  \param  t   Trie
 *  \return Associated value if a node is found; otherwise, (uintptr_t)NULL.
 */
extern uintptr_t    trie_search   (unsigned int *k, trie_t *t);

#ifdef TRIE_REC
/*  \brief  Print a trie structure in a dot format (for debug).
 *  \param  t   Trie
 *  \param  out File pointer
 */
extern void     trie_print    (trie_t *t, FILE *out);
#endif

#endif /*TRIE_H*/
