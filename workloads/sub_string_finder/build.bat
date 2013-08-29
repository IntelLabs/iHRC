@ECHO OFF
rem ==============================================================================
rem Copyright (c) 2013 Intel Corporation.
rem All rights reserved.
rem ==============================================================================
rem 
rem Developed by:
rem 
rem     Programming Systems Lab, Intel Labs
rem 
rem     Intel Corporation
rem 
rem     http://www.intel.com
rem 
rem Permission is hereby granted, free of charge, to any person obtaining a copy of
rem this software and associated documentation files (the "Software"), to deal with
rem the Software without restriction, including without limitation the rights to
rem use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
rem of the Software, and to permit persons to whom the Software is furnished to do
rem so, subject to the following conditions:
rem 
rem     * Redistributions of source code must retain the above copyright notice,
rem       this list of conditions and the following disclaimers.
rem 
rem     * Redistributions in binary form must reproduce the above copyright notice,
rem       this list of conditions and the following disclaimers in the
rem       documentation and/or other materials provided with the distribution.
rem 
rem     * Neither the names of the LLVM Team, University of Illinois at
rem       Urbana-Champaign, nor the names of its contributors may be used to
rem       endorse or promote products derived from this Software without specific
rem       prior written permission.
rem 
rem THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
rem IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
rem FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
rem CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
rem LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
rem OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
rem SOFTWARE.
rem ==============================================================================
@ECHO ON


mkdir gen
del /Q gen\substring*.bc
del /Q gen\substring*.ll
del /Q substring.exe
del /Q gen\*.l.s 
del /Q gen\*.l.cl

set CXX_FLAGS=-DUSE_TBB_MALLOC -DCONCORD -D_CONSOLE_ -D_GNUC_ 
set CXX_HEADERS=-I%TBB_PATH%\include -IC:\mingw64\include\c++\4.4.7\x86_64-w64-mingw32 -IC:\mingw64\include\c++\4.4.7  -IC:\mingw64\x86_64-w64-mingw32\include 
set CXX_LIBS=%RUNTIME_PATH%\iHRC.dll  %TBB_LIB_PATH%\tbbmalloc.dll 


clang++.exe -m64 -w %CXX_FLAGS% %CXX_HEADERS%  -c -emit-llvm sub_string_finder.cpp -o gen\substring.bc

cd gen

opt -O2 substring.bc -o substring_o.bc
opt -heterotbb substring_o.bc -o  substring_prog_o.bc
opt -O2 substring_prog_o.bc -o substring_prog.bc

opt -hetero substring_prog.bc -o  substring_hetero.bc 

opt -heterogpu substring_hetero.bc -o  substring_gpu_temp.bc 
opt -heteroplusclean substring_gpu_temp.bc -o  substring_gpu.bc -debug
llvm-dis substring_gpu.bc
llc -march=opencl substring_gpu.ll -o kernel.l.s


llvm-link substring_prog.bc kernel_annotations.bc -o substring_cpu_prog.bc
opt -O2 substring_cpu_prog.bc -o  substring_cpuo.bc
opt -heterocpu substring_cpuo.bc -o  substring_cpu.bc -debug
llc -mtriple=x86_64-w64-mingw32 substring_cpu.bc -o substring.s

g++ -c -m64 ..\..\headers\g++_exception_handle.c -o exception.o
x86_64-w64-mingw32-g++.exe -O2 -m64 substring.s exception.o %CXX_LIBS% -o ..\substring 

cd ..
