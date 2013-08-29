/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/
//#include <iostream>
#include <iostream>
#include <string>
//#include <algorithm>
#include <stdio.h>
//#include <stdlib.h>
#include <new>
#ifdef CONCORD
#include "../headers/win_hetero.h"
#include "../headers/malloc_shared.h" 
#include "../headers/win_hetero_runtime.h" 
#else
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "tbb/tick_count.h"
using namespace tbb;
#endif

#ifdef _WIN32
#include <windows.h>
#include <time.h>

long long get_time_in_microseconds()
{
    LARGE_INTEGER tickPerSecond, tick;
    QueryPerformanceFrequency(&tickPerSecond);
    QueryPerformanceCounter(&tick);
    return tick.QuadPart*1000000/tickPerSecond.QuadPart;
}
#endif

//using namespace std;
static const   int N = 22; //23;

#ifdef CONCORD
class substring{
//	union { int p1[2];int* c_str; };
//	union { int p2[2];int *max_array;  };
//	union { int p3[2];int *pos_array; };
	/*char*/ int *c_str;
	int *max_array;
	int *pos_array;
    int size;

public:

#ifdef _GNUC_
__attribute__((noinline))
#endif
void operator() ( int i ) const { 

		int max_size = 0, max_pos = 0;
		int limit = 0;
		int lsize = size;
		int /*char*/ *lc_str = c_str;

		/*for (  int j = 0; j < lsize; ++j){
			if (j != i){
				limit = (i<j) ? (lsize - j) : (lsize - i);
				for (  int k = 0; k < limit; ++k) {
					if (lc_str[i + k] != lc_str[j + k]) break;
					if (k > max_size) {
						max_size = k;
						max_pos = j;
					}
				}
			}
		}*/
		for (int j = 0; j < i; ++j){
				limit = (lsize - i);
				for (  int k = 0; k < limit; ++k) {
					if (lc_str[i + k] != lc_str[j + k]) break;
					if (k > max_size) {
						max_size = k;
						max_pos = j;
					}
				}
		}
		for (int j = i+1; j < lsize; ++j){
				limit = (lsize - j);
				for (  int k = 0; k < limit; ++k) {
					if (lc_str[i + k] != lc_str[j + k]) break;
					if (k > max_size) {
						max_size = k;
						max_pos = j;
					}
				}
		}
			max_array[i] = max_size;
			pos_array[i] = max_pos;

}
 
substring(/*char*/int *s,   int *m,   int *p,  int siz) :
    c_str(s), max_array(m), pos_array(p),size(siz) { } 
};
#endif

/******************************************/
#ifdef VERIFY_RESULTS

class SubStringFinder_seq {

	const std::string str;
    int *max_array;
    int *pos_array;
    int size;

public:
	void operator() ( int iters,int dummy) const {

		for ( int i = 0; i != iters; ++i ) {
			int max_size = 0, max_pos = 0;
			int limit=0;
			for (int j = 0; j < str.size(); ++j){
				if (j != i) {
					limit = str.size()-std::max(i,j);
					for (int k = 0; k < limit; ++k) {
						if (str[i + k] != str[j + k]) break;
						if (k > max_size) {
							max_size = k;
							max_pos = j;
						}
					}
				}
			}
				max_array[i] = max_size;
				pos_array[i] = max_pos;
		}
	}
	SubStringFinder_seq(std::string &s,   int *m,   int *p) :
    str(s), max_array(m), pos_array(p) { }
};
#endif

int main() 
{
#ifdef CONCORD
    long long stime,etime;
    hetero_init();
#else
    tick_count stime, etime;
#endif
    double elapsed;
    std::string str[N] = { std::string("a"), std::string("b") };

    for (  int i = 2; i < N; ++i) str[i] = str[i-1]+str[i-2];
    std::string &to_scan = str[N-1];
    int num_elem = to_scan.size();
    printf("Size: %d\n",num_elem);

#ifdef CONCORD
    int *c_str = (int*)malloc_shared(sizeof(int)*num_elem);
    //char *c_str = (char*)malloc_shared(sizeof(char)*num_elem);
    for(int i=0;i<num_elem;i++) {
        c_str[i] = to_scan[i]; // 1;
    }
    int *max_hetero = (int*)malloc_shared(sizeof(int)*num_elem);
    int *pos_hetero = (int*)malloc_shared(sizeof(int)*num_elem);
#endif
    int *max = new   int[num_elem];
    int *pos = new   int[num_elem];

#ifdef CONCORD
#ifdef VERIFY_RESULTS
    SubStringFinder_seq sub=SubStringFinder_seq( to_scan, max, pos );
	stime = get_time_in_microseconds(); 
	sub(num_elem,0);
	etime = get_time_in_microseconds();
	elapsed = (etime - stime)/1000000.0f;
	printf("SEQ: Time Elapsed with STRING class=%f\n",elapsed);
#endif /* VERIFY_RESULTS */

	stime = get_time_in_microseconds();
	substring *substr=
            new(malloc_shared(sizeof(substring)))
                substring( c_str, max_hetero, pos_hetero,num_elem );
	parallel_for_hetero(num_elem,*substr, GPU);
	etime = get_time_in_microseconds();
	elapsed = (etime - stime)/1000000.0f;
	printf("CONCORD: Time Elapsed with integer array=%f\n",elapsed);
#else  /* !CONCORD */
	parallel_for(blocked_range<  int>(0, num_elem ),
	               SubStringFinder( to_scan, max, pos ) );
#endif /* !CONCORD */


#ifdef VERIFY_RESULTS
 for (  int i = 0; i < num_elem; i+=1)
	if ((max_hetero[i]-max[i]) != 0) {
        for (  int i = 0; i < num_elem; i+=1)
            if ((max_hetero[i]-max[i]) != 0) {
				//printf("iter=%d; gpu=%d, seq=%d",i,max_hetero[i], max[i]);
				printf("FAILED test.\n");
				return 1;
			}
	}
  printf("PASSED test.\n");
#endif

  delete[] pos;
  delete[] max;

#ifdef CONCORD
	hetero_cleanup();
        //free_shared(c_str);
        //free_shared((char *)pos_hetero);
        //free_shared((char *)max_hetero);
#endif
        return 0;
}

