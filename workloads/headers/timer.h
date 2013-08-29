//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
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
unsigned long get_time_in_microseconds()
{
    LARGE_INTEGER tickPerSecond, tick;
    QueryPerformanceFrequency(&tickPerSecond);
    QueryPerformanceCounter(&tick);
    return tick.QuadPart*1000000/tickPerSecond.QuadPart;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
