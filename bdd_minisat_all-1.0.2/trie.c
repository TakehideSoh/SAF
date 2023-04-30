/** \file     trie.c
 *  \brief    Implementation of binary trie
 *  \author   Takahisa Toda
 *  \see      R.Sedgewich, "Algorithms in C"
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "my_def.h"
#include "trie.h"

#define IS_EXT(h)   ((uintptr_t)(h)%2 == 1)
#define LEFT(h)     (((st_node*)(((uintptr_t)(h) / 2) * 2))->l)
#define RIGHT(h)    (((st_node*)(((uintptr_t)(h) / 2) * 2))->r)
#define KEY(h)      ((unsigned int*)LEFT(LEFT(h)))
#define VAL(h)      ((uintptr_t)RIGHT(LEFT(h)))


struct trie_node {
    struct  trie_node  *l;
    struct  trie_node  *r;
};

static int        isequal       (unsigned int *k1, unsigned int *k2, int len);
static st_node    *trie_getnode (int len, unsigned int *k, uintptr_t v);
static st_node    *trie_split   (st_node *p, st_node *q, int w);
#ifdef TRIE_REC
static st_node    *trie_insertR (unsigned int *k, int w, int len, uintptr_t v, st_node *h);
static uintptr_t  trie_searchR  (unsigned int *k, int w, int len, st_node *h);
static int        trie_printR   (int c, st_node *h, FILE *out);
#endif

#define FN_INIT_X   (1UL << 10)
#define VECS_INIT_X (1UL << 10)

static trie_t *trielist = NULL;

static st_node**        fn       = NULL;
static uintptr_t        fn_x     = 0;
static uintptr_t        fn_y     = 0;
static uintptr_t        fn_max_x = FN_INIT_X;
static const uintptr_t  fn_max_y = 64;

static unsigned int**   vecs        = NULL;
static uintptr_t        vecs_x      = 0;
static uintptr_t        vecs_y      = 0;
static uintptr_t        vecs_max_x  = VECS_INIT_X;
static const uintptr_t  vecs_max_y  = 64;


/* \brief Setup node management. If tries are already created, they are initialized.
 * \note 
 * - Call pior to any other function calls. 
 * - This can be also used to clear all existing tries, where length of each trie will not be changed.
 */
extern void trie_initialize(void)
{
    if (fn != NULL || vecs != NULL)
        trie_finalize();

    fn_x = 0;
    fn_y = 0;
    fn_max_x = FN_INIT_X;
    fn = (st_node**)malloc(sizeof(st_node*)*fn_max_y);
    ENSURE_TRUE_MSG(fn != NULL, "memory allocation failed");
    for (int i = 0; i < fn_max_y; i++)
        fn[i] = NULL;
    fn[0] = (st_node*)malloc(sizeof(st_node)*fn_max_x);
    ENSURE_TRUE_MSG(fn[0] != NULL, "memory allocation failed");

    vecs_x = 0;
    vecs_y = 0;
    vecs_max_x = VECS_INIT_X;
    vecs = (unsigned int**)malloc(sizeof(unsigned int*)*vecs_max_y);
    ENSURE_TRUE_MSG(vecs != NULL, "memory allocation failed");
    for (int i = 0; i < vecs_max_y; i++)
        vecs[i] = NULL;
    vecs[0] = (unsigned int*)malloc(sizeof(unsigned int)*vecs_max_x);
    ENSURE_TRUE_MSG(vecs[0] != NULL, "memory allocation failed");

    for (trie_t *p = trielist; p != NULL; p = p->nx) {
        p->root = (st_node*)((uintptr_t)NULL + 1);
    }
}


/* \brief Finalize node management.
 * \note
 * - trie nodes and bit vectors are cleared.
 * - trie_t data structure is not cleared for a later use!
 * - Call trie_delete to destroy trie_t data structure.
 */
extern void trie_finalize(void)
{
    if (fn != NULL) {
        for (int i = 0; i < fn_max_y; i++) {
            if (fn[i] != NULL) {
                free(fn[i]);
                fn[i] = NULL;
            }
        }
        free(fn);
        fn = NULL;
    }


    if (vecs != NULL) {
        for (int i = 0; i < vecs_max_y; i++) {
            if (vecs[i] != NULL) {
                free(vecs[i]);
                vecs[i] = NULL;
            }
        }
        free(vecs);
        vecs = NULL;
    }
}


static inline st_node *get_freenode(void)
{
    if (fn_x >= fn_max_x) {
        fn_max_x *= 2;
        fn_x = 0;
        fn_y++;
        assert(fn_y < fn_max_y);
        fn[fn_y] = (st_node*)malloc(sizeof(st_node)*fn_max_x);
        ENSURE_TRUE_MSG(fn[fn_y] != NULL, "memory allocation failed");
    }

    return &(fn[fn_y][fn_x++]);
}


