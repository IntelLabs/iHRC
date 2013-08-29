//===-- cpuoffload.cpp - Multicore CPU execution via various APIS   ---------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifdef CPU_TBB_RUNTIME
#include<tbb/tbb.h>
#include<tbb/blocked_range.h>
extern long long get_time_in_microseconds();
class dummy_class{

public:
	int test;
	void *context;
	void *gContext;
	void (*fp)(void*,size_t, void *);
	dummy_class(void (*fp1)(void*,size_t, void *),void* ctxt, void *gtxt):context(ctxt),fp(fp1), gContext(gtxt) {
	}
	void operator()(const tbb::blocked_range<size_t>& r)const{
		//body(i);
		int begin = (int)r.begin();
		int end = (int)r.end();
		for(int index=begin;index<end;index++)
			(*fp)(context,index,gContext);
	}
};
#endif

#include <stdlib.h>

extern "C"
{
void cpu_apply_f(int numiters,
	int offset,
	void (fun)(void*,size_t, void *),
	void (seq_join) (void*,void*),
	void* context, 
	void *gContext) {

#ifdef CPU_TBB_RUNTIME
	tbb::parallel_for(tbb::blocked_range<size_t>(offset,offset+numiters),dummy_class(fun,context,gContext));
#else
	for(int i=offset;i<numiters+offset;i++){
		fun(context,i, gContext);	
	}
#endif
	// TODO add sequential join here
}
double cpu_apply_timer_f(int numiters,
	int offset,
	void (fun)(void*,size_t, void *),
	void (seq_join) (void*,void*),
	void* context, 
	void *gContext) {

#ifdef CPU_TBB_RUNTIME
	tbb::tick_count t0 = tbb::tick_count::now();
	tbb::parallel_for(tbb::blocked_range<size_t>(offset,offset+numiters),dummy_class(fun,context,gContext));
	tbb::tick_count t1 = tbb::tick_count::now();
	return ((t1-t0).seconds()*(1.0e+6f));
#else
	for(int i=offset;i<numiters+offset;i++){
		fun(context,i, gContext);	
	}
#endif
	// TODO add sequential join here
}
};

