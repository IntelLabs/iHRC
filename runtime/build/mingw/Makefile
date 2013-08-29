# ==============================================================================
# Copyright (c) 2013 Intel Corporation.
# All rights reserved.
# ==============================================================================
# 
# Developed by:
# 
#     Programming Systems Lab, Intel Labs
# 
#     Intel Corporation
# 
#     http://www.intel.com
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal with
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimers.
# 
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimers in the
#       documentation and/or other materials provided with the distribution.
# 
#     * Neither the names of the LLVM Team, University of Illinois at
#       Urbana-Champaign, nor the names of its contributors may be used to
#       endorse or promote products derived from this Software without specific
#       prior written permission.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
# SOFTWARE.
# ==============================================================================


# Compiler used to build
CC = gcc
CXX = g++
LD = $(CC)

# Root directory
TOP = ../..

# OpenCL SDK installation directory
OPENCLSDK = "C:\Program Files (x86)\Intel\OpenCL SDK\3.0"

# source file directory
SRC_DIR = $(TOP)/src

# include file directory
INCLUDE = -I$(TOP)/include

# opencl include directory 
INCLUDE += -I$(OPENCLSDK)/include
STATIC_LIB += -L$(OPENCLSDK)/lib/x64 -lOpenCL


# obj directory
OBJDIR = mingw

CFLAGS += -m64

# include tbb 
ifneq ($(TBBMALLOC),false)
CFLAGS += -DUSE_TBB_MALLOC
INCLUDE += -I$(TOP)/../tbb41_20130613oss/include
ifeq ($(DEBUG),true)
#LIB += -static -L../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_debug
#LIB += -ltbbmalloc_debug.dll
DY_LIB += ../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_debug/tbb_debug.dll ../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_debug/tbbmalloc_debug.dll
else
#LIB += -static -L../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_release 
#LIB += -ltbbmalloc.dll
DY_LIB += ../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_release/tbb.dll ../../../tbb41_20130613oss/build/windows_intel64_gcc_mingw_release/tbbmalloc.dll
endif
endif

ifeq ($(DEBUG),true)
CFLAGS += -g
else
CFLAGS += -O3
endif	

LIBNAME = iHRC

TARGET_DLL = iHRC.dll
TARGET_LIB = libiHRC.a

C_SRCS = hetero.c win_wrapper.c 
C_SRCS += malloc_shared.c
CPP_SRCS = dllmain.cpp cpuoffload.cpp

C_OBJS = $(C_SRCS:.c=.o)
CPP_OBJS = $(CPP_SRCS:.cpp=.o)

ALL: $(TARGET_DLL)

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $? -o $@

%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -c $? -o $@

$(TARGET_DLL): $(C_OBJS) $(CPP_OBJS)
	$(CC) -shared -o $@ $^ -static $(STATIC_LIB) $(DY_LIB) -Wl,--output-def,$(LIBNAME).def,--out-implib,$(TARGET_LIB)

clean:
	rm -f $(TARGET_DLL) $(TARGET_LIB) $(LIBNAME).def $(C_OBJS) $(CPP_OBJS)
