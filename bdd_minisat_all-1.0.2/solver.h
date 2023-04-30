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

#ifndef solver_h
#define solver_h

#ifdef _WIN32
#define inline __inline // compatible with MS VS 6.0
#endif

#include <time.h>
#include <signal.h>
#include "vec.h"
#include "obdd.h"
#include "trie.h"

//=================================================================================================
// Simple types:

// does not work for c++
typedef int  bool;
static const bool  true      = 1;
static const bool  false     = 0;

typedef int                lit;
typedef char               lbool;

#ifdef _WIN32
typedef signed __int64     uint64;   // compatible with MS VS 6.0
#else
typedef unsigned long long uint64;
#endif

static const int   var_Undef = -1;
static const lit   lit_Undef = -2;

static const lbool l_Undef   =  0;
static const lbool l_True    =  1;
static const lbool l_False   = -1;

static inline lit  toLit   (int v) { return v + v; }
static inline lit  lit_neg (lit l) { return l ^ 1; }
static inline int  lit_var (lit l) { return l >> 1; }
static inline int  lit_sign(lit l) { return (l & 1); }


//=================================================================================================
// Public interface:

extern volatile sig_atomic_t eflag;

struct solver_t;
typedef struct solver_t solver;

extern solver* solver_new(void);
extern void    solver_delete(solver* s);

extern bool    solver_addclause(solver* s, lit* begin, lit* end);
extern bool    solver_simplify(solver* s);
extern bool    solver_solve(solver* s, lit* begin, lit* end);

extern int     solver_nvars(solver* s);
extern int     solver_nclauses(solver* s);
extern int     solver_nconflicts(solver* s);

extern void    solver_setnvars(solver* s,int n);

extern void totalup_stats(solver *s);

struct stats_t
{
    uint64   starts, decisions, propagations, inspects, conflicts;
    uint64   ncachelookup, ncachehits;
    uint64   clauses, clauses_literals, learnts, learnts_literals, max_literals, tot_literals;
    uint64   tot_solutions;
#ifdef GMP
    mpz_t    tot_solutions_gmp;
#endif
    clock_t  clk;
#ifdef NONBLOCKING
    uint64   maxnodes;      // maximum number of obdd nodes.
#endif /*NONBLOCKING*/
    uint64   refreshes;     // the number of refresh operations performed.
    uint64   obddsize;      // total sum of obdd sizes
};
typedef struct stats_t stats;

//=================================================================================================
// Solver representation:

struct clause_t;
typedef struct clause_t clause;

struct solver_t
{
    int      size;          // nof variables
    int      cap;           // size of varmaps
    int      qhead;         // Head index of queue.
    int      qtail;         // Tail index of queue.

    // clauses
    vecp     clauses;       // List of problem constraints. (contains: clause*)
    vecp     learnts;       // List of learnt clauses. (contains: clause*)

    // activities
    double   var_inc;       // Amount to bump next variable with.
    double   var_decay;     // INVERSE decay factor for variable activity: stores 1/decay. 
    float    cla_inc;       // Amount to bump next clause with.
    float    cla_decay;     // INVERSE decay factor for clause activity: stores 1/decay.

    vecp*    wlists;        // 
    double*  activity;      // A heuristic measurement of the activity of a variable.
    lbool*   assigns;       // Current values of variables.
    int*     orderpos;      // Index in variable order.
    clause** reasons;       //
    int*     levels;        //
    lit*     trail;

    // fields for obdd construction (added)
    int         nextvar;    // variable to be considered next  
    obdd_t*     root;       // root node of an obdd to be constructed
#ifdef CUTSETCACHE
    int*        cutwidth;   // Cusetset sizes
    int         maxcutwidth;// maximum size of a cutset 
    clause***   cutsets;    //
#else /*SEPARATORCACHE*/
    int*        pathwidth;  // Separator sizes
    int         maxpathwidth;// maximum size of a separator
    int**       separators; //
#endif
    trie_t**      cache;      // Binary Trie whose leaves are associated with obdd nodes.
    vecp        bitvecs;    // vectors for cache lookup
    veci        cachedvars; // variables at which caches were inserted
    vecp        obddpath;   // traversed obdd nodes

#ifdef NONBLOCKING
    FILE*    out;           //
    int*     sublevels;     // decision sublevels
    veci     subtrail_lim;  // Separator indices for different decision sublevels in 'trail'. (contains: int)
    vecp     generated_clauses;  // generated clauses stack for nonchronological backtracking when subspace is exhausted.
    int      lim;           // limit level
#endif /*NONBLOCKING*/


    clause*  binary;        // A temporary binary clause
    lbool*   tags;          //
    veci     tagged;        // (contains: var)
    veci     stack;         // (contains: var)

    veci     order;         // Variable order. (heap) (contains: var)
    veci     trail_lim;     // Separator indices for different decision levels in 'trail'. (contains: int)

    int      root_level;    // Level of first proper decision.
    int      simpdb_assigns;// Number of top-level assignments at last 'simplifyDB()'.
    int      simpdb_props;  // Number of propagations before next 'simplifyDB()'.
    double   random_seed;
    double   progress_estimate;
    int      verbosity;     // Verbosity level. 0=silent, 1=some progress report, 2=everything

    stats    stats;
};

#endif