static inline unsigned int *get_freevec(int n)
{
    if (vecs_x+n >= vecs_max_x) {
        vecs_max_x *= 2;
        vecs_x = 0;
        vecs_y++;
        assert(vecs_y < vecs_max_y);
        vecs[vecs_y] = (unsigned int*)malloc(sizeof(unsigned int)*vecs_max_x);
        ENSURE_TRUE_MSG(vecs[vecs_y] != NULL, "memory allocation failed");
    }

    unsigned int *t = &(vecs[vecs_y][vecs_x]);
    vecs_x += n;
    return t;
}


/*  \brief  Decide if k1 == k2.
 *  \param  k1    Bitvector
 *  \param  k2    Bitvector
 *  \param  len   Bit length
 *  \return 1 if equal; otherwise, 0.
 */
static int isequal(unsigned int *k1, unsigned int *k2, int len)
{
    const int nwords = GET_NWORDS(len);

    for (int i = 0; i < nwords; i++) {
        if (k1[i] != k2[i]) 
            return 0;
    }

    return 1; 
}


/*  \brief  Get a trie node.
 *  \param  len length of a bitvector
 *  \param  k   Bitvector(key)
 *  \param  v   Value associated with the bitvector.
 *  \return Pointer to an obtained trie node.
 */
static st_node *trie_getnode(int len, unsigned int *k, uintptr_t v)
{
    st_node *p = get_freenode();
    st_node *t = get_freenode();

    p->l = (st_node*)((uintptr_t)(t)+1); // left child holds a key-value pair.
    p->r = (st_node*)1;

    if(len > 0) {
        const int nwords  = GET_NWORDS(len);
        unsigned int *vec = get_freevec(nwords);
        for(int j = 0; j < nwords; j++) 
            vec[j] = k[j];
        t->l = (st_node*)(vec);
    } else {
        t->l = (st_node*)NULL;
    }
    t->r = (st_node*)v;

    return p;
}


/*  \brief  Split a trie so that it contains p and q.
 *  \param  p   Trie node
 *  \param  q   Trie node
 *  \param  w   Position in a bitvector
 */
static st_node *trie_split(st_node *p, st_node *q, int w)
#ifdef TRIE_REC
{
    st_node *t = trie_getnode(0, (unsigned int*)NULL, (uintptr_t)NULL);
    switch (DIGIT(KEY(p), w)*2 + DIGIT(KEY(q), w)) {
        case 0:  t->l = trie_split(p,q,w+1); break;
        case 1:  t->l = p; t->r =q;          break;
        case 2:  t->l = q; t->r =p;          break;
        case 3:  t->r = trie_split(p,q,w+1); break;
    }
    return t;
}
#else /*TRIE_ITERATION*/
{
    st_node tmp;
    st_node *prev = &tmp;
    int sgn = 0;

    for (int i = w; 1; i++) {
        st_node *t = trie_getnode(0, (unsigned int*)NULL, (uintptr_t)NULL);
        if (sgn)
            prev->l = t;
        else
            prev->r = t;
        prev = t;

        switch (DIGIT(KEY(p), i)*2 + DIGIT(KEY(q), i)) {
            case 0:  sgn  = 1;              break;
            case 1:  t->l = p; t->r = q;    return tmp.r;
            case 2:  t->l = q; t->r = p;    return tmp.r;
            case 3:  sgn  = 0;              break;
        }
    }

    return tmp.r;
}
#endif


/* \brief   Create an empty trie. 
 * \param   n   Length of a bitvector
 * \return  Created trie.
 * \note Make sure that trie_initialize is done.
 */
trie_t *trie_create(int n)
{
    trie_t *t = (trie_t*)malloc(sizeof(trie_t));
    ENSURE_TRUE_MSG(t != NULL, "memory allocation failed");

    t->root = (st_node*)((uintptr_t)NULL + 1);
    t->len  = n;
    if (trielist != NULL)
        trielist->pv = t; 
    t->nx = (trie_t*)trielist;
    t->pv = NULL;
    trielist = t;

    return t;
}


/* \brief delete a trie.
 * \note to finish the usage of trie completely, also call trie_finalize.
 */
void trie_delete(trie_t *t)
{
    if (t != NULL) {
        if (t->pv != NULL)
            t->pv->nx = t->nx;
        else
            trielist = t->nx;

        if (t->nx != NULL)
            t->nx->pv = t->pv;

        free(t);
    }
}


/*  \brief  Insert a new node in a trie.
 *  \param  k   Bitvector(key)
 *  \param  v   Value associated with the bitvector
 *  \param  t   Trie
 */
