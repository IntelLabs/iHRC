//===-- malloc_shared.c - naive memory managemenr routine   ---------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef USE_TBB_MALLOC
#include <stdlib.h>
#include <stdio.h>
#include "malloc_shared.h"

char *nextSharedMemory;

char *malloc_shared(size_t size)
{
  char *ret; 
  ret = nextSharedMemory;
  nextSharedMemory += size;
  return ret;
}
void free_shared(char *p)
{
}

#endif
