/** \file     obdd.c
 *  \brief    OBDD implementation, where OBDDs mean BDDs that are ordered, but need not be reduced.
 *  \author   Takahisa Toda
 */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "my_def.h"
#include "obdd.h"

static const int initlen = 65536;
static obdd_t *freelist = NULL;

static uintmax_t nnodes = 0; // the total number of nodes that have been created so far.

obdd_t *top_node = NULL;
obdd_t *bot_node = NULL;

uintmax_t obdd_nnodes(void)
{
    return nnodes;
}

obdd_t *obdd_node(int v, obdd_t *lo, obdd_t *hi)
{
    assert(v > 0);
    if (freelist == NULL)
    {
        freelist = (obdd_t *)malloc(sizeof(obdd_t) * initlen);
        ENSURE_TRUE_MSG(freelist != NULL, "memory allocation failed");
        for (int i = 1; i < initlen; i++)
            freelist[i - 1].aux = (intptr_t)(freelist + i);
        freelist[initlen - 1].aux = (intptr_t)NULL;
    }

    obdd_t *new = freelist;
    freelist = (obdd_t *)freelist->aux;

    obdd_setlabel(v, new);
    new->lo = lo;
    new->hi = hi;
    new->aux = 0;
    nnodes++;

    return new;
}

static void obdd_free(obdd_t *p)
{
    p->aux = (intptr_t)freelist;
    freelist = p;
    assert(nnodes > 0);
    nnodes--;
}

uintmax_t obdd_complete(obdd_t *p)
{
    obdd_t *dummy = obdd_node(INT_MAX, NULL, NULL);
    obdd_t *tail = dummy;
    obdd_t *stack = NULL;

    while (1)
    {
        while (p != NULL && !obdd_const(p) && p->v > 0)
        {
            p->v *= -1;
            p->aux = (intptr_t)stack;
            stack = p;
            tail->nx = p;
            tail = p;
            p = p->lo;
        }
        if ((p = stack) == NULL)
            break;
        stack = (obdd_t *)stack->aux;
        p = p->hi;
    }

    tail->nx = NULL;
    p = dummy->nx;
    obdd_free(dummy);

    uintmax_t size = 0;
    for (; p != NULL; p = p->nx)
    {
        if (p->lo == NULL)
            p->lo = obdd_bot();
        if (p->hi == NULL)
            p->hi = obdd_bot();
        assert(p->v < 0);
        p->v *= -1;
        size++;
    }

    return size;
}

void obdd_delete_all(obdd_t *p)
{
    while (p != NULL)
    {
        obdd_t *nx = p->nx;
        p->nx = NULL;
        obdd_free(p);
        p = nx;
    }
}

uintmax_t obdd_size(obdd_t *p)
{
    uintmax_t n = 0;
    for (; p != NULL; p = p->nx)
        n++;

    return n;
}

/* \brief multiply x by 2^k.
 * \param x     nonzero integer
 * \param k     exponent
 * \return  the resulted integer.
 * \note    UINTPTR_MAX is returned if overflow happens.
 */
static inline uintptr_t my_mul_2exp(uintptr_t x, int k)
{
    if (x <= (UINTPTR_MAX >> k))
        x = (x << k);
    else
        return UINTPTR_MAX;

    return x;
}

intptr_t obdd_nsols(int n, obdd_t *p)
{
    obdd_t **list = (obdd_t **)malloc(sizeof(obdd_t *) * (n + 1));
    ENSURE_TRUE_MSG(list != NULL, "memory allocation failed");
    for (int i = 0; i <= n; i++)
        list[i] = NULL;

    for (obdd_t *s = p; s != NULL; s = s->nx)
    {
        int v = obdd_label(s);
        assert(v <= n);
        s->aux = (intptr_t)list[v];
        list[v] = s;
    }

    obdd_top()->aux = 1;
    obdd_bot()->aux = 0;
    uintptr_t result;
    for (int i = n; i > 0; i--)
    {
        for (obdd_t *s = list[i]; s != NULL;)
        {
            obdd_t *nx = (obdd_t *)s->aux;
            int j = obdd_const(s->hi) ? n + 1 : obdd_label(s->hi);
            result = my_mul_2exp((uintptr_t)s->hi->aux, j - i - 1);
            intptr_t c1 = result > INTPTR_MAX ? INTPTR_MAX : (intptr_t)result;

            j = obdd_const(s->lo) ? n + 1 : obdd_label(s->lo);
            result = my_mul_2exp((uintptr_t)s->lo->aux, j - i - 1);
            intptr_t c2 = result > INTPTR_MAX ? INTPTR_MAX : (intptr_t)result;

            s->aux = c1 <= INTPTR_MAX - c2 ? c1 + c2 : INTPTR_MAX;
            s = nx;
        }
    }

    result = my_mul_2exp((uintptr_t)p->aux, obdd_label(p) - 1);
    result = result > INTPTR_MAX ? INTPTR_MAX : (intptr_t)result;

    free(list);

    return result;
}

