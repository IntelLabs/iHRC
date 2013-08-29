//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===

#ifndef MALLOC_SHARED_H
#define MALLOC_SHARED_H

#include <stdlib.h>


#ifdef USE_TBB_MALLOC
/* The SVM malloc, free, and related functions are implemented using TBB's scalable malloc. */
#include "tbb/scalable_allocator.h"

/* 
 * malloc_shared: allocate a block of shared memory of size bytes. 
 *   void *malloc_shared(size_t size)
 */
#define malloc_shared(size) \
    scalable_malloc(size)

/* 
 * free_shared: free a previously allocated piece of memory. 
 *   void free_shared(void *ptr)
 */
#define free_shared(ptr) \
    scalable_free(ptr)

/* 
 * realloc_shared: a "realloc" analogue complementing malloc_shared.
 *   void *realloc_shared(void* ptr, size_t size)
 */
#define realloc_shared(ptr, size) \
    scalable_realloc((ptr), (size))

/* 
 * calloc_shared: a "calloc" analogue complementing malloc_shared.
 *   void *calloc_shared(size_t nobj, size_t size)
 */
#define calloc_shared(nobj, size) \
    scalable_calloc((nobj), (size))

/* 
 * aligned_malloc_shared: a posix memalign allocation analogue: the returned 
 * pointer to shared memory will be a multiple of "alignment", which must 
 * be a power of two.
 *   void * aligned_malloc_shared(size_t size, size_t alignment)
 */
#define aligned_malloc_shared(size, alignment) \
    scalable_aligned_malloc((size), (alignment))

/* 
 * aligned_realloc_shared: an "aligned realloc" analogue.
 *   void *aligned_realloc_shared(void* ptr, size_t size, size_t alignment)
 */
#define aligned_realloc_shared(ptr, size, alignment) \
    scalable_aligned_realloc((ptr), (size), (alignment))

/* 
 * aligned_free_shared: an "aligned free" analogue.
 *   void *aligned_free_shared(void *ptr)
 */
#define aligned_free_shared(ptr) \
    scalable_aligned_free(ptr)

#else


#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

#define DLLIMPORT __declspec(dllimport)

#else
#define DLLIMPORT
#endif

/*DLLIMPORT*/ char *malloc_shared(size_t);
/*DLLIMPORT*/ void free_shared(char *);

#ifdef __cplusplus
}
#endif

#endif
#endif /* MALLOC_SHARED_H */
