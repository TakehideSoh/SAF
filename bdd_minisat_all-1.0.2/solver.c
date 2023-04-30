/**************************************************************************************************
MiniSat -- Copyright (c) 2005, Niklas Sorensson
http://www.cs.chalmers.se/Cs/Research/FormalMethods/MiniSat/

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/
// Modified to compile with MS Visual Studio 6.0 by Alan Mishchenko
// Modified to implement BDD-based AllSAT Solver on top of MiniSat by Takahisa Toda

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "solver.h"
#include "obdd.h"
#include "trie.h"

//=================================================================================================
// Debug:

//#define VERBOSEDEBUG

// For derivation output (verbosity level 2)
#ifdef NONBLOCKING
#define L_IND    "%d:%-*d"
#define L_ind    solver_dlevel(s),solver_dlevel(s)*3+3,solver_sublevel(s)
#else
#define L_IND    "%-*d"
#define L_ind    solver_dlevel(s)*3+3,solver_dlevel(s)
#endif /*NONBLOCKING*/

#define L_LIT    "%sx%d"
#define L_lit(p) lit_sign(p)?"~":"", (lit_var(p))

// Just like 'assert()' but expression will be evaluated in the release version as well.
static inline void check(int expr) { assert(expr); }

static void printlits(lit* begin, lit* end)
{
    int i;
    for (i = 0; i < end - begin; i++)
        printf(L_LIT" ",L_lit(begin[i]));
}

//=================================================================================================
// Random numbers:


// Returns a random float 0 <= x < 1. Seed must never be 0.
static inline double drand(double* seed) {
    int q;
    *seed *= 1389796;
    q = (int)(*seed / 2147483647);
    *seed -= (double)q * 2147483647;
    return *seed / 2147483647; }


// Returns a random integer 0 <= x < size. Seed must never be 0.
static inline int irand(double* seed, int size) {
    return (int)(drand(seed) * size); }


//=================================================================================================
// Predeclarations:

void sort(void** array, int size, int(*comp)(const void *, const void *));

//=================================================================================================
// Clause datatype + minor functions:

struct clause_t
{
#ifdef CUTSETCACHE
    lit minlit;  // literal with minimum variable index: this field must be placed before lits[0]
    lit maxlit;  // literal with maximum variable index: this field must be placed before lits[0]
#endif
    int size_learnt;
    lit lits[0];
};

static inline int   clause_size       (clause* c)          { return c->size_learnt >> 1; }
static inline lit*  clause_begin      (clause* c)          { return c->lits; }
static inline lit*  clause_end        (clause* c)          { return c->lits + clause_size(c); } // Note: the next position of the last literal
#ifdef CUTSETCACHE
static inline lit   clause_minlit     (clause* c)          { return c->minlit; }
static inline lit   clause_maxlit     (clause* c)          { return c->maxlit; }
#endif
static inline int   clause_learnt     (clause* c)          { return c->size_learnt & 1; }
static inline float clause_activity   (clause* c)          { return *((float*)&c->lits[c->size_learnt>>1]); }
static inline void  clause_setactivity(clause* c, float a) { *((float*)&c->lits[c->size_learnt>>1]) = a; }

//=================================================================================================
// Encode literals in clause pointers:

clause* clause_from_lit (lit l)     { return (clause*)((unsigned long)l + (unsigned long)l + 1);  }
bool    clause_is_lit   (clause* c) { return ((unsigned long)c & 1);                              }
lit     clause_read_lit (clause* c) { return (lit)((unsigned long)c >> 1);                        }

//=================================================================================================
// Simple helpers:

static inline int     solver_dlevel(solver* s)    { return veci_size(&s->trail_lim); }
#ifdef NONBLOCKING
static inline int     solver_sublevel(solver* s) { return veci_size(&s->subtrail_lim); }
#endif /*NONBLOCKING*/
static inline vecp*   solver_read_wlist     (solver* s, lit l){ return &s->wlists[l]; }
static inline void    vecp_remove(vecp* v, void* e)
{
    void** ws = vecp_begin(v);
    int    j  = 0;

    for (; ws[j] != e  ; j++);
    assert(j < vecp_size(v));
    for (; j < vecp_size(v)-1; j++) ws[j] = ws[j+1];
    vecp_resize(v,vecp_size(v)-1);
}

static inline lit solver_assumedlit(solver *s, int level) {assert(level >= 1); return s->trail[veci_begin(&s->trail_lim)[level-1]];}

#ifdef CUTSETCACHE
static void solver_setminmaxlit(solver *s)
{

    const int m   = solver_nclauses(s);
    clause** cls  = (clause**)vecp_begin(&s->clauses);

    for (int i = 0; i < m; i++) {
        cls[i]->minlit = *(clause_begin(cls[i]));
        cls[i]->maxlit = *(clause_end(cls[i])-1);

        for (int j = 0; j < clause_size(cls[i]); j++) {
            const lit l = clause_begin(cls[i])[j];
            if (lit_var(l) > lit_var(cls[i]->maxlit))
                cls[i]->maxlit = l; 
            if (lit_var(l) < lit_var(cls[i]->minlit))
                cls[i]->minlit = l;
        }
    }
}
#endif /*CUTSETCACHE*/

static void solver_printtrail(solver *s){
    int lev = -1;
    int sublev = s->root_level;

    printf("--------------------------------------------------------------------------------");fflush(stdout);
    for (int i = 0; i <= s->qtail-1; i++) {
        lit t = s->trail[i];
        if (lev < s->levels[lit_var(t)]) {
            lev = s->levels[lit_var(t)];
            printf("\n#%d ", lev); // newline for each level 
        }
#ifdef NONBLOCKING
        if (sublev < s->sublevels[lit_var(t)]) {
            sublev = s->sublevels[lit_var(t)];
            printf("| "); // separator of sublevels
        }
#endif /*NONBLOCKING*/
        printf(L_LIT"%s ", L_lit(t), s->reasons[lit_var(t)] == (clause*)0? "*":"");// "*" means having NULL antecedent.
    }
    printf("\n\n");
    printf("\n--------------------------------------------------------------------------------\n");fflush(stdout);
}

#ifdef NONBLOCKING
static void solver_printgencls(solver *s) // for debug
{
    printf("#generated_clauses %d:\n", vecp_size(&s->generated_clauses));
    for (int i = 0; i < vecp_size(&s->generated_clauses); i++) {
        veci *v = (veci*)vecp_begin(&s->generated_clauses)[i];
        for (int j = 0; j < veci_size(v); j++) 
            printf(L_LIT" ", L_lit(veci_begin(v)[j]));
        printf("\n");
    }
    printf("\n");fflush(stdout);
}
#endif /*NONBLOCKING*/

// cl3 <- resolution of cl1 and cl2, where the initial literals of cl1 and cl2 should be opposite.
// the initial literal of cl3 must be of the highest level.
#ifdef NONBLOCKING
static void perform_resolution(solver *s, veci *cl1, veci *cl2, veci *cl3)
{
    assert(veci_size(cl1) > 0);
    assert(veci_size(cl2) > 0);
    assert(*veci_begin(cl1) == lit_neg(*veci_begin(cl2)));
    lbool *ws = s->tags; // working space

    veci_resize(cl3, 0);
    for(int i = 1; i < veci_size(cl1); i++) {
        lit t = veci_begin(cl1)[i];
        veci_push(cl3, t);
        ws[lit_var(t)] = lit_sign(t)? l_False: l_True;
    }

    for (int i = 1; i < veci_size(cl2); i++) {
        const lit t  = veci_begin(cl2)[i];

        if (ws[lit_var(t)] == l_Undef) {
            veci_push(cl3, t);
        }
        /*else if (ws[lit_var(t)] == l_True && lit_sign(t) == 1 
               || ws[lit_var(t)] == l_False && lit_sign(t) == 0) {
            int j;
            for(j = 0; j < veci_size(cl3) && lit_var(veci_begin(cl3)[j]) != lit_var(t); j++) ;
            assert(j < veci_size(cl3));
            while(++j < veci_size(cl3))
                veci_begin(cl3)[j-1] = veci_begin(cl3)[j];
            veci_resize(cl3, veci_size(cl3)-1);
        }*/
        assert(!(ws[lit_var(t)] == l_True && lit_sign(t) == 1 || ws[lit_var(t)] == l_False && lit_sign(t) == 0));

        ws[lit_var(t)] = l_Undef; // initialize ws
    }

    if (veci_size(cl3) > 0) {
        int highest = s->levels[lit_var(veci_begin(cl3)[0])];
        int pos     = 0;
        ws[lit_var(veci_begin(cl3)[0])] = l_Undef;
        for (int i = 1; i < veci_size(cl3); i++) { // initialize ws and find the highest level and its position
            ws[lit_var(veci_begin(cl3)[i])] = l_Undef;
            if (highest < s->levels[lit_var(veci_begin(cl3)[i])]) {
                highest = s->levels[lit_var(veci_begin(cl3)[i])];
                pos     = i;
            }
        }

#ifdef DEBUG
        for (int i = 0; i < s->size; i++)
            assert(ws[i] == l_Undef); // for debug
#endif

        lit tmp = veci_begin(cl3)[0];
        veci_begin(cl3)[0] = veci_begin(cl3)[pos];
        veci_begin(cl3)[pos] = tmp;
    }
}
#endif /*NONBLOCKING*/


void totalup_stats(solver *s)
{
    intptr_t sols = s->root->aux;

    uint64 size = (uint64)obdd_complete(s->root);

    // total up obdd size
    s->stats.obddsize += size;

    // total up number of solutions
#ifdef GMP
    mpz_t result;
    mpz_init(result);
    mpz_set_ui(result,0);
    obdd_nsols_gmp(result, s->size, s->root);
    mpz_add(s->stats.tot_solutions_gmp, s->stats.tot_solutions_gmp, result); 
    mpz_clear(result);

#else
    if(s->stats.tot_solutions <= ULONG_MAX - sols)
        s->stats.tot_solutions += sols; // Note: s->root->aux can not count more than INTPTR_MAX!
    else
        s->stats.tot_solutions = ULONG_MAX;
#endif
}


static inline void solver_printstatus(solver *s)
{
    if (s->verbosity < 1) 
        return;

    printf("%.1f", (float)(clock() - s->stats.clk)/(float)(CLOCKS_PER_SEC));
    printf("\t%ju", s->stats.conflicts);
    printf("\t%ju", s->stats.propagations);

    if (s->stats.refreshes == 0) {
        printf("\t%jd", s->root->aux);
        if (s->root->aux >= INTPTR_MAX)
            printf("+");
    } else {
        printf("\t-");
    }
        
    printf("\t\t%d", vecp_size(&s->clauses));
    printf("\t\t%d", vecp_size(&s->learnts));
    printf("\t\t%ju", obdd_nnodes());
    printf("\n");
}

