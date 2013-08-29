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
del /Q gen\*.bc
del /Q gen\*.ll
del /Q seismic.exe
del /Q gen\*.l.cl
del /Q gen\*.s

set CXX_FLAGS=-DTEST -DUSE_TBB_MALLOC -DCONCORD -D_CONSOLE_ -D_GNUC_ 
set CXX_HEADERS=-I%TBB_PATH%\include -IC:\mingw64\include\c++\4.4.7\x86_64-w64-mingw32 -IC:\mingw64\include\c++\4.4.7  -IC:\mingw64\x86_64-w64-mingw32\include 
set CXX_LIBS=%RUNTIME_PATH%\iHRC.dll  %TBB_LIB_PATH%\tbbmalloc.dll 

clang++.exe -m64 -w -O2 %CXX_FLAGS% %CXX_HEADERS% -c -emit-llvm SeismicSimulation.cpp -o gen\seismic_.bc

clang++.exe -m64 -w -O2 %CXX_FLAGS% %CXX_HEADERS% -c -emit-llvm convideo.cpp -o gen\convideo.bc

cd gen

llvm-link seismic_.bc convideo.bc -o seismic.bc

opt -O2 seismic.bc -o seismic_o.bc
opt -heterotbb seismic_o.bc -o  seismic_prog_o.bc 
opt -O2 seismic_prog_o.bc -o seismic_prog.bc

opt -hetero seismic_prog.bc -o  seismic_hetero.bc 

opt -heterogpu seismic_hetero.bc -o  seismic_gpu_temp.bc 
opt -heteroplusclean seismic_gpu_temp.bc -o  seismic_gpu.bc -debug
llvm-dis seismic_gpu.bc
llc -O2 -march=opencl seismic_gpu.ll -o kernel.l.s

llvm-link seismic_prog.bc kernel_annotations.bc -o seismic_cpu_prog.bc
opt -O2 seismic_cpu_prog.bc -o  seismic_cpuo.bc
opt -heterocpu seismic_cpuo.bc -o  seismic_cpu.bc 
llc -mtriple=x86_64-w64-mingw32 seismic_cpu.bc -o seismic.s

x86_64-w64-mingw32-g++.exe -m64 seismic.s %CXX_LIBS% -o ..\seismic.exe 

cd ..
