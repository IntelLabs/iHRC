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

set iHRC_MODE=Release

rem Mingw path
set MINGW_PATH=C:\mingw64\bin;C:\mingw64\msys\bin;

set HETERO_DIR=%cd%\..

rem runtime path
rem set RUNTIME_PATH=%HETERO_DIR%\runtime\build\msvc\%iHRC_MODE%
set RUNTIME_PATH=%HETERO_DIR%\runtime\build\mingw\

rem compiler binary path
set LLVM_CPU_PATH=%HETERO_DIR%\compiler\build\mingw\bin

rem tbb path
set TBB_PATH=%HETERO_DIR%\tbb41_20130613oss
rem set TBB_LIB_PATH=%TBB_PATH%\build\vsproject\intel64\%iHRC_MODE%
set TBB_LIB_PATH=%TBB_PATH%\build\windows_intel64_gcc_mingw_release\

rem intel ocl path
set INTEL_OPENCL_SDK_DIR="C:\Program Files (x86)\Intel\OpenCL SDK\3.0\bin\x64"
set TBB_DLL_LIB_PATH=%INTEL_OPENCL_SDK_DIR%\tbb

rem set path now
set PATH=%RUNTIME_PATH%;%TBB_LIB_PATH%;%TBB_DLL_LIB_PATH%;%LLVM_CPU_PATH%;%MINGW_PATH%;%INTEL_OPENCL_SDK_DIR%;%PATH%

rem visual studio path (not needed for mingw)
rem "C:\Program Files\Microsoft Visual Studio 11.0\Common7\Tools\vsvars32.bat"