//=================================================================================================
// Variable order functions:

static inline void order_update(solver* s, int v) // updateorder
{
    int*    orderpos = s->orderpos;
    double* activity = s->activity;
    int*    heap     = veci_begin(&s->order);
    int     i        = orderpos[v];
    int     x        = heap[i];
    int     parent   = (i - 1) / 2;

    assert(s->orderpos[v] != -1);

    while (i != 0 && activity[x] > activity[heap[parent]]){
        heap[i]           = heap[parent];
        orderpos[heap[i]] = i;
        i                 = parent;
        parent            = (i - 1) / 2;
    }
    heap[i]     = x;
    orderpos[x] = i;
}

static inline void order_assigned(solver* s, int v) 
{
}

static inline void order_unassigned(solver* s, int v) // undoorder
{
    int* orderpos = s->orderpos;
    if (orderpos[v] == -1){
        orderpos[v] = veci_size(&s->order);
        veci_push(&s->order,v);
        order_update(s,v);
    }
}

/*static int  order_select(solver* s, float random_var_freq) // selectvar
{
    int*    heap;
    double* activity;
    int*    orderpos;

    lbool* values = s->assigns;

    // Random decision:
    if (drand(&s->random_seed) < random_var_freq){
        int next = irand(&s->random_seed,s->size);
        assert(next >= 0 && next < s->size);
        if (values[next] == l_Undef)
            return next;
    }

    // Activity based decision:

    heap     = veci_begin(&s->order);
    activity = s->activity;
    orderpos = s->orderpos;


    while (veci_size(&s->order) > 0){
        int    next  = heap[0];
        int    size  = veci_size(&s->order)-1;
        int    x     = heap[size];

        veci_resize(&s->order,size);

        orderpos[next] = -1;

        if (size > 0){
            double act   = activity[x];

            int    i     = 0;
            int    child = 1;


            while (child < size){
                if (child+1 < size && activity[heap[child]] < activity[heap[child+1]])
                    child++;

                assert(child < size);

                if (act >= activity[heap[child]])
                    break;

                heap[i]           = heap[child];
                orderpos[heap[i]] = i;
                i                 = child;
                child             = 2 * child + 1;
            }
            heap[i]           = x;
            orderpos[heap[i]] = i;
        }

        if (values[next] == l_Undef)
            return next;
    }

    return var_Undef;
}*/

//=================================================================================================
// Activity functions:

static inline void act_var_rescale(solver* s) {
    double* activity = s->activity;
    int i;
    for (i = 0; i < s->size; i++)
        activity[i] *= 1e-100;
    s->var_inc *= 1e-100;
}

static inline void act_var_bump(solver* s, int v) {
    double* activity = s->activity;
    if ((activity[v] += s->var_inc) > 1e100)
        act_var_rescale(s);

    //printf("bump %d %f\n", v-1, activity[v]);

    if (s->orderpos[v] != -1)
        order_update(s,v);

}

static inline void act_var_decay(solver* s) { s->var_inc *= s->var_decay; }

static inline void act_clause_rescale(solver* s) {
    clause** cs = (clause**)vecp_begin(&s->learnts);
    int i;
    for (i = 0; i < vecp_size(&s->learnts); i++){
        float a = clause_activity(cs[i]);
        clause_setactivity(cs[i], a * (float)1e-20);
    }
    s->cla_inc *= (float)1e-20;
}


static inline void act_clause_bump(solver* s, clause *c) {
    float a = clause_activity(c) + s->cla_inc;
    clause_setactivity(c,a);
    if (a > 1e20) act_clause_rescale(s);
}

static inline void act_clause_decay(solver* s) { s->cla_inc *= s->cla_decay; }


//=================================================================================================
// Clause functions:

/* pre: size > 1 && no variable occurs twice
 */
static clause* clause_new(solver* s, lit* begin, lit* end, int learnt)
{
    int size;
    clause* c;
    int i;

    assert(end - begin > 1);
    assert(learnt >= 0 && learnt < 2);
    size           = end - begin;
    c              = (clause*)malloc(sizeof(clause) + sizeof(lit) * size + learnt * sizeof(float));
    c->size_learnt = (size << 1) | learnt;
    assert(((unsigned long)c & 1) == 0);

    for (i = 0; i < size; i++)
        c->lits[i] = begin[i];

    if (learnt)
        *((float*)&c->lits[size]) = 0.0;

    assert(begin[0] >= 0);
    assert(begin[0] < s->size*2);
    assert(begin[1] >= 0);
    assert(begin[1] < s->size*2);

    assert(lit_neg(begin[0]) < s->size*2);
    assert(lit_neg(begin[1]) < s->size*2);

    //vecp_push(solver_read_wlist(s,lit_neg(begin[0])),(void*)c);
    //vecp_push(solver_read_wlist(s,lit_neg(begin[1])),(void*)c);

    vecp_push(solver_read_wlist(s,lit_neg(begin[0])),(void*)(size > 2 ? c : clause_from_lit(begin[1])));
    vecp_push(solver_read_wlist(s,lit_neg(begin[1])),(void*)(size > 2 ? c : clause_from_lit(begin[0])));

    return c;
}


static void clause_remove(solver* s, clause* c)
{
    lit* lits = clause_begin(c);
    assert(lit_neg(lits[0]) < s->size*2);
    assert(lit_neg(lits[1]) < s->size*2);

    //vecp_remove(solver_read_wlist(s,lit_neg(lits[0])),(void*)c);
    //vecp_remove(solver_read_wlist(s,lit_neg(lits[1])),(void*)c);

    assert(lits[0] < s->size*2);
    vecp_remove(solver_read_wlist(s,lit_neg(lits[0])),(void*)(clause_size(c) > 2 ? c : clause_from_lit(lits[1])));
    vecp_remove(solver_read_wlist(s,lit_neg(lits[1])),(void*)(clause_size(c) > 2 ? c : clause_from_lit(lits[0])));

    if (clause_learnt(c)){
        s->stats.learnts--;
        s->stats.learnts_literals -= clause_size(c);
    }else{
        s->stats.clauses--;
        s->stats.clauses_literals -= clause_size(c);
    }

    free(c);
}


#ifdef CUTSETCACHE
static void clause_remove_nofree(solver* s, clause* c)
{
    lit* lits = clause_begin(c);
    assert(lit_neg(lits[0]) < s->size*2);
    assert(lit_neg(lits[1]) < s->size*2);

    //vecp_remove(solver_read_wlist(s,lit_neg(lits[0])),(void*)c);
    //vecp_remove(solver_read_wlist(s,lit_neg(lits[1])),(void*)c);

    assert(lits[0] < s->size*2);
    vecp_remove(solver_read_wlist(s,lit_neg(lits[0])),(void*)(clause_size(c) > 2 ? c : clause_from_lit(lits[1])));
    vecp_remove(solver_read_wlist(s,lit_neg(lits[1])),(void*)(clause_size(c) > 2 ? c : clause_from_lit(lits[0])));

    if (clause_learnt(c)){
        s->stats.learnts--;
        s->stats.learnts_literals -= clause_size(c);
    }else{
        s->stats.clauses--;
        s->stats.clauses_literals -= clause_size(c);
    }

    //free(c);
}
#endif

static lbool clause_simplify(solver* s, clause* c)
{
    lit*   lits   = clause_begin(c);
    lbool* values = s->assigns;
    int i;

    assert(solver_dlevel(s) == 0);

    for (i = 0; i < clause_size(c); i++){
        lbool sig = !lit_sign(lits[i]); sig += sig - 1;
        if (values[lit_var(lits[i])] == sig)
            return l_True;
    }
    return l_False;
}



static lbool clause_simplify_noprop_until(solver* s, clause* c, int var) // evaluate clause without information of unit propagation
{
    lit*   lits   = clause_begin(c);
    lbool* values = s->assigns;

    for (int i = 0; i < clause_size(c); i++){
        lbool sig = !lit_sign(lits[i]); sig += sig - 1;
        if (lit_var(lits[i]) <= var  && values[lit_var(lits[i])] == sig) 
            return l_True;
    }
    return l_False;
}



#ifdef NONBLOCKING
static lbool clause_isasserting(solver* s, veci* c) // After decision, place unit literal, if exists, at the begining of the clause.
{
    lbool* values = s->assigns;

    int i,j,k;
    lit*   lits   = veci_begin(c);
    for (i = j = 0; i < veci_size(c); i++){
        lbool sig = !lit_sign(lits[i]); sig += sig - 1;
        if (values[lit_var(lits[i])] == l_Undef) 
            k=i, j++;
        if (values[lit_var(lits[i])] == sig || j > 1) 
            return l_False; // if c is satisfied or there are at least two undefined variables.
    }

    if(j == 1) {
      lit t   = lits[0];
      lits[0] = lits[k];
      lits[k] = t;
      return l_True;
    } else {
      return l_False;
    }
}
#endif /*NONBLOCKING*/

//=================================================================================================
// Cache-related functions:


#ifdef CUTSETCACHE
static void solver_setcutsets(solver* s)
{

    const int nvars = s->size;
    clause** cls    = (clause**)vecp_begin(&s->clauses);
    int*     cw     = s->cutwidth;

    for (int i = 0; i < nvars; i++)
        cw[i] = 0;

    const int m = solver_nclauses(s);
    for (int i = 0; i < m; i++) {
        int j = lit_var(clause_minlit(cls[i]));
        cw[j] += 1;

        int k = lit_var(clause_maxlit(cls[i]));
        cw[k] -= 1;
    }

    int max = 0; 
    for (int i = 1; i < nvars; i++) {
        cw[i] += cw[i-1];
        if (max < cw[i])
            max = cw[i];
    }
    s->maxcutwidth = max;

    for (int i = 0; i < nvars; i++) {
        if (s->cutsets[i] != NULL)
            free(s->cutsets[i]);
        s->cutsets[i] = (clause**)malloc(sizeof(clause*)*s->cutwidth[i]);
        assert(s->cutsets[i] != NULL);
    }

    int* w = (int*)malloc(sizeof(int)*nvars);  // working space
    assert(w != NULL);
    for (int i = 0; i < nvars; i++)
        w[i] = 0;
    for (int i = 0; i < m; i++) {
        for (int j = lit_var(clause_minlit(cls[i])); j < lit_var(clause_maxlit(cls[i])); j++) {
            assert(w[j] < s->cutwidth[j]);
            s->cutsets[j][w[j]++] = cls[i];
        }
    }

    free(w);
}