#ifdef GMP
void obdd_nsols_gmp(mpz_t result, int n, obdd_t *p)
{
    obdd_t **list = (obdd_t **)malloc(sizeof(obdd_t *) * (n + 1));
    ENSURE_TRUE_MSG(list != NULL, "memory allocation failed");
    for (int i = 0; i <= n; i++)
        list[i] = NULL;

    int m = 0;
    for (obdd_t *s = p; s != NULL; s = s->nx)
    {
        int v = obdd_label(s);
        s->aux = (intptr_t)list[v];
        list[v] = s;
        m++;
    }

    m += 2; // include terminal nodes.
    const int size = m;
    mpz_t *a = (mpz_t *)malloc(sizeof(mpz_t) * m);
    ENSURE_TRUE_MSG(a != NULL, "memory allocation failed");
    for (int i = 0; i < m; i++)
        mpz_init(a[i]);

    assert(m > 0);
    obdd_top()->aux = (intptr_t)--m;
    mpz_set_ui(a[m], 1);
    assert(m > 0);
    obdd_bot()->aux = (intptr_t)--m;
    mpz_set_ui(a[m], 0);

    for (int i = n; i > 0; i--)
    {
        for (obdd_t *s = list[i]; s != NULL;)
        {
            obdd_t *nx = (obdd_t *)s->aux;
            int j = obdd_const(s->hi) ? n + 1 : obdd_label(s->hi);
            intptr_t m1 = s->hi->aux;
            if (j - i - 1 > 0)
                mpz_mul_2exp(a[m1], a[m1], j - i - 1);

            j = obdd_const(s->lo) ? n + 1 : obdd_label(s->lo);
            intptr_t m2 = s->lo->aux;
            if (j - i - 1 > 0)
                mpz_mul_2exp(a[m2], a[m2], j - i - 1);

            assert(m > 0);
            s->aux = (intptr_t)--m;
            mpz_add(a[m], a[m1], a[m2]);
            s = nx;
        }
    }
    assert(m == 0);

    m = p->aux;
    if (obdd_label(p) - 1 > 0)
        mpz_mul_2exp(a[m], a[m], obdd_label(p) - 1);
    mpz_set(result, a[m]);

    for (int i = 0; i < size; i++)
        mpz_clear(a[i]);
    free(list);
    free(a);
}
#endif

