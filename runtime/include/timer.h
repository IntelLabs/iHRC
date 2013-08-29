//===-- timer.h - Windows timer implementation   ------------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef TIMER_H
#define TIMER_H

#ifdef _WIN32
#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

static unsigned long time_in_microseconds()
{
    LARGE_INTEGER tickPerSecond, tick;
    QueryPerformanceFrequency(&tickPerSecond);
    QueryPerformanceCounter(&tick);
    return tick.QuadPart*1000000/tickPerSecond.QuadPart;
}

#ifdef __cplusplus
}
#endif

#else // other platforms are not supported yet

#endif

#endif