#else /*SEPARATORCACHE*/
static void solver_setseparators(solver* s) 
{
    const int nvars = s->size;
    int* w = (int*)malloc(sizeof(int)*nvars); // working space
    assert(w != NULL);
    for (int i = 0; i < nvars; i++)
        w[i] = i;

    const int m = solver_nclauses(s);
    clause **cls = (clause**)vecp_begin(&s->clauses);
    for (int i = 0; i < m; i++) {
        const int v = lit_var(*(clause_end(cls[i])-1));
        for (lit* l = clause_begin(cls[i]); l < clause_end(cls[i]); l++) {
        if (w[lit_var(*l)] < v)
            w[lit_var(*l)] = v;
        }
    }

    int* pw = s->pathwidth;
    for (int i = nvars-1; i >= 0; i--) {
        pw[i] = 1;
        pw[w[i]]--;
    }

    int max = 0;
    for (int i = 1; i < nvars; i++) {
        pw[i] += pw[i-1];
        assert(pw[i] >= 0);
        if (max < pw[i])
            max = pw[i];
    }
    s->maxpathwidth = max;

    for (int i = 0; i < nvars; i++) {
        if (s->separators[i] != NULL)
            free(s->separators[i]);
        s->separators[i] = (int*)malloc(sizeof(int)*s->pathwidth[i]);
        assert(s->separators[i] != NULL);
    }

    for (int i = nvars-1; i >= 0; i--) {
        const int k = w[i];
        w[i] = 0; // w[i] now means the position to insert a new element in the i-th separator. 
        for (int j = i; j < k; j++) {
            assert(w[j] < s->pathwidth[j]);
            s->separators[j][w[j]++] = i;
        }
    }

  /*for (int i = 0; i < nvars; i++) {
    assert(w[i] == s->pathwidth[i]);
    printf("%d: ", i+1);
    for (int j = 0; j < w[i]; j++) {
      printf("%d ", s->separators[i][j]+1);
    }
    printf("\n");
  }*/
    free(w);
}
#endif

static void solver_makecache(solver* s, unsigned int* vec, int i)
{
#ifdef CUTSETCACHE
    const int cutwidth  = s->cutwidth[i];

    UNSET_ALL_DIGIT(vec, cutwidth);
    for (int j = 0; j < cutwidth; j++) {
        if (clause_simplify_noprop_until(s, s->cutsets[i][j], i) == l_True)    
            SET_DIGIT(vec, j);
    }

#else /*SEPARATORCACHE*/
    const int pathwidth  = s->pathwidth[i];
    UNSET_ALL_DIGIT(vec, pathwidth);
    for (int j = 0; j < pathwidth; j++) {
        if (s->assigns[s->separators[i][j]] == l_True)    
            SET_DIGIT(vec, j);
    }
#endif

    veci_push(&s->cachedvars, i);
}


static void solver_initcache(solver *s)
{
    for (int i = 0; i < vecp_size(&s->bitvecs); i++) {
        if(vecp_begin(&s->bitvecs)[i] != NULL)
            free(vecp_begin(&s->bitvecs)[i]);
        vecp_begin(&s->bitvecs)[i] = NULL;
    }
    vecp_resize(&s->bitvecs, 0);

    trie_initialize();

#ifdef CUTSETCACHE
    solver_setminmaxlit(s);
    solver_setcutsets(s);

    for (int i = 0; i < s->size; i++) {
        s->cache[i]  = trie_create(s->cutwidth[i]);
        const int nwords = GET_NWORDS(s->cutwidth[i]);
        unsigned int *vec = (unsigned int*)malloc(sizeof(unsigned int) * nwords);
        assert(vec != NULL);
        for (int j = 0; j < nwords; j++)
            vec[j] = 0;
        vecp_push(&s->bitvecs, (void*)vec);
    }

#else  /*SEPARATORCACHE*/
    solver_setseparators(s);

    for (int i = 0; i < s->size; i++) {
        s->cache[i]  = trie_create(s->pathwidth[i]);
        const int nwords = GET_NWORDS(s->pathwidth[i]);
        unsigned int *vec = (unsigned int*)malloc(sizeof(unsigned int) * nwords);
        assert(vec != NULL);
        for (int j = 0; j < nwords; j++)
            vec[j] = 0;
        vecp_push(&s->bitvecs, (void*)vec);
    }
#endif
}


static void solver_insertcacheuntil(solver* s, int level)
{
    // s->obddpath holds the latest path added to OBDD.  
    if (!(vecp_size(&s->obddpath) > 0)) 
        return;

    const int k = (level >= s->root_level)? lit_var(solver_assumedlit(s,level+1)): 0;
    int j = 0;

    for (int i = 0; i < vecp_size(&s->obddpath)-1; i++) { 
        obdd_t* p = vecp_begin(&s->obddpath)[i];
        assert(obdd_label(p) == i+1);
        assert(s->assigns[i] != l_Undef);
        obdd_t* q = s->assigns[i] == l_True? p->hi: p->lo;

        if (q != vecp_begin(&s->obddpath)[i+1]) {
            if (k <= i) 
                vecp_resize(&s->obddpath, k+1);
            return;
        }
        if (i < k)  
            continue;

        int* vars = veci_begin(&s->cachedvars);
        int  len  = veci_size(&s->cachedvars);
        for (; j < len && vars[j] < i; j++) ;
        if (j < len && vars[j] == i) // insert only when cache is created.
            trie_insert((unsigned int*)vecp_begin(&s->bitvecs)[i], (intptr_t)vecp_begin(&s->obddpath)[i+1], s->cache[i]);
    }

    if (k+1 < vecp_size(&s->obddpath)) 
        vecp_resize(&s->obddpath, k+1);
}


#ifdef NONBLOCKING
static void solver_refreshobdd(solver *s)
{
    //printf("refreshing obdd...");
    s->stats.refreshes++;

    totalup_stats(s);

    if (s->out != NULL) {
        //printf("\tdecomposing bdd...");fflush(stdout);
        obdd_decompose(s->out, s->size, s->root);
    }

    obdd_delete_all(s->root);
    s->root = obdd_node(1, NULL, NULL);

    trie_initialize();
    vecp_resize(&s->obddpath, 0);
    veci_resize(&s->cachedvars, 0);
    //printf("\tfin\n");fflush(stdout);
}
#endif /*NONBLOCKING*/

//=================================================================================================
// Minor (solver) functions:

void solver_setnvars(solver* s,int n)
{
    int var;

    if (s->cap < n){

        while (s->cap < n) s->cap = s->cap*2+1;

        s->wlists    = (vecp*)   realloc(s->wlists,   sizeof(vecp)*s->cap*2);
        s->activity  = (double*) realloc(s->activity, sizeof(double)*s->cap);
        s->assigns   = (lbool*)  realloc(s->assigns,  sizeof(lbool)*s->cap);
        s->orderpos  = (int*)    realloc(s->orderpos, sizeof(int)*s->cap);
        s->reasons   = (clause**)realloc(s->reasons,  sizeof(clause*)*s->cap);
        s->levels    = (int*)    realloc(s->levels,   sizeof(int)*s->cap);
#ifdef NONBLOCKING
        s->sublevels = (int*)    realloc(s->sublevels,   sizeof(int)*s->cap);
#endif /*NONBLOCKING*/
        s->tags      = (lbool*)  realloc(s->tags,     sizeof(lbool)*s->cap);
        s->trail     = (lit*)    realloc(s->trail,    sizeof(lit)*s->cap);
        s->cache     = (trie_t**)  realloc(s->cache,    sizeof(trie_t*)*s->cap);
#ifdef CUTSETCACHE
        s->cutwidth  = (int*)    realloc(s->cutwidth, sizeof(int)*s->cap);
        s->cutsets   = (clause***)  realloc(s->cutsets,  sizeof(clause**)*s->cap);
#else /*SEPARATORCACHE*/
        s->pathwidth = (int*)    realloc(s->pathwidth,     sizeof(int)*s->cap);
        s->separators = (int**)  realloc(s->separators,  sizeof(int*)*s->cap);
#endif
    }

    for (var = s->size; var < n; var++){
        vecp_new(&s->wlists[2*var]);
        vecp_new(&s->wlists[2*var+1]);
        s->activity [var] = 0;
        s->assigns  [var] = l_Undef;
        s->orderpos [var] = veci_size(&s->order);
        s->reasons  [var] = (clause*)0;
        s->levels   [var] = 0;
#ifdef NONBLOCKING
        s->sublevels[var] = 0;
#endif /*NONBLOCKING*/
        s->tags     [var] = l_Undef;
#ifdef CUTSETCACHE
        s->cutwidth [var] = 0;
        s->cutsets  [var] = NULL;
#else /*SEPARATORCACHE*/
        s->pathwidth  [var] = 0;
        s->separators [var] = NULL;
#endif
        
        /* does not hold because variables enqueued at top level will not be reinserted in the heap
           assert(veci_size(&s->order) == var); 
         */
        veci_push(&s->order,var);
        order_update(s, var);
    }

    s->size = n > s->size ? n : s->size;
}


static inline bool enqueue(solver* s, lit l, clause* from)
{
    lbool* values = s->assigns;
    int    v      = lit_var(l);
    lbool  val    = values[v];
/* #ifdef VERBOSEDEBUG */
/*     printf(L_IND"enqueue("L_LIT")", L_ind, L_lit(l)); */
/*     if (from == 0) */
/*         printf(" with null ant."); */
/*     if (from != 0) { */
/*         printf(L_IND"implied by {", L_ind); */
/*         lit *lits; */
/*         int size; */
/*         lit tmp; */
/*         if (clause_is_lit(from)) { */
/*             tmp = clause_read_lit(from); */
/*             lits = &tmp; */
/*             size = 1; */
/*         } else { */
/*             lits = clause_begin(from); */
/*             size = clause_size(from); */
/*         } */
/*         for (int i = 0; i < size; i++) printf(" "L_LIT, L_lit(lits[i])); */
/*         printf("}"); */
/*     } */
/*     printf("\n"); */
/* #endif */

    lbool sig = !lit_sign(l); sig += sig - 1;
    if (val != l_Undef){
        return val == sig;
    }else{
        // New fact -- store it.
/* #ifdef VERBOSEDEBUG */
/*         printf(L_IND"bind("L_LIT")\n", L_ind, L_lit(l)); */
/* #endif */
        int*     levels  = s->levels;
#ifdef NONBLOCKING
        int*     sublevels  = s->sublevels;
#endif /*NONBLOCKING*/
        clause** reasons = s->reasons;

        values [v] = sig;
        levels [v] = solver_dlevel(s);
#ifdef NONBLOCKING
        sublevels [v] = solver_sublevel(s);
#endif /*NONBLOCKING*/
        reasons[v] = from;
        s->trail[s->qtail++] = l;

        order_assigned(s, v);
        return true;
    }
}