int obdd_to_dot(int n, obdd_t *p, FILE *out)
{
    if (obdd_const(p))
    {
        ENSURE_TRUE_WARN(0, "invalid input");
        return ST_FAILURE;
    }

    obdd_t **list = (obdd_t **)malloc(sizeof(obdd_t *) * (n + 1));
    ENSURE_TRUE_MSG(list != NULL, "memory allocation failed");
    for (int i = 0; i <= n; i++)
        list[i] = NULL;

    for (obdd_t *s = p; s != NULL; s = s->nx)
    {
        int v = obdd_label(s);
        s->aux = (intptr_t)list[v];
        list[v] = s;
    }

    int res = fseek(out, 0L, SEEK_SET);
    ENSURE_SUCCESS(res);

    fprintf(out, "digraph obdd {\n");
    fprintf(out, "{rank = same; %jd %jd}\n", (intptr_t)obdd_top(), (intptr_t)obdd_bot());
    for (int i = 1; i <= n; i++)
    {
        fprintf(out, "{rank = same;");
        for (obdd_t *s = list[i]; s != NULL; s = (obdd_t *)s->aux)
            fprintf(out, " %jd", (intptr_t)s);
        fprintf(out, "}\n");
    }
    for (int i = n; i > 0; i--)
    {
        for (obdd_t *s = list[i]; s != NULL;)
        {
            obdd_t *t = (obdd_t *)s->aux;
            fprintf(out, "%jd [label = %d];\n", (intptr_t)s, i);
            fprintf(out, "%jd -> %jd ;\n", (intptr_t)s, (intptr_t)s->hi);
            fprintf(out, "%jd -> %jd [style = dotted];\n", (intptr_t)s, (intptr_t)s->lo);
            s = t;
        }
    }
    fprintf(out, "%jd [label = 1,shape=box];\n", (intptr_t)obdd_top());
    fprintf(out, "%jd [label = 0,shape=box];\n", (intptr_t)obdd_bot());
    fprintf(out, "}\n");

    free(list);

    return ST_SUCCESS;
}

// Decompose bdd into satisfying assignments.
// static uintptr_t obdd_decompose_main(FILE *out, int n, obdd_t* p, uintptr_t (*func)(FILE *, int, int, int*))
static uintptr_t obdd_decompose_main(FILE *out, int n, obdd_t *p, uintptr_t (*func)(int, int, int *))
{
    uintptr_t total = 0; // total number of total solutions

    int *a = (int *)malloc(sizeof(int) * (n + 1));
    ENSURE_TRUE_MSG(a != NULL, "memory allocation failed");
    for (int i = 0; i <= n; i++)
        a[i] = 0;

    obdd_t **b = (obdd_t **)malloc(sizeof(obdd_t *) * (n + 1));
    ENSURE_TRUE_MSG(b != NULL, "memory allocation failed");
    for (int i = 0; i <= n; i++)
        b[i] = NULL;

    int s = 0; // index of a
    int t = 0; // index of b
    while (1)
    {
        while (!(p == obdd_bot() || p == obdd_top()))
        {
            b[t++] = p;
            a[s++] = -(p->v);
            p = p->lo;
        }
        if (p == obdd_top())
        {
            // uintptr_t result = func(out, s, n, a);
            uintptr_t result = func(s, n, a);
            if (total < UINTPTR_MAX - result)
                total += result;
            else
                total = UINTPTR_MAX;
        }

        if (t <= 0)
            break; // b is empty
        p = b[--t];
        while (a[--s] > 0)
            ;
        a[s] = abs(a[s]);
        s++;
        p = p->hi;
    }

    free(b);
    free(a);
    return total;
}

/* \brief print a partial assignment that is stored in a.
 * \param   out     pointer to output file
 * \param   s       length of a in which valid values are contained, which may be less than the actual length of a.
 * \param   n       the number of variables
 * \return  the number of total assignments
 */
static uintptr_t fprintf_partial(FILE *out, int s, int n, int *a)
{
    int prev = 0;
    uintptr_t sols = 1;
    for (int j = 0; j < s; j++)
    {
        fprintf(out, "%d ", a[j]);
        sols = my_mul_2exp(sols, abs(a[j]) - prev - 1);
        prev = abs(a[j]);
    }
    fprintf(out, "0\n");

    return my_mul_2exp(sols, n - prev);
}

/*
 * added by T. Soh
 */
static uintptr_t fprintf_partial_soh(int s, int n, int *a)
{
    int prev = 0;
    uintptr_t sols = 1;
    for (int j = 0; j < s; j++)
    {
        if (a[j] < 0)
        {
            printf("0");
        }
        else
        {
            printf("1");
        }
        // printf("%d ", a[j]);
        sols = my_mul_2exp(sols, abs(a[j]) - prev - 1);
        prev = abs(a[j]);
    }
    printf("\n");

    return my_mul_2exp(sols, n - prev);
}

uintptr_t obdd_decompose(FILE *out, int n, obdd_t *p)
{
    //    return obdd_decompose_main(out, n, p, fprintf_partial);
    return obdd_decompose_main(out, n, p, fprintf_partial_soh);
}