void trie_insert(unsigned int *k, uintptr_t v, trie_t *t)
{
  //printf("<");
  //for(int i = 0; i < t->len; i++) {
  //  printf("%d", DIGIT(k, i));
  //}
  //printf("\n");fflush(stdout);

#ifdef TRIE_REC
    t->root = trie_insertR(k, 0, t->len, v, t->root);

#else /*TRIE_ITERATION*/
    st_node *h = t->root;
    int len = t->len;

    st_node tmp;
    tmp.r = t->root;
    st_node *prev  = &tmp;
    int sgn = 0;

    for (int w = 0; 1; w++) {
        if (IS_EXT(h)) {
            if(sgn)
                prev->l = trie_getnode(len, k, v);
            else
                prev->r = trie_getnode(len, k, v);
            break;
        }

        if (IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h))) {
            if (sgn)
                prev->l = (!isequal(k, KEY(h),len))? trie_split(trie_getnode(len, k,v), h, w): h;
            else
                prev->r = (!isequal(k, KEY(h),len))? trie_split(trie_getnode(len, k,v), h, w): h;
            break;
        }

        if (sgn)
            prev->l = h;
        else
            prev->r = h;

        prev = h;
        sgn  = (DIGIT(k, w) == 0);
        h    = sgn? LEFT(h): RIGHT(h);
    }

    t->root = tmp.r;
#endif
}


#ifdef TRIE_REC
static st_node *trie_insertR(unsigned int *k, int w, int len, uintptr_t v, st_node *h)
{
    if (IS_EXT(h))
        return trie_getnode(len, k, v);

    if (IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h))) {
        if (!isequal(k, KEY(h), len))  
            return trie_split(trie_getnode(len, k,v), h, w);
        else
            return h;
    }

    if (DIGIT(k, w) == 0)  
        h->l = trie_insertR(k, w+1, len, v, LEFT(h));
    else
        h->r = trie_insertR(k, w+1, len, v, RIGHT(h));

  return h;
}
#endif

/*  \brief  Search a node with a specified key.
 *  \param  k   Bitvector(key)
 *  \param  t   Trie
 *  \return Associated value if a node is found; otherwise, (uintptr_t)NULL.
 */
uintptr_t trie_search(unsigned int *k, trie_t *t)
{
  //printf("?");
  //for(int i = 0; i < t->len; i++) {
  //  printf("%d", DIGIT(k,i));
  //}
  //printf("\n");fflush(stdout);

#ifdef TRIE_REC
  return trie_searchR(k, 0, t->len, t->root);

#else /*TRIE_ITERATION*/
    st_node *h = t->root;

    for (int w = 0; 1; w++) {
        if (IS_EXT(h))
            return (uintptr_t)NULL;

        if (IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h)))
            return isequal(k, KEY(h), t->len)? VAL(h): (uintptr_t)NULL;

        h = DIGIT(k, w) == 0? LEFT(h): RIGHT(h);
    }
#endif
}

#ifdef TRIE_REC
static uintptr_t trie_searchR(unsigned int *k, int w, int len, st_node *h)
{
    if (IS_EXT(h))
        return (uintptr_t)NULL;

    if (IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h)))
        return isequal(k, KEY(h), len)? VAL(h): (uintptr_t)NULL;

    if (DIGIT(k,w) == 0)
        return trie_searchR(k, w+1, len, LEFT(h));
    else
        return trie_searchR(k, w+1, len, RIGHT(h));
}
#endif


#ifdef TRIE_REC
/*  \brief  Print a trie structure as a dot format.
 *  \param  t   Trie
 *  \param  out File pointer
 */
void trie_print(trie_t *t, FILE *out)
{
    fprintf(out, "digraph trie {\n");
    trie_printR(1, t->root, out); 
    fprintf(out, "}\n");
}

static int trie_printR(int c, st_node *h, FILE *out)
{
    if (IS_EXT(h))
        return c;

    if (IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h))) {
        fprintf(out, "%d [shape=circle];\n", c);
        return c+1;
    } else if (IS_EXT(LEFT(h)) && !IS_EXT(RIGHT(h))) {
        int r = trie_printR(c, RIGHT(h), out);
        fprintf(out, "%d [shape=point];\n", r);
        fprintf(out, "%d -> %d;\n", r, r-1);
        return r+1;
    } else if (!IS_EXT(LEFT(h)) && IS_EXT(RIGHT(h))) {
        int l = trie_printR(c, LEFT(h), out);
        fprintf(out, "%d [shape=point];\n", l);
        fprintf(out, "%d -> %d [style=dotted];\n", l, l-1);
        return l+1;
    } else {
        int l = trie_printR(c, LEFT(h), out);
        int r = trie_printR(l, RIGHT(h), out);
        fprintf(out, "%d [shape=point];\n", r);
        fprintf(out, "%d -> %d [style=dotted];\n", r, l-1);
        fprintf(out, "%d -> %d;\n", r, r-1);
        return r+1;
    }
}
#endif