static inline void assume(solver* s, lit l){
    assert(s->qtail == s->qhead);
    assert(s->assigns[lit_var(l)] == l_Undef);
#ifdef VERBOSEDEBUG
    printf(L_IND"assume("L_LIT")\n", L_ind, L_lit(l));
#endif
    veci_push(&s->trail_lim,s->qtail);
#ifdef NONBLOCKING
    veci_push(&s->subtrail_lim,s->qtail);
#endif /*NONBLOCKING*/
    enqueue(s,l,(clause*)0);
}


static inline void solver_canceluntil(solver* s, int level) {
    lit*     trail;   
    lbool*   values;  
    clause** reasons; 
    int      bound;
    int      c;
    
    if (solver_dlevel(s) <= level)
        return;

    trail   = s->trail;
    values  = s->assigns;
    reasons = s->reasons;
    bound   = (veci_begin(&s->trail_lim))[level];

    s->nextvar  = lit_var(trail[bound]);

    {
      int* vars = veci_begin(&s->cachedvars);
      int i;
      for (i = veci_size(&s->cachedvars)-1; i >= 0 && s->nextvar <= vars[i]; i--) ;
      veci_resize(&s->cachedvars, i+1);
    }

#ifdef NONBLOCKING
    int sublevel;
    lit t = s->trail[veci_begin(&s->trail_lim)[level]-1]; // the last literal at the target level
    sublevel = level > s->root_level? s->sublevels[lit_var(t)] : level; // convert level to sublevel
#endif /*NONBLOCKING*/

    for (c = s->qtail-1; c >= bound; c--) {
        int     x  = lit_var(trail[c]);
        values [x] = l_Undef;
        reasons[x] = (clause*)0;
    }

    for (c = s->qhead-1; c >= bound; c--)
        order_unassigned(s,lit_var(trail[c]));

    s->qhead = s->qtail = bound;
    veci_resize(&s->trail_lim,level);
#ifdef NONBLOCKING
    veci_resize(&s->subtrail_lim,sublevel);
#endif /*NONBLOCKING*/
}

#ifdef NONBLOCKING
static clause *solver_record(solver* s, veci* cls)
{
    lit*    begin = veci_begin(cls);
    lit*    end   = begin + veci_size(cls);
    clause* c     = (veci_size(cls) > 1) ? clause_new(s,begin,end,1) : (clause*)0;
    assert(veci_size(cls) > 0);
    if (clause_isasserting(s,cls) == l_True) {
        // this may be a literal with null antecedent, in which a new sublevel is not defined.
        // However, this does not matter because solver_analyze is modifed to handle this case.
        enqueue(s,veci_begin(cls)[0],c); 
    }

    if (c != 0) {
        vecp_push(&s->learnts,c);
        act_clause_bump(s,c);
        s->stats.learnts++;
        s->stats.learnts_literals += veci_size(cls);
    }

    return c;
}

static clause *solver_record_noenqueue(solver* s, veci* cls)
{
    lit*    begin = veci_begin(cls);
    lit*    end   = begin + veci_size(cls);
    clause* c     = (veci_size(cls) > 1) ? clause_new(s,begin,end,1) : (clause*)0;
    assert(veci_size(cls) > 0);

    if (c != 0) {
        vecp_push(&s->learnts,c);
        act_clause_bump(s,c);
        s->stats.learnts++;
        s->stats.learnts_literals += veci_size(cls);
    }

    return c;
}

#else
static void solver_record(solver* s, veci* cls)
{
    lit*    begin = veci_begin(cls);
    lit*    end   = begin + veci_size(cls);
    clause* c     = (veci_size(cls) > 1) ? clause_new(s,begin,end,1) : (clause*)0;
    enqueue(s,*begin,c);

    assert(veci_size(cls) > 0);

    if (c != 0) {
        vecp_push(&s->learnts,c);
        act_clause_bump(s,c);
        s->stats.learnts++;
        s->stats.learnts_literals += veci_size(cls);
    }
}
#endif /*NONBLOCKING*/


/*static double solver_progress(solver* s) 
{
    lbool*  values = s->assigns;
    int*    levels = s->levels;
    int     i;

    double  progress = 0;
    double  F        = 1.0 / s->size;
    for (i = 0; i < s->size; i++)
        if (values[i] != l_Undef)
            progress += pow(F, levels[i]);
    return progress / s->size;
}*/

//=================================================================================================
// Major methods:

