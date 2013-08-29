//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef EXCEPTIONHANDLE_H
#define EXCEPTIONHANDLE_H
#ifdef __cplusplus
extern "C" {
#endif
void *_Unwind_Resume() {/* return void*/};
void *__gxx_personality_v0() {/* return void*/};
#ifdef __cplusplus
}
#endif
#endif