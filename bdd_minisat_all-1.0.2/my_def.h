/** \file    my_def.h
 *  \brief   Common definitions and utilities
 *  \author  Takahisa Toda
 */
#ifndef MY_DEF_H
#define MY_DEF_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define ST_SUCCESS (0)
#define ST_FAILURE (-1)

#define INFTY  INT32_MAX
typedef int32_t itemval; //!< item value type, which must be able to contain INFTY and negative integers.

#define MAX_RECURSION_DEPTH (8192) //!< Guard against stack overfow.

#ifdef RECDEPTH_GUARD
#define INC_RECDEPTH(v)                                                                                 \
  do {                                                                                                  \
    if (v > MAX_RECURSION_DEPTH)                                                                       \
      {                                                                                                 \
fprintf(stderr, "error:%s:%d:%s: maximum recursion depth exceeded\n", __FILE__, __LINE__, __func__);  \
  exit(EXIT_FAILURE);                                                                                   \
      }                                                                                                 \
    v++;                                                                                                \
  } while(0)

#define DEC_RECDEPTH(v) \
  do {                  \
    assert(v > 0);      \
    v--;                \
  } while(0)
#else
#define INC_RECDEPTH(v)  do{} while(0)
#define DEC_RECDEPTH(v)  do{} while(0)
#endif /*RECDEPTH_GUARD*/


#define ENSURE_TRUE_WARN(v, msg)                                                      \
  do {                                                                                \
    if (!(v))                                                                         \
      {                                                                               \
        fprintf(stderr, "warning:%s:%d:%s:%s\n", __FILE__, __LINE__, __func__, msg);  \
      }                                                                               \
  } while(0)

#define ENSURE_TRUE(v)                                                      \
  do {                                                                      \
    if (!(v))                                                               \
      {                                                                     \
  fprintf(stderr, "error:%s:%d:%s failed\n", __FILE__, __LINE__, __func__); \
  exit(EXIT_FAILURE);                                                       \
      }                                                                     \
  } while(0)

#define ENSURE_TRUE_MSG(v, msg)                                               \
  do {                                                                        \
    if (!(v))                                                                 \
      {                                                                       \
  fprintf(stderr, "error:%s:%d:%s:%s\n", __FILE__, __LINE__, __func__, msg);  \
  exit(EXIT_FAILURE);                                                         \
      }                                                                       \
  } while(0)


#define ENSURE_SUCCESS(v)                                                   \
  do {                                                                      \
    if ((v) != ST_SUCCESS)                                                  \
      {                                                                     \
  fprintf(stderr, "error:%s:%d:%s failed\n", __FILE__, __LINE__, __func__); \
  exit(EXIT_FAILURE);                                                       \
      }                                                                     \
  } while(0)

#define ENSURE_SUCCESS_MSG(v, msg)                                            \
  do {                                                                        \
    if ((v) != ST_SUCCESS)                                                    \
      {                                                                       \
  fprintf(stderr, "error:%s:%d:%s:%s\n", __FILE__, __LINE__, __func__, msg);  \
  exit(EXIT_FAILURE);                                                         \
      }                                                                       \
  } while(0)


#endif /*MY_DEF_H*/