static bool solver_lit_removable(solver* s, lit l, int minl)
{
    lbool*   tags    = s->tags;
    clause** reasons = s->reasons;
#ifdef NONBLOCKING
#ifdef DLEVEL
    int*     levels  = s->levels;
#else
    int*     levels  = s->sublevels;
#endif
#else
    int*     levels  = s->levels;
#endif /*NONBLOCKING*/
    int      top     = veci_size(&s->tagged);

    assert(lit_var(l) >= 0 && lit_var(l) < s->size);
    assert(reasons[lit_var(l)] != 0);
    veci_resize(&s->stack,0);
    veci_push(&s->stack,lit_var(l));

    while (veci_size(&s->stack) > 0){
        clause* c;
        int v = veci_begin(&s->stack)[veci_size(&s->stack)-1];
        assert(v >= 0 && v < s->size);
        veci_resize(&s->stack,veci_size(&s->stack)-1);
        assert(reasons[v] != 0);
        c    = reasons[v];

        if (clause_is_lit(c)){
            int v = lit_var(clause_read_lit(c));
            if (tags[v] == l_Undef && levels[v] != 0){
                if (reasons[v] != 0 && ((1 << (levels[v] & 31)) & minl)){
                    veci_push(&s->stack,v);
                    tags[v] = l_True;
                    veci_push(&s->tagged,v);
                }else{
                    int* tagged = veci_begin(&s->tagged);
                    int j;
                    for (j = top; j < veci_size(&s->tagged); j++)
                        tags[tagged[j]] = l_Undef;
                    veci_resize(&s->tagged,top);
                    return false;
                }
            }
        }else{
            lit*    lits = clause_begin(c);
            int     i, j;

            for (i = 1; i < clause_size(c); i++){
                int v = lit_var(lits[i]);
                if (tags[v] == l_Undef && levels[v] != 0){
                    if (reasons[v] != 0 && ((1 << (levels[v] & 31)) & minl)){

                        veci_push(&s->stack,lit_var(lits[i]));
                        tags[v] = l_True;
                        veci_push(&s->tagged,v);
                    }else{
                        int* tagged = veci_begin(&s->tagged);
                        for (j = top; j < veci_size(&s->tagged); j++)
                            tags[tagged[j]] = l_Undef;
                        veci_resize(&s->tagged,top);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}


#ifdef NONBLOCKING
static void solver_analyze(solver* s, clause* c, veci* learnt, lit target_lit)
{
    lit*     trail   = s->trail;
    lbool*   tags    = s->tags;
    clause** reasons = s->reasons;
    int*     levels     = s->levels;
    int*     sublevels  = s->sublevels;
    int      cnt     = 0;
    lit      p       = lit_Undef;
    int      ind     = s->qtail-1;
    lit*     lits;
    int      i, j, minl;
    int*     tagged;

    veci_push(learnt,lit_Undef);
    lbool target_passed = (target_lit == lit_Undef? l_True: l_False);

#ifdef DLEVEL
    do{
        /*if(c == 0)   { // for debug
            printf("target lit: "L_LIT", the current lit: "L_LIT" \n", L_lit(target_lit), L_lit(p));
            solver_printtrail(s);
        }*/
        assert(c != 0);
        if (clause_is_lit(c)){
            lit q = clause_read_lit(c);
            assert(lit_var(q) >= 0 && lit_var(q) < s->size);
            if (tags[lit_var(q)] == l_Undef && levels[lit_var(q)] > 0){
                tags[lit_var(q)] = l_True;
                veci_push(&s->tagged,lit_var(q));
                act_var_bump(s,lit_var(q));
                if (levels[lit_var(q)] == solver_dlevel(s))
                    cnt++;
                else
                    veci_push(learnt,q);
            }
        } else {

            if (clause_learnt(c))
                act_clause_bump(s,c);

            lits = clause_begin(c);
            //printlits(lits,lits+clause_size(c)); printf("\n");
            for (j = (p == lit_Undef ? 0 : 1); j < clause_size(c); j++){
                lit q = lits[j];
                assert(lit_var(q) >= 0 && lit_var(q) < s->size);
                if (tags[lit_var(q)] == l_Undef && levels[lit_var(q)] > 0){
                    tags[lit_var(q)] = l_True;
                    veci_push(&s->tagged,lit_var(q));
                    act_var_bump(s,lit_var(q));
                    if (levels[lit_var(q)] == solver_dlevel(s))
                        cnt++;
                    else
                        veci_push(learnt,q);
                }
            }
        }

        do {
            while (tags[lit_var(trail[ind--])] == l_Undef);

            p = trail[ind+1];
            c = reasons[lit_var(p)];
            cnt--;
            if (p == target_lit)
                target_passed = l_True;
            if (c == 0 && cnt > 0 && p != target_lit)
                veci_push(learnt, lit_neg(p));
        } while (c == 0 && cnt > 0); // add flipped decisions and skip them.
    } while (cnt > 0 || target_passed == l_False);

    if (target_lit == lit_Undef) {
        *veci_begin(learnt) = lit_neg(p);
     } else {
        if(p != target_lit)
            veci_push(learnt, lit_neg(p));
        *veci_begin(learnt) = lit_neg(target_lit);
     }

#else /*SUBLEVEL*/
    do{
        /*if(cnt > 0 && target_passed == l_True)   { // for debug
            printf("target lit: "L_LIT", the current lit: "L_LIT" \n", L_lit(target_lit), L_lit(p));
            solver_printtrail(s);
        }*/
        assert(c != 0);

        if (clause_is_lit(c)){
            lit q = clause_read_lit(c);
            assert(lit_var(q) >= 0 && lit_var(q) < s->size);
            if (tags[lit_var(q)] == l_Undef && sublevels[lit_var(q)] > 0){
                tags[lit_var(q)] = l_True;
                veci_push(&s->tagged,lit_var(q));
                act_var_bump(s,lit_var(q));
                if (sublevels[lit_var(q)] == solver_sublevel(s))
                    cnt++;
                else
                    veci_push(learnt,q);
            }
        } else {

            if (clause_learnt(c))
                act_clause_bump(s,c);

            lits = clause_begin(c);
            //printlits(lits,lits+clause_size(c)); printf("\n");
            for (j = (p == lit_Undef ? 0 : 1); j < clause_size(c); j++){
                lit q = lits[j];
                assert(lit_var(q) >= 0 && lit_var(q) < s->size);
                if (tags[lit_var(q)] == l_Undef && sublevels[lit_var(q)] > 0){
                    tags[lit_var(q)] = l_True;
                    veci_push(&s->tagged,lit_var(q));
                    act_var_bump(s,lit_var(q));
                    if (sublevels[lit_var(q)] == solver_sublevel(s))
                        cnt++;
                    else
                        veci_push(learnt,q);
                }
            }
        }

        do {
            while (tags[lit_var(trail[ind--])] == l_Undef);

            p = trail[ind+1];
            c = reasons[lit_var(p)];
            cnt--;
            if (p == target_lit)
                target_passed = l_True;
            //if (c == 0 && cnt > 0 && p != target_lit) 
            //    veci_push(learnt, lit_neg(p)); // note: this must be commented in sublevel analysis!
        } while (c == 0 && cnt > 0);
    } while (cnt > 0 || target_passed == l_False);

    if (target_lit == lit_Undef) {
        *veci_begin(learnt) = lit_neg(p);
     } else {
        if(p != target_lit)
            veci_push(learnt, lit_neg(p));
        *veci_begin(learnt) = lit_neg(target_lit);
     }
#endif

#ifdef DLEVEL
    lits = veci_begin(learnt);
    minl = 0;
    for (i = 1; i < veci_size(learnt); i++){
        int lev = levels[lit_var(lits[i])];
        minl    |= 1 << (lev & 31);
    }
#else /*SUBLEVEL*/
    lits = veci_begin(learnt);
    minl = 0;
    for (i = 1; i < veci_size(learnt); i++){
        int lev = sublevels[lit_var(lits[i])];
        minl    |= 1 << (lev & 31);
    }
#endif

    // simplify (full)
    for (i = j = 1; i < veci_size(learnt); i++){
        if (reasons[lit_var(lits[i])] == 0 || !solver_lit_removable(s,lits[i],minl))
            lits[j++] = lits[i];
    }

    // update size of learnt + statistics
    s->stats.max_literals += veci_size(learnt);
    veci_resize(learnt,j);
    s->stats.tot_literals += j;

    // clear tags
    tagged = veci_begin(&s->tagged);
    for (i = 0; i < veci_size(&s->tagged); i++)
        tags[tagged[i]] = l_Undef;
    veci_resize(&s->tagged,0);

#ifdef DEBUG
    for (i = 0; i < s->size; i++)
        assert(tags[i] == l_Undef);
#endif

#ifdef VERBOSEDEBUG
    printf(L_IND"Learnt {", L_ind);
    for (i = 0; i < veci_size(learnt); i++) printf(" "L_LIT, L_lit(lits[i]));
#endif
    if (veci_size(learnt) > 1){
        int max_i = 1;
        int max   = sublevels[lit_var(lits[1])];
        lit tmp;

        for (i = 2; i < veci_size(learnt); i++)
            if (sublevels[lit_var(lits[i])] > max){
                max   = sublevels[lit_var(lits[i])];
                max_i = i;
            }

        tmp         = lits[1];
        lits[1]     = lits[max_i];
        lits[max_i] = tmp;
    }
#ifdef VERBOSEDEBUG
    {
        int lev = veci_size(learnt) > 1 ? s->levels[lit_var(lits[1])] : 0;
        int sublev = veci_size(learnt) > 1 ? sublevels[lit_var(lits[1])] : 0;
        printf(" } at level %d, sublevel %d\n", lev, sublev);
    }
#endif

}

#else
static void solver_analyze(solver* s, clause* c, veci* learnt)
{
    lit*     trail   = s->trail;
    lbool*   tags    = s->tags;
    clause** reasons = s->reasons;
    int*     levels  = s->levels;
    int      cnt     = 0;
    lit      p       = lit_Undef;
    int      ind     = s->qtail-1;
    lit*     lits;
    int      i, j, minl;
    int*     tagged;

    veci_push(learnt,lit_Undef);

    do{
        assert(c != 0);

        if (clause_is_lit(c)){
            lit q = clause_read_lit(c);
            assert(lit_var(q) >= 0 && lit_var(q) < s->size);
            if (tags[lit_var(q)] == l_Undef && levels[lit_var(q)] > 0){
                tags[lit_var(q)] = l_True;
                veci_push(&s->tagged,lit_var(q));
                act_var_bump(s,lit_var(q));
                if (levels[lit_var(q)] == solver_dlevel(s))
                    cnt++;
                else
                    veci_push(learnt,q);
            }
        }else{

            if (clause_learnt(c))
                act_clause_bump(s,c);

            lits = clause_begin(c);
            //printlits(lits,lits+clause_size(c)); printf("\n");
            for (j = (p == lit_Undef ? 0 : 1); j < clause_size(c); j++){
                lit q = lits[j];
                assert(lit_var(q) >= 0 && lit_var(q) < s->size);
                if (tags[lit_var(q)] == l_Undef && levels[lit_var(q)] > 0){
                    tags[lit_var(q)] = l_True;
                    veci_push(&s->tagged,lit_var(q));
                    act_var_bump(s,lit_var(q));
                    if (levels[lit_var(q)] == solver_dlevel(s))
                        cnt++;
                    else
                        veci_push(learnt,q);
                }
            }
        }

        while (tags[lit_var(trail[ind--])] == l_Undef);

        p = trail[ind+1];
        c = reasons[lit_var(p)];
        cnt--;

    }while (cnt > 0);

    *veci_begin(learnt) = lit_neg(p);

    lits = veci_begin(learnt);
    minl = 0;
    for (i = 1; i < veci_size(learnt); i++){
        int lev = levels[lit_var(lits[i])];
        minl    |= 1 << (lev & 31);
    }

    // simplify (full)
    for (i = j = 1; i < veci_size(learnt); i++){
        if (reasons[lit_var(lits[i])] == 0 || !solver_lit_removable(s,lits[i],minl))
            lits[j++] = lits[i];
    }

    // update size of learnt + statistics
    s->stats.max_literals += veci_size(learnt);
    veci_resize(learnt,j);
    s->stats.tot_literals += j;

    // clear tags
    tagged = veci_begin(&s->tagged);
    for (i = 0; i < veci_size(&s->tagged); i++)
        tags[tagged[i]] = l_Undef;
    veci_resize(&s->tagged,0);

#ifdef DEBUG
    for (i = 0; i < s->size; i++)
        assert(tags[i] == l_Undef);
#endif

#ifdef VERBOSEDEBUG
    printf(L_IND"Learnt {", L_ind);
    for (i = 0; i < veci_size(learnt); i++) printf(" "L_LIT, L_lit(lits[i]));
#endif
    if (veci_size(learnt) > 1){
        int max_i = 1;
        int max   = levels[lit_var(lits[1])];
        lit tmp;

        for (i = 2; i < veci_size(learnt); i++)
            if (levels[lit_var(lits[i])] > max){
                max   = levels[lit_var(lits[i])];
                max_i = i;
            }

        tmp         = lits[1];
        lits[1]     = lits[max_i];
        lits[max_i] = tmp;
    }
#ifdef VERBOSEDEBUG
    {
        int lev = veci_size(learnt) > 1 ? levels[lit_var(lits[1])] : 0;
        printf(" } at level %d\n", lev);
    }
#endif
}
#endif /*NONBLOCKING*/


clause* solver_propagate(solver* s)
{
    lbool*  values = s->assigns;
    clause* confl  = (clause*)0;
    lit*    lits;

    //printf("solver_propagate\n");
    while (confl == 0 && s->qtail - s->qhead > 0){
        lit  p  = s->trail[s->qhead++];
        vecp* ws = solver_read_wlist(s,p);
        clause **begin = (clause**)vecp_begin(ws);
        clause **end   = begin + vecp_size(ws);
        clause **i, **j;

        s->stats.propagations++;
        s->simpdb_props--;

        //printf("checking lit %d: "L_LIT"\n", veci_size(ws), L_lit(p));
        for (i = j = begin; i < end; ){
            if (clause_is_lit(*i)){
                *j++ = *i;
                if (!enqueue(s,clause_read_lit(*i),clause_from_lit(p))){
                    confl = s->binary;
                    (clause_begin(confl))[1] = lit_neg(p);
                    (clause_begin(confl))[0] = clause_read_lit(*i++);

                    // Copy the remaining watches:
                    while (i < end)
                        *j++ = *i++;
                }
            }else{
                lit false_lit;
                lbool sig;

                lits = clause_begin(*i);

                // Make sure the false literal is data[1]:
                false_lit = lit_neg(p);
                if (lits[0] == false_lit){
                    lits[0] = lits[1];
                    lits[1] = false_lit;
                }
                assert(lits[1] == false_lit);
                //printf("checking clause: "); printlits(lits, lits+clause_size(*i)); printf("\n");

                // If 0th watch is true, then clause is already satisfied.
                sig = !lit_sign(lits[0]); sig += sig - 1;
                if (values[lit_var(lits[0])] == sig){
                    *j++ = *i;
                }else{
                    // Look for new watch:
                    lit* stop = lits + clause_size(*i);
                    lit* k;
                    for (k = lits + 2; k < stop; k++){
                        lbool sig = lit_sign(*k); sig += sig - 1;
                        if (values[lit_var(*k)] != sig){
                            lits[1] = *k;
                            *k = false_lit;
                            vecp_push(solver_read_wlist(s,lit_neg(lits[1])),*i);
                            goto next; }
                    }

                    *j++ = *i;
                    // Clause is unit under assignment:
                    if (!enqueue(s,lits[0], *i)){
                        confl = *i++;
                        // Copy the remaining watches:
                        while (i < end)
                            *j++ = *i++;
                    }
                }
            }
        next:
            i++;
        }

        s->stats.inspects += j - (clause**)vecp_begin(ws);
        vecp_resize(ws,j - (clause**)vecp_begin(ws));
    }

    return confl;
}

static inline int clause_cmp (const void* x, const void* y) {
    return clause_size((clause*)x) > 2 && (clause_size((clause*)y) == 2 || clause_activity((clause*)x) < clause_activity((clause*)y)) ? -1 : 1; }

void solver_reducedb(solver* s)
{
    int      i, j;
    double   extra_lim = s->cla_inc / vecp_size(&s->learnts); // Remove any clause below this activity
    clause** learnts = (clause**)vecp_begin(&s->learnts);
    clause** reasons = s->reasons;

    sort(vecp_begin(&s->learnts), vecp_size(&s->learnts), &clause_cmp);

    for (i = j = 0; i < vecp_size(&s->learnts) / 2; i++){
        if (clause_size(learnts[i]) > 2 && reasons[lit_var(*clause_begin(learnts[i]))] != learnts[i])
            clause_remove(s,learnts[i]);
        else
            learnts[j++] = learnts[i];
    }
    for (; i < vecp_size(&s->learnts); i++){
        if (clause_size(learnts[i]) > 2 && reasons[lit_var(*clause_begin(learnts[i]))] != learnts[i] && clause_activity(learnts[i]) < extra_lim)
            clause_remove(s,learnts[i]);
        else
            learnts[j++] = learnts[i];
    }

    //printf("reducedb deleted %d\n", vecp_size(&s->learnts) - j);


    vecp_resize(&s->learnts,j);
}


static void solver_extendobdd(solver* s, obdd_t* target)
{
    lbool*  values   = s->assigns;
    const int targetvar = (target == obdd_top()) ? s->size: obdd_label(target)-1;
    obdd_t* p;

    // Go down to a leaf of OBDD according to the current assignment.
    vecp_resize(&s->obddpath, 0);
    p = s->root;
    int i;
    while (p != NULL && (i=obdd_label(p)-1) < targetvar) {
        vecp_push(&s->obddpath, (void*)p);
        p = (values[i] == l_False? p->lo: p->hi);
    }
    assert(vecp_size(&s->obddpath) > 0);
#ifdef NONBLOCKING
    assert(p == NULL); // solutions never be rediscovered due to chronologcal backtracking.
#endif

    if (p == NULL) {
        // Concatenate new nodes to OBDD.
        p = vecp_begin(&s->obddpath)[vecp_size(&s->obddpath)-1];
        vecp_resize(&s->obddpath, vecp_size(&s->obddpath)-1);
        for (i=obdd_label(p)-1; i < targetvar; i++) {
            vecp_push(&s->obddpath, (void*)p);
            obdd_t* next = (i == targetvar-1? target: obdd_node(i+2, NULL, NULL));
            if (values[i] == l_False)  
                p->lo = next;
            else                      
                p->hi = next;
            p = next;
        }

        // Update the aux field of each obdd node traversed above (this is for counting the number of solutions).
        for (i = vecp_size(&s->obddpath)-1; i >= 0; i--) { 
            p = (obdd_t*)vecp_begin(&s->obddpath)[i];
            intptr_t nl = (p->lo != NULL? p->lo->aux: 0);
            intptr_t nh = (p->hi != NULL? p->hi->aux: 0);
            if (nl == INTPTR_MAX || nh == INTPTR_MAX || INTPTR_MAX - nl <= nh)  
                p->aux = INTPTR_MAX; // overflow!
            else                                                               
                p->aux = nl+nh;
        }
    }

    vecp_push(&s->obddpath, target);
}


#ifdef NONBLOCKING
// chronological backtrack from a given level
static lit solver_backtrack(solver*s, int level)
{
    lit t = solver_assumedlit(s, level);
    solver_insertcacheuntil(s, level-1);
    solver_canceluntil(s,level-1);

    if (level-1 > s->root_level)
        veci_push(&s->subtrail_lim,s->qtail);
    assert(s->assigns[lit_var(t)] == l_Undef);
    enqueue(s,lit_neg(t),(clause*)0);

}

// conflict resolution based on chronological backtracking
static lbool solver_resolve_conflict_bt(solver *s, clause *confl)
{
    assert(confl != (clause*)0);
    s->stats.conflicts++;
    if (solver_dlevel(s) <= s->root_level) {
        return l_True;
    }

    veci learnt_clause;
    veci_new(&learnt_clause);
    solver_analyze(s, confl, &learnt_clause, lit_Undef);

    solver_backtrack(s, solver_dlevel(s)); 
    s->lim = solver_dlevel(s);

    solver_record(s,&learnt_clause);
    act_var_decay(s);
    act_clause_decay(s);

    veci_delete(&learnt_clause);
    return l_False;
}


// conflict resolution based on non-chronological backtracking with level limit
static lbool solver_resolve_conflict_bj(solver *s, clause *confl)
{
    assert(confl != (clause*)0);
    s->stats.conflicts++;
    if (solver_dlevel(s) <= s->root_level) {
        return l_True;
    }

    veci learnt_clause;
    veci_new(&learnt_clause);
    solver_analyze(s, confl, &learnt_clause, lit_Undef);

    if (s->lim < solver_dlevel(s)) {
        int blevel = veci_size(&learnt_clause) > 1 ? s->levels[lit_var(veci_begin(&learnt_clause)[1])] : s->root_level;
        blevel = blevel < s->lim ? s->lim: blevel;
        solver_insertcacheuntil(s, blevel);
        solver_canceluntil(s,blevel);
    } else {
        solver_backtrack(s, solver_dlevel(s)); 
        s->lim = solver_dlevel(s);
    }

    solver_record(s,&learnt_clause);
    act_var_decay(s);
    act_clause_decay(s);

    veci_delete(&learnt_clause);
    return l_False;
}


// conflict resolution based on conflict-directed backjumping
static lbool solver_resolve_conflict_cbj(solver *s, clause *confl)
{
    assert(confl != (clause*)0);
    assert(vecp_size(&s->generated_clauses) == 0);

    clause *c;
    veci learnt_clause;
    veci_new(&learnt_clause);

    while(1) {
        if (confl != 0) {
            s->stats.conflicts++;
            if (solver_dlevel(s) <= s->root_level) {
                veci_delete(&learnt_clause);
                return l_True;
            }

            veci_resize(&learnt_clause,0);
            solver_analyze(s, confl, &learnt_clause, lit_Undef);

            veci *cl = (veci*)malloc(sizeof(veci));
            veci_new(cl);
            for (int i = 0; i < veci_size(&learnt_clause); i++)
                veci_push(cl, veci_begin(&learnt_clause)[i]);
            vecp_push(&s->generated_clauses, (veci*)cl);

            lit p = solver_backtrack(s,solver_dlevel(s));
            s->lim = solver_dlevel(s) < s->lim ? solver_dlevel(s): s->lim;
        } else if (vecp_size(&s->generated_clauses) > 0) {
            veci *cl1 = (veci*)vecp_begin(&s->generated_clauses)[vecp_size(&s->generated_clauses)-1];
            vecp_resize(&s->generated_clauses, vecp_size(&s->generated_clauses)-1);

            lbool asserting = clause_isasserting(s,cl1);// unit literal is placed at the begining.
            c = solver_record_noenqueue(s,cl1);
            act_var_decay(s);
            act_clause_decay(s);

            if (asserting == l_True) {
                const lit unit = *veci_begin(cl1);
                enqueue(s,unit,(clause*)c);

                if ((confl = solver_propagate(s)) != 0) {
                    s->stats.conflicts++;

                    if (solver_dlevel(s) <= s->root_level){
                        veci_delete(cl1); free(cl1);
                        veci_delete(&learnt_clause);
                        return l_True;
                    }

                    veci_resize(&learnt_clause,0);
                    solver_analyze(s, confl, &learnt_clause, unit);
                    //solver_printtrail(s);
                    //printf("learnt:");printlits(veci_begin(&learnt_clause), veci_begin(&learnt_clause)+veci_size(&learnt_clause));printf("\n");
                    assert(veci_begin(&learnt_clause)[0] == lit_neg(unit));

                    veci *cl3 = (veci*)malloc(sizeof(veci));
                    veci_new(cl3);
                    //printf("cl1:");printlits(veci_begin(cl1), veci_begin(cl1)+veci_size(cl1));printf("\n");
                    //printf("cl2:");printlits(veci_begin(&learnt_clause), veci_begin(&learnt_clause)+veci_size(&learnt_clause));printf("\n");
                    perform_resolution(s, cl1, &learnt_clause, cl3);
                    if (veci_size(cl3) == 0) { // if the whole space was exhausted,
                        veci_delete(cl3); free(cl3);
                        veci_delete(cl1); free(cl1);
                        veci_delete(&learnt_clause);
                        return l_True;
                    }
                    //printf("cl3:");printlits(veci_begin(cl3), veci_begin(cl3)+veci_size(cl3));printf("\n\n");

                    vecp_push(&s->generated_clauses, (veci*)cl3);

                    int highest = s->levels[lit_var(*veci_begin(cl3))];
                    lit p = solver_backtrack(s, highest);
                    s->lim = solver_dlevel(s) < s->lim ? solver_dlevel(s): s->lim;
                }
            }
            veci_delete(cl1); free(cl1);
        } else {
            break;
        }
        confl = solver_propagate(s);
    }

    veci_delete(&learnt_clause);
    return l_False;
}


// conflict resolution based on combination of BJ and CBJ
static lbool solver_resolve_conflict_bjcbj(solver *s, clause *confl)
{
    if (s->lim < solver_dlevel(s)) {
        return solver_resolve_conflict_bj(s, confl);
    } else {
        return solver_resolve_conflict_cbj(s, confl);
    }
}


static lbool solver_resolve_conflict(solver *s, clause *confl)
{
#if defined(BT)
#ifdef VERBOSEDEBUG
                printf(L_IND"**BT**\n", L_ind);
#endif
    return solver_resolve_conflict_bt(s, confl);
#elif defined(BJ)
#ifdef VERBOSEDEBUG
                printf(L_IND"**BJ**\n", L_ind);
#endif
    return solver_resolve_conflict_bj(s, confl);
#elif defined(CBJ)
#ifdef VERBOSEDEBUG
                printf(L_IND"**CBJ**\n", L_ind);
#endif
    return solver_resolve_conflict_cbj(s, confl);
#else //BJ+CBJ
#ifdef VERBOSEDEBUG
                printf(L_IND"**BJ+CBJ**\n", L_ind);
#endif
    return solver_resolve_conflict_bjcbj(s, confl);
#endif
}


static lbool solver_search(solver* s, int nof_conflicts, int nof_learnts)
{
    int*    levels          = s->levels;
    int*    sublevels       = s->sublevels;
    double  var_decay       = 0.95;
    double  clause_decay    = 0.999;
    /*double  random_var_freq = 0.02;*/

    /*int     conflictC       = 0;*/

    assert(s->root_level == solver_dlevel(s));
    assert(s->root_level == solver_sublevel(s));
    assert(s->root_level == s->lim);

    lbool*    values  = s->assigns;
    const int nvars   = s->size; 


    s->stats.starts++;
    s->var_decay = (float)(1 / var_decay   );
    s->cla_decay = (float)(1 / clause_decay);

    for (;;){
		if (eflag == 1) return l_False;
        clause* confl = solver_propagate(s);
        if (confl != 0) {
            // CONFLICT
            lbool res = solver_resolve_conflict(s, confl);
            if(res == l_True) 
                return l_True;
        } else {

            // NO CONFLICT
            int next;

            /*if (nof_conflicts >= 0 && conflictC >= nof_conflicts){ // restart is disabled.
                // Reached bound on number of conflicts:
                s->progress_estimate = solver_progress(s);
                solver_canceluntil(s,s->root_level);
                veci_delete(&learnt_clause);
                return l_Undef; }*/

            if (solver_dlevel(s) == 0)
                // Simplify the set of problem clauses:
                solver_simplify(s);

            if (nof_learnts >= 0 && vecp_size(&s->learnts) - s->qtail >= nof_learnts)
                // Reduce the set of learnt clauses:
                solver_reducedb(s);

            // New variable decision:
            s->stats.decisions++;
            /*next = order_select(s,(float)random_var_freq);*/

            bool  modelfound = false;

#ifdef LAZY
            for (next = s->nextvar; next < nvars && values[next] != l_Undef; next++) ;

            if (next == nvars) { // model found without cache
                modelfound = true;
                solver_extendobdd(s, obdd_top());
            } else if (s->nextvar < next) {
                unsigned int *vec = vecp_begin(&s->bitvecs)[next-1];
                solver_makecache(s, vec, next-1);
                obdd_t* lookup;
                s->stats.ncachelookup++;
                if ((lookup = (obdd_t*)trie_search(vec, s->cache[next-1])) != NULL) {
                    modelfound = true;
                    s->stats.ncachehits++;
                    solver_extendobdd(s, lookup);
                }
            }
            s->nextvar = next;
#else
            for (next = s->nextvar; next < nvars-1 && values[next] != l_Undef; next++) {
                unsigned int *vec = vecp_begin(&s->bitvecs)[next];
                solver_makecache(s, vec, next);

                obdd_t* lookup;
                s->stats.ncachelookup++;
                if ((lookup = (obdd_t*)trie_search(vec, s->cache[next])) != NULL) {
                    modelfound = true;
                    s->stats.ncachehits++;
                    solver_extendobdd(s, lookup);
                    break;
                }
            }

            if (!modelfound && next == nvars-1 && values[next] != l_Undef)  { // model found without cache.
                modelfound = true;
                solver_extendobdd(s, obdd_top());
            }
            s->nextvar = next;
#endif

            if (modelfound) {
#ifdef VERBOSEDEBUG
                printf(L_IND"**MODEL**\n", L_ind);
#endif

                if (solver_dlevel(s) <= s->root_level){ // model found without any assumption
                    return l_True;
                }

#ifdef REFRESH
                if (obdd_nnodes() + s->size > s->stats.maxnodes) 
                    solver_refreshobdd(s);
#endif

                solver_backtrack(s, solver_dlevel(s));
                s->lim = solver_dlevel(s);
            } else {
                assume(s,lit_neg(toLit(s->nextvar)));
            }
        }
    }

    return l_Undef; // cannot happen
}

#else
static lbool solver_search(solver* s, int nof_conflicts, int nof_learnts)
{
    int*    levels          = s->levels;
    double  var_decay       = 0.95;
    double  clause_decay    = 0.999;
    /*double  random_var_freq = 0.02;*/ // deleted

    /*int     conflictC       = 0;*/ // deleted
    veci    learnt_clause;

    assert(s->root_level == solver_dlevel(s));

    lbool*    values  = s->assigns;
    const int nvars   = s->size; 


    s->stats.starts++;
    s->var_decay = (float)(1 / var_decay   );
    s->cla_decay = (float)(1 / clause_decay);
    //veci_resize(&s->model,0);
    veci_new(&learnt_clause);

    for (;;){
		if (eflag == 1) return l_False;
        clause* confl = solver_propagate(s);
        if (confl != 0){
            // CONFLICT
            int blevel;
#ifdef VERBOSEDEBUG
            printf(L_IND"**CONFLICT**\n", L_ind);
#endif
            s->stats.conflicts++; /*conflictC++;*/ 
            if (solver_dlevel(s) <= s->root_level){
                veci_delete(&learnt_clause);
                return l_True;
            }

            veci_resize(&learnt_clause,0);
            solver_analyze(s, confl, &learnt_clause);
            blevel = veci_size(&learnt_clause) > 1 ? levels[lit_var(veci_begin(&learnt_clause)[1])] : s->root_level;
            blevel = s->root_level > blevel ? s->root_level : blevel;
            solver_canceluntil(s,blevel);
            solver_record(s,&learnt_clause);
            act_var_decay(s);
            act_clause_decay(s);
        }else{
            // NO CONFLICT
            int next;

            /*if (nof_conflicts >= 0 && conflictC >= nof_conflicts){ // restart is disabled.
                // Reached bound on number of conflicts:
                s->progress_estimate = solver_progress(s);
                solver_canceluntil(s,s->root_level);
                veci_delete(&learnt_clause);
                return l_Undef; }*/

            if (solver_dlevel(s) == 0)
                // Simplify the set of problem clauses:
                solver_simplify(s);

            if (nof_learnts >= 0 && vecp_size(&s->learnts) - s->qtail >= nof_learnts)
                // Reduce the set of learnt clauses:
                solver_reducedb(s);

            // New variable decision:
            s->stats.decisions++;
            /*next = order_select(s,(float)random_var_freq);*/ // deleted

            bool  modelfound = false;

#ifdef LAZY
            for (next = s->nextvar; next < nvars && values[next] != l_Undef; next++) ;

            if (next == nvars) { // model found without cache
                modelfound = true;
                solver_extendobdd(s, obdd_top());
            } else if (s->nextvar < next) {
                unsigned int *vec = vecp_begin(&s->bitvecs)[next-1];
                solver_makecache(s, vec, next-1);
                obdd_t* lookup;
                s->stats.ncachelookup++;
                if ((lookup = (obdd_t*)trie_search(vec, s->cache[next-1])) != NULL) {
                    modelfound = true;
                    s->stats.ncachehits++;
                    solver_extendobdd(s, lookup);
                }
            }
            s->nextvar = next;
#else
            for (next = s->nextvar; next < nvars-1 && values[next] != l_Undef; next++) {
                unsigned int *vec = vecp_begin(&s->bitvecs)[next];
                solver_makecache(s, vec, next);

                obdd_t* lookup;
                s->stats.ncachelookup++;
                if ((lookup = (obdd_t*)trie_search(vec, s->cache[next])) != NULL) {
                    modelfound = true;
                    s->stats.ncachehits++;
                    solver_extendobdd(s, lookup);
                    break;
                }
            }

            if (!modelfound && next == nvars-1 && values[next] != l_Undef)  { // model found without cache.
                modelfound = true;
                solver_extendobdd(s, obdd_top());
            }
            s->nextvar = next;
#endif

            if (modelfound) {
#ifdef VERBOSEDEBUG
                printf(L_IND"**MODEL**\n", L_ind);
#endif
                if (solver_dlevel(s) <= s->root_level){ // model found without any assumption
                    veci_delete(&learnt_clause);
                    return l_True;
                }

                veci_resize(&learnt_clause,0);
                for (int i = solver_dlevel(s); i > s->root_level; i--) 
                    veci_push(&learnt_clause, lit_neg(solver_assumedlit(s, i))); // set a blocking clause.
                int blevel = solver_dlevel(s)-1;
#ifdef VERBOSEDEBUG
                printf(L_IND"Learnt {", L_ind);
                for (int i = 0; i < veci_size(&learnt_clause); i++) 
                    printf(" "L_LIT, L_lit(veci_begin(&learnt_clause)[i]));
                printf(" } at level %d\n", blevel);
#endif

                solver_insertcacheuntil(s, blevel);
                solver_canceluntil(s,blevel); 
                solver_record(s,&learnt_clause); 
                act_var_decay(s);
                act_clause_decay(s);
            } else {
                assume(s,lit_neg(toLit(s->nextvar)));
            }
        }
    }

    return l_Undef; // cannot happen
}

#endif /*NONBLOCKING*/

//=================================================================================================
// External solver functions:

solver* solver_new(void)
{
    solver* s = (solver*)malloc(sizeof(solver));

    // initialize vectors
    vecp_new(&s->clauses);
    vecp_new(&s->learnts);
    vecp_new(&s->bitvecs);
    veci_new(&s->order);
    veci_new(&s->trail_lim);
#ifdef NONBLOCKING
    veci_new(&s->subtrail_lim);
    vecp_new(&s->generated_clauses);
#endif /*NONBLOCKING*/
    veci_new(&s->cachedvars);
    veci_new(&s->tagged);
    veci_new(&s->stack);

    vecp_new(&s->obddpath);

#ifdef NONBLOCKING
    s->out       = NULL;
    s->lim        = 0;
    s->stats.maxnodes  = INT_MAX;
#endif /*NONBLOCKING*/
    s->stats.refreshes = 0;
    s->stats.obddsize  = 0;

    // initialize arrays
    s->wlists    = 0;
    s->activity  = 0;
    s->assigns   = 0;
    s->orderpos  = 0;
    s->reasons   = 0;
    s->levels    = 0;
#ifdef NONBLOCKING
    s->sublevels = 0;
#endif /*NONBLOCKING*/
    s->tags      = 0;
    s->trail     = 0;

    s->stats.clk       = (clock_t)0;

    // fields for obdd construction
    s->nextvar     = 0;
#ifdef CUTSETCACHE
    s->maxcutwidth = 0;
    s->cutwidth    = NULL;
    s->cutsets     = NULL;
#else /*SEPARATORCACHE*/
    s->maxpathwidth= 0;
    s->pathwidth   = NULL;
    s->separators  = NULL;
#endif
    s->cache       = NULL;
    s->root        = NULL;
    s->trail       = NULL;

    s->root         = obdd_node(1, NULL, NULL); 
    obdd_top()->aux = (intptr_t)1;
    obdd_bot()->aux = (intptr_t)0;

    // initialize other vars
    s->size                   = 0;
    s->cap                    = 0;
    s->qhead                  = 0;
    s->qtail                  = 0;
    s->cla_inc                = 1;
    s->cla_decay              = 1;
    s->var_inc                = 1;
    s->var_decay              = 1;
    s->root_level             = 0;
    s->simpdb_assigns         = 0;
    s->simpdb_props           = 0;
    s->random_seed            = 91648253;
    s->progress_estimate      = 0;
    s->binary                 = (clause*)malloc(sizeof(clause) + sizeof(lit)*2);
    s->binary->size_learnt    = (2 << 1);
    s->verbosity              = 0;

    s->stats.starts           = 0;
    s->stats.decisions        = 0;
    s->stats.propagations     = 0;
    s->stats.inspects         = 0;
    s->stats.conflicts        = 0;
    s->stats.clauses          = 0;
    s->stats.clauses_literals = 0;
    s->stats.learnts          = 0;
    s->stats.learnts_literals = 0;
    s->stats.max_literals     = 0;
    s->stats.tot_literals     = 0;

    s->stats.ncachehits       = 0;
    s->stats.ncachelookup     = 0;

    s->stats.tot_solutions  = 0; 
#ifdef GMP
    mpz_init(s->stats.tot_solutions_gmp);
    mpz_set_ui(s->stats.tot_solutions_gmp, 0); 
#endif

    return s;
}


void solver_delete(solver* s)
{
    int i;
    for (i = 0; i < vecp_size(&s->clauses); i++)
        free(vecp_begin(&s->clauses)[i]);

    for (i = 0; i < vecp_size(&s->learnts); i++)
        free(vecp_begin(&s->learnts)[i]);

#ifdef NONBLOCKING
    for (i = 0; i < vecp_size(&s->generated_clauses); i++) {
        veci_delete(vecp_begin(&s->generated_clauses)[i]);
        free(vecp_begin(&s->generated_clauses)[i]);
    }
#endif /*NONBLOCKING*/

    for (i = 0; i < vecp_size(&s->bitvecs); i++)
        free(vecp_begin(&s->bitvecs)[i]);

    // delete vectors
    vecp_delete(&s->clauses);
    vecp_delete(&s->learnts);
    vecp_delete(&s->bitvecs);
    vecp_delete(&s->obddpath);
    veci_delete(&s->order);
    veci_delete(&s->trail_lim);
#ifdef NONBLOCKING
    veci_delete(&s->subtrail_lim);
    vecp_delete(&s->generated_clauses);
#endif /*NONBLOCKING*/
    veci_delete(&s->cachedvars);
    veci_delete(&s->tagged);
    veci_delete(&s->stack);
    free(s->binary);

#ifdef GMP
    mpz_clear(s->stats.tot_solutions_gmp);
#endif

    // delete arrays
    if (s->wlists != 0){
        int i;
        for (i = 0; i < s->size*2; i++)
            vecp_delete(&s->wlists[i]);

        // if one is different from null, all are
        free(s->wlists);
        free(s->activity );
        free(s->assigns  );
        free(s->orderpos );
        free(s->reasons  );
        free(s->levels   );
#ifdef NONBLOCKING
        free(s->sublevels);
#endif /*NONBLOCKING*/
        free(s->trail    );
        free(s->tags     );
    }

#ifdef CUTSETCACHE
    for (int i = 0; i < s->size; i++) {
        free(s->cutsets[i]);
    }
    free(s->cutsets);
    free(s->cutwidth);
#else /*SEPARATORCACHE*/
    for (int i = 0; i < s->size; i++) {
        free(s->separators[i]);
    }
    free(s->separators);
    free(s->pathwidth);
#endif

    for (int i = 0; i < s->size; i++) 
        trie_delete(s->cache[i]);
    trie_finalize();

    free(s);
}


bool solver_addclause(solver* s, lit* begin, lit* end)
{
    lit *i,*j;
    int maxvar;
    lbool* values;
    lit last;

    if (begin == end) return false;

    //printlits(begin,end); printf("\n");
    // insertion sort
    maxvar = lit_var(*begin);
    for (i = begin + 1; i < end; i++){
        lit l = *i;
        maxvar = lit_var(l) > maxvar ? lit_var(l) : maxvar;
        for (j = i; j > begin && *(j-1) > l; j--)
            *j = *(j-1);
        *j = l;
    }
    solver_setnvars(s,maxvar+1);

    //printlits(begin,end); printf("\n");
    values = s->assigns;

    // delete duplicates
    last = lit_Undef;
    for (i = j = begin; i < end; i++){
        //printf("lit: "L_LIT", value = %d\n", L_lit(*i), (lit_sign(*i) ? -values[lit_var(*i)] : values[lit_var(*i)]));
        lbool sig = !lit_sign(*i); sig += sig - 1;
        if (*i == lit_neg(last) || sig == values[lit_var(*i)])
            return true;   // tautology
        else if (*i != last && values[lit_var(*i)] == l_Undef)
            last = *j++ = *i;
    }

    //printf("final: "); printlits(begin,j); printf("\n");

    if (j == begin)          // empty clause
        return false;
    else if (j - begin == 1) // unit clause
        return enqueue(s,*begin,(clause*)0);

    // create new clause
    vecp_push(&s->clauses,clause_new(s,begin,j,0));


    s->stats.clauses++;
    s->stats.clauses_literals += j - begin;

    return true;
}


bool   solver_simplify(solver* s)
{
    clause** reasons;
    int type;

    assert(solver_dlevel(s) == 0);

    if (solver_propagate(s) != 0)
        return false;

    if (s->qhead == s->simpdb_assigns || s->simpdb_props > 0)
        return true;

    reasons = s->reasons;
    for (type = 0; type < 2; type++){
        vecp*    cs  = type ? &s->learnts : &s->clauses;
        clause** cls = (clause**)vecp_begin(cs);

        int i, j;
        for (j = i = 0; i < vecp_size(cs); i++){
            if (reasons[lit_var(*clause_begin(cls[i]))] != cls[i] &&
                clause_simplify(s,cls[i]) == l_True) {
#ifdef CUTSETCACHE // original clauses must not be removed from memory because cutset caching is based on clause evaluation.
                if(cs == &s->clauses) clause_remove_nofree(s,cls[i]);
                else                  clause_remove(s,cls[i]);
#else /*SEPARATORCACHE*/
                clause_remove(s,cls[i]);
#endif
            } else
                cls[j++] = cls[i];
        }
        vecp_resize(cs,j);
    }

    s->simpdb_assigns = s->qhead;
    // (shouldn't depend on 'stats' really, but it will do for now)
    s->simpdb_props   = (int)(s->stats.clauses_literals + s->stats.learnts_literals);

    return true;
}


bool   solver_solve(solver* s, lit* begin, lit* end)
{
    double  nof_conflicts = 100;
    double  nof_learnts   = solver_nclauses(s) / 3;
    lbool   status        = l_Undef;
    lbool*  values        = s->assigns;
    lit*    i;

    solver_initcache(s);

    //printf("solve: "); printlits(begin, end); printf("\n");
    /*for (i = begin; i < end; i++){
        switch (lit_sign(*i) ? -values[lit_var(*i)] : values[lit_var(*i)]){
        case 1: // l_True:
            break;
        case 0: // l_Undef
            assume(s, *i);
            if (solver_propagate(s) == NULL)
                break;
            // falltrough
        case -1: // l_False
            solver_canceluntil(s, 0);
            return false;
        }
    }*/

    s->root_level = solver_dlevel(s);
#ifdef NONBLOCKING
    s->lim         = solver_dlevel(s);
    assert(solver_dlevel(s) == solver_sublevel(s));
#endif /*NONBLOCKING*/

    /*if (s->verbosity >= 1){
        printf("==================================[MINISAT]===================================\n");
        printf("| Conflicts |     ORIGINAL     |              LEARNT              | Progress |\n");
        printf("|           | Clauses Literals |   Limit Clauses Literals  Lit/Cl |          |\n");
        printf("==============================================================================\n");
    }*/

    if (s->verbosity >= 1){
        printf("==============================[MINISAT_ALL]==========================================\n");
        printf("| Time | Conflicts | Propagations | TOTAL       |            |   LEARNT  | OBDD     |\n");
        printf("|      |           |              | Solutions   | Clauses    |   Clauses | Nodes    |\n");
        printf("=====================================================================================\n");
    }    

    while (status == l_Undef){
       /* double Ratio = (s->stats.learnts == 0)? 0.0 :
            s->stats.learnts_literals / (double)s->stats.learnts;

        if (s->verbosity >= 1){
            printf("| %9.0f | %7.0f %8.0f | %7.0f %7.0f %8.0f %7.1f | %6.3f %% |\n", 
                (double)s->stats.conflicts,
                (double)s->stats.clauses, 
                (double)s->stats.clauses_literals,
                (double)nof_learnts, 
                (double)s->stats.learnts, 
                (double)s->stats.learnts_literals,
                Ratio,
                s->progress_estimate*100);
            fflush(stdout);
        }*/
        status = solver_search(s,(int)nof_conflicts, (int)nof_learnts);
        //nof_conflicts *= 1.5;
        //nof_learnts   *= 1.1;
    }

    if (s->verbosity >= 1) {
        printf("==============================================================================\n");
    }

    totalup_stats(s);
    solver_canceluntil(s,0);
    return status != l_False;
}


int solver_nvars(solver* s)
{
    return s->size;
}


int solver_nclauses(solver* s)
{
    return vecp_size(&s->clauses);
}


int solver_nconflicts(solver* s)
{
    return (int)s->stats.conflicts;
}

//=================================================================================================
// Sorting functions (sigh):

static inline void selectionsort(void** array, int size, int(*comp)(const void *, const void *))
{
    int     i, j, best_i;
    void*   tmp;

    for (i = 0; i < size-1; i++){
        best_i = i;
        for (j = i+1; j < size; j++){
            if (comp(array[j], array[best_i]) < 0)
                best_i = j;
        }
        tmp = array[i]; array[i] = array[best_i]; array[best_i] = tmp;
    }
}


static void sortrnd(void** array, int size, int(*comp)(const void *, const void *), double* seed)
{
    if (size <= 15)
        selectionsort(array, size, comp);

    else{
        void*       pivot = array[irand(seed, size)];
        void*       tmp;
        int         i = -1;
        int         j = size;

        for(;;){
            do i++; while(comp(array[i], pivot)<0);
            do j--; while(comp(pivot, array[j])<0);

            if (i >= j) break;

            tmp = array[i]; array[i] = array[j]; array[j] = tmp;
        }

        sortrnd(array    , i     , comp, seed);
        sortrnd(&array[i], size-i, comp, seed);
    }
}

void sort(void** array, int size, int(*comp)(const void *, const void *))
{
    double seed = 91648253;
    sortrnd(array,size,comp,&seed);
}

