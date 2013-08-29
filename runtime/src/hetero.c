//===-- hetero.c - GPU code execution   --------------------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
/*
* to generate llvm bitcode from clang, compile with:
* clang -fblocks -emit-llvm -S <filename>
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "impl.h"
#include "malloc_shared.h"
#include "timer.h"
//extern long long get_time_in_microseconds();
// this handle has to be created separately somewhere
cl_mem SharedMemoryBufferHandle;
// linear address seen by CPU as the base address of shared area
char* SharedMemoryBuffer; 

#ifndef USE_TBB_MALLOC
extern char *nextSharedMemory;
#endif


#ifdef PRINT_RUNTIME_STATS
unsigned long total_kernel_compile_time = 0; // gpu kernel compilation time
unsigned long total_kernel_execution_time = 0; // gpu kernel execution time obtained from opencl profiler
unsigned long total_kernel_time = 0; // gpu_time
unsigned long total_time = 0; // cpu + gpu time
unsigned long num_offload_calls = 0;
double dynamic_perf_gpu_pct_sum = 0.0;
#endif

void initModule() {

}

cl_command_queue ClCommandQueue;

static cl_platform_id platform;
static cl_device_id ClDevice;
static cl_context ClContext;
cl_event Event;

/*
* Cleanup opencl cpu
*/
void ocl_cleanup() {
	if (ClCommandQueue) { clReleaseCommandQueue(ClCommandQueue); ClCommandQueue = NULL; }
	if (ClContext) { clReleaseContext(ClContext); ClContext = NULL; }
}

/*
*  report error 
*/
void report_err(cl_int error, const char * error_name){
	if (error != CL_SUCCESS) {
		printf("ERROR: %s (%d)\n",error_name,error);
		ocl_cleanup();
		exit(EXIT_FAILURE);
	}
}

/*
* Get the intel opencl platform
*/
cl_platform_id get_intel_opencl_platform(){
	cl_platform_id platforms[10] = { 0 };
	char platform_name[128] = { 0 };
	cl_uint i;

	cl_uint platforms_ctr = 0;
	cl_int err = clGetPlatformIDs(10, platforms, &platforms_ctr);
	for (i = 0; i < platforms_ctr; ++i){
		err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 128 * sizeof(char), platform_name, NULL);
		report_err(err, "clGetPlatFormIDs");
		if (!strcmp(platform_name, "Intel(R) OpenCL"))
			return platforms[i];
	}
	return NULL;
}


/* WARNING!!!! do not change this parameter without changing the reduction template
* this is tied with compiler implementation */
#define LOCAL_SIZE 64

int init_ocl() {

	cl_int err;
	cl_int ClDeviceFreq, ClDeviceUnits;
#ifdef WORK_STEALING
	int ws_size;
#endif


#if 1
	err = clGetPlatformIDs(1,&platform,NULL);
	/*if (err != CL_SUCCESS) {
	fprintf(stderr, "clGetPlatformIDs failed: %d\n",err);
	return err;
	}*/

	/* TODO
	For now, we simply use the first platform returned, but we
	should use the clGetPlatformInfo call to make sure we use the
	the Gen OpenCL platform.
	*/

	err = clGetDeviceIDs(platform,
		CL_DEVICE_TYPE_GPU,
		1,
		&ClDevice,
		NULL);
	if (err != CL_SUCCESS)
	{
		fprintf(stderr,"Error: clGetDeviceIDs %d\n", err);
		return err;
	}
	//err = clGetDeviceInfo(ClDevice,CL_DEVICE_NAME,1024,ClDeviceName,NULL);
	err = clGetDeviceInfo(ClDevice, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &ClDeviceFreq, NULL);
	err |= clGetDeviceInfo(ClDevice, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &ClDeviceUnits, NULL);
	//printf("device name is %s\n",ClDeviceName);
	fprintf(stdout, "GPU MHz=%d Units=%d\n",ClDeviceFreq, ClDeviceUnits);

#else
	platform = get_intel_opencl_platform();
	if( platform == NULL )
	{
		printf("ERROR: Failed to find Intel OpenCL platform.\n");
		return -1;
	}

	context_properties[0] = CL_CONTEXT_PLATFORM;
	context_properties[1] = (cl_context_properties)platform;
	context_properties[2] = NULL;
	ClContext = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_GPU, NULL, NULL, NULL);

	if (ClContext == NULL) {
		printf("ERROR: Failed to create context for Intel OpenCL platform.\n");
		return -1;
	}

	err = clGetContextInfo(ClContext, CL_CONTEXT_DEVICES, 0, NULL, &cb);
	report_err(err, "clGetContextInfo");
	clGetContextInfo(ClContext, CL_CONTEXT_DEVICES, cb, devices, NULL);
	ClDevice = devices[0];
	//err = clGetDeviceInfo(ClDevice,CL_DEVICE_NAME,1024,ClDeviceName,NULL);
	err = clGetDeviceInfo(ClDevice, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &ClDeviceFreq, NULL);
	err |= clGetDeviceInfo(ClDevice, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &ClDeviceUnits, NULL);
	//printf("device name is %s\n",ClDeviceName);
	fprintf(stdout, "GPU MHz=%d Units=%d\n",ClDeviceFreq, ClDeviceUnits);
#endif
	/* TODO
	For now, we simply use the first GPU device returned, but we
	should use the clGetDeviceInfo call to make sure we use the
	integrated Gen device.
	*/

	/* Create the OCL context */
	ClContext = clCreateContext(NULL, /* const cl_context_properties* */
		1,
		&ClDevice,
		NULL,NULL,/* callback function & its data */
		&err);
	if (!ClContext)
	{ 
		fprintf(stderr,"Error: clCreateContext %d\n", err);
		return err;
	}

	ClCommandQueue = clCreateCommandQueue(ClContext,ClDevice,
		CL_QUEUE_PROFILING_ENABLE ,
		&err);
	if (!ClCommandQueue)
	{ 
		fprintf(stderr,"Error:clCreateCommandQueue %d",err);
		return err;
	}

#ifdef _WIN32
	// BTL: Get the start of the TBB scalable allocator's SVM region
	// fprintf(stderr,"BTL: init_ocl: calling get_svm_region\n");
#ifdef USE_TBB_MALLOC
	SharedMemoryBuffer = (char *)get_svm_region();
	SharedMemoryBuffer = (char *)((((uintptr_t)SharedMemoryBuffer) & PAGE_ALIGNMENT_MASK)+PAGE_SIZE);
	//fprintf(stderr,"BTL: init_ocl: get_svm_region() returned 0x%p\n", SharedMemoryBuffer);
	SharedMemoryBufferHandle = clCreateBuffer(ClContext, CL_MEM_READ_WRITE | 
		CL_MEM_USE_HOST_PTR,  
		(size_t)SVM_SIZE, 
		SharedMemoryBuffer,
		&err);
	if (err != CL_SUCCESS) {
		fprintf(stderr,"Error: clCreateBuffer of SVM: %d\n",err);
		return err;
	}
#else
	SharedMemoryBuffer = (char *)malloc(SVM_SIZE);
	nextSharedMemory = SharedMemoryBuffer = (char *)((((uintptr_t)SharedMemoryBuffer) & PAGE_ALIGNMENT_MASK)+PAGE_SIZE);
	SharedMemoryBufferHandle = clCreateBuffer(ClContext, CL_MEM_READ_WRITE | 
		CL_MEM_USE_HOST_PTR,  
		(size_t)SVM_SIZE, 
		SharedMemoryBuffer,
		&err);
	if (err != CL_SUCCESS) {
		fprintf(stderr,"Error: clCreateBuffer of SVM: %d\n",err);
		return err;
	}
#endif
#elif defined(__linux__)
	SharedMemoryBufferHandle = clCreateBuffer(ClContext, CL_MEM_READ_WRITE | CL_MEM_PINNABLE, SVM_SIZE, NULL, &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr,"Error: clCreateBuffer of SVM: %d\n",err);
		return err;
	}
	err = clIntelPinBuffer(SharedMemoryBufferHandle);
	if (err != CL_SUCCESS) {
		fprintf(stderr,"Error: clIntelPinBuffer: %d\n",err);
		return err;
	}
	nextSharedMemory = SharedMemoryBuffer = clIntelMapBuffer(SharedMemoryBufferHandle, &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr,"Error: clIntelMapBuffer: %d\n",err);
		return err;
	}
#else
#error Unsupported platform.
#endif

	return 0;
}
///#######################################################################################################################
void check_command_queue(cl_command_queue * q) {	   
	cl_context tmp_ctx;
	cl_device_id dev_id;
	cl_uint count;
	cl_command_queue_properties ppt;
	char device_name[256];
	cl_uint err;      
	fprintf(stderr, "=======Begin queue check========\n");
	err = clGetCommandQueueInfo(*q, CL_QUEUE_CONTEXT, sizeof(cl_context), &tmp_ctx, NULL);
	fprintf(stderr, "E[%d] Context = %d \n" ,err, tmp_ctx ); 
	err=clGetCommandQueueInfo(*q, CL_QUEUE_DEVICE, sizeof(cl_device_id), &dev_id, NULL);
	fprintf(stderr, "E[%d] Device ID : %d\n" ,err, dev_id );
	err=clGetDeviceInfo(	dev_id,CL_DEVICE_NAME,sizeof(char)*256, (void*)(&device_name[0]),NULL);
	fprintf(stderr, "E[%d] Device Name : %s\n" ,err, device_name );		  
	clGetCommandQueueInfo(*q, CL_QUEUE_REFERENCE_COUNT, 0, &count, NULL);
	fprintf(stderr, "Reference count :%d\n",count);
	clGetCommandQueueInfo(*q, CL_QUEUE_PROPERTIES, 0, &ppt, NULL);
	fprintf(stderr, "Properties :: ");
	if (ppt & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) {
		fprintf(stderr, "CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE ");
	} else {
		fprintf(stderr, "IN_ORDER_EXEC_MODE_ ");
	}
	if (ppt & CL_QUEUE_PROFILING_ENABLE) {
		fprintf(stderr, "CL_QUEUE_PROFILING_ENABLE");
	} else {
		fprintf(stderr, "CL_QUEUE_PROFILING_DISABLED ");
	}
	fprintf(stderr, "\n=======End queue check========\n");
	return;
}
///#######################################################################################################################
int execute_gpu_function_ocl(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void* functionBinary,
	void* context,
	void *gContext) 
{
	cl_int error;

	//unsigned long long start_time, end_time;
	size_t local_size=64; //192; //512;
	/* Set the arguments to the function and launch */
	/* This is what we would want to accelerate with a user-level queue
	between the GPU and CPU */
	cl_kernel kernel = (cl_kernel)functionBinary;
	/*	cl_mem ReductionBufferHandle;
	int total_reduction_object_size=numiters*12;
	printf("Here %d\n",total_reduction_object_size);	

	ReductionBufferHandle = clCreateBuffer(ClContext, CL_MEM_READ_WRITE,
	(size_t)total_reduction_object_size, 
	context,
	&error);
	if (error != CL_SUCCESS) {
	fprintf(stderr,"Error: clCreateBuffer of Reduction: %d\n",error);
	return error;
	}*/
	//fprintf(stderr, "Gpu got num%d off%d\n",numiters,offset);
	clSetKernelArg(kernel,0,sizeof(cl_mem),&SharedMemoryBufferHandle); /* cl_mem */
	clSetKernelArg(kernel,1,sizeof(cl_uint),&SharedMemoryBuffer);
	clSetKernelArg(kernel,2,sizeof(cl_uint),&context);
	//clSetKernelArg(kernel,2,sizeof(cl_mem),&ReductionBufferHandle);
	clSetKernelArg(kernel,3,sizeof(cl_uint),&gContext);
	/****/
	clSetKernelArg(kernel,4,sizeof(cl_uint),&offset);
	/****/
	//	clSetKernelArg(kernel,4,sizeof(cl_mem),&ReductionBufferHandle);
	//printf("num_iters=%d\n", numiters);
	/****/
	/* Note: Using event this way is thread safe because only 1 thread
	at a time will use it per semantics of hetero queue */
	error = clEnqueueNDRangeKernel(ClCommandQueue,
		kernel,
		1, /* work_dim */
		NULL, /* global_work_offset */
		(size_t*)&numiters, /* global_work_size */
		NULL, //&local_size, /* local_work_size */
		0, /* num_events_in_wait_list */
		NULL, /* event_wait_list */
		&Event
		);
	if (error != CL_SUCCESS) {
		fprintf(stderr, "clEnqueueNDRangeKernel failed. Error=%d.\n", error);
		fprintf(stderr, "2)Info num_items=%d, local_size=%d\n", numiters, local_size);
		fprintf(stderr, "*********************FAIL***********************\n");
		check_command_queue(&ClCommandQueue);
		fprintf(stderr, "*********************END FAIL***********************\n");
		exit(-1);
	}
	/*fprintf(stderr, "=====================SUCCESS==================\n");
	check_command_queue(&ClCommandQueue);
	error = clRetainCommandQueue(ClCommandQueue);
	fprintf(stderr, "Retained? %d \n", error==CL_SUCCESS);
	fprintf(stderr, "=====================END SUCCESS==============\n");*/
	/* Wait for kernel to finish execution */
	//error = clFinish(ClCommandQueue);
	//if (error != CL_SUCCESS)
	//	return 0;
	//printf(". ");
	//clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
	//clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
	//printf("\n\nEnqueue exec time(usec)=%f usec\n", (end_time-start_time)*1.0e-3f);
	return 1;
}

void* compile_gpu_program_ocl(gpu_program_t* program,
	dispatch_queue_hetero_t queue) {
		const char* programIr = program->ir;
		//const char* programIr = program->cl_ir;
		cl_int error;
		cl_program clProgram ;
		/* TODO: OCL device list */
		//cl_device_id devices[1];
		//cl_uint num_devices = 1;
#ifdef __linux__
		/* For Linux.
		*/
		size_t programIrLength = program->irSize;
		cl_int status;
		cl_program clProgram = 
			clCreateProgramWithBinary(ClContext,
			1,
			&ClDevice,
			&programIrLength,
			(const unsigned char **)&programIr,
			&status,
			&error);
		if (error != CL_SUCCESS){
			fprintf(stderr,"JITing (clCreateProgramWithBinary) failed. binary status = %d, error code=%d\n.", status, error);
			return NULL;
		}
#else
		/* For Windows, the following relies on the internal build that accepts GHAL IL text as legitimate kernel source.
		*/
		//printf("%s\n",programIr);
		clProgram = 
			clCreateProgramWithSource(ClContext,
			1,
			&programIr,
			NULL,
			&error);
		if (error != CL_SUCCESS){
			fprintf(stderr,"JITing (clCreateProgramWithSource) failed. error code=%d\n", error);
			return NULL;
		}
#endif
		// printf("compile_gpu_program_ocl\n");
		//buildOptions = "";
		/* Note: We can run build program asynchronously by passing a 
		callback function to clBuildProgram. */
		error = clBuildProgram(clProgram,
			0, /* num_devices */
			NULL, /* device_list */
			NULL,
			NULL, /* callback function */
			NULL);
		if (error != CL_SUCCESS) {
			size_t len;
			char buffer[2048];
			fprintf(stderr,"JITing (clBuildProgram) failed. error code=%d\n", error);
			printf("Error: Failed to build program executable\n");            // [8]
			clGetProgramBuildInfo(clProgram, ClDevice, CL_PROGRAM_BUILD_LOG,
				sizeof(buffer), buffer, &len);
			printf("%s\n", buffer);
			return NULL;
		}
		return clProgram;
}

void* get_function_from_program_ocl(void* programBinary,const char* name) {
	cl_int error;
	cl_kernel clKernel = clCreateKernel((cl_program)programBinary,
		name,
		&error);
	if (error != CL_SUCCESS) {
		fprintf(stderr,"clCreateKernel failed. kernel name = %s, error = %d.\n", name, error);
		return NULL;
	}
	return clKernel;
}


/* TODO initialize the runtime */

void* compile_gpu_program(gpu_program_t* program,
	dispatch_queue_hetero_t queue) {
		return compile_gpu_program_ocl(program,queue);
}

int execute_gpu_function(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void* functionBinary,
	void* context,
	void *gContext) {
		return execute_gpu_function_ocl(numiters,offset,queue,functionBinary,context, gContext);
}

void* get_function_from_program(void* programBinary,const char* name) {
	return get_function_from_program_ocl(programBinary,name);
}

void* compile_gpu_module(gpu_program_t* program,
	dispatch_queue_hetero_t queue) {
		void  *programBinary;
#ifdef PRINT_RUNTIME_STATS
		unsigned long start_t, end_t;
		start_t = time_in_microseconds();
#endif
		programBinary = program->binary;
		// printf("compile_gpu_function\n");  
		if (programBinary == NULL) {
			/* TODO: thread safety: acquire a lock on programBinary so only 
			1 thread compiles and updates programBinary */
			/* Note: To hide the latency of compilation, we can create another
			thread that does this compilation concurrently, and let this
			thread continue and evaluate the kernel on the CPU 

			Note: We can compile when the program loads or when the module
			containing IR loads 
			*/
			programBinary = program->binary 
				= compile_gpu_program(program,queue); 
			if (programBinary == NULL) // JITing failed!
				return NULL;
		}
#ifdef PRINT_RUNTIME_STATS
		end_t = time_in_microseconds();
		total_kernel_compile_time += (end_t - start_t);
#endif
		return programBinary;//get_function_from_program(programBinary,gpu_function->name);
}


int gpu_apply(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	gpu_function_t* gpu_function,
	void* context,
	void (fun)(void*,size_t, void *),
	void *gContext) {
		void* functionBinary = gpu_function->binary;
		void *binary;
		if (functionBinary == NULL) {
			/* TODO: thread safety: acquire a lock on functionBinary so only 
			1 thread compiles and updates functionBinary */
			binary = compile_gpu_module(gpu_function->program,queue);
			functionBinary =  gpu_function->binary = get_function_from_program(binary,gpu_function->name);
			if (functionBinary == NULL) { // JITing failed!
				printf("Exiting: JITing failed for function=%s\n", gpu_function->name);
				return 0;
			}
		}
		return execute_gpu_function(numiters,offset,queue,functionBinary,context, gContext);
}


int is_gpu_idle() {
	return 1; /* TODO */
}

gpu_function_t* lookup_gpu_function_f(void (fun)(void*,size_t, void *)) {
	/* Runtime lookup of a function pointer to find its corresponding 
	gpu_function structure */
	return NULL; /* TODO */
}

double elapsed_gpu_in_micro;
int try_gpu_apply_f(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void (fun)(void*,size_t, void *),
	void* context,
	gpu_function_t* gpu_function,
	void *gContext) {
		cl_int error;
		unsigned long long start_time, end_time;

#ifdef __APPLE__
		return 0;
#endif
		int ret;
		/*if (!is_gpu_idle() 
		|| (gpu_function == NULL && (gpu_function = lookup_gpu_function_f(fun)) == NULL))
		return 0;*/
		ret = gpu_apply(numiters,offset,queue,gpu_function,context,fun,gContext);	
		error = clFinish(ClCommandQueue);
		//fprintf(stderr, "[%d,%d]Finish\n", offset, numiters);
		if (error != CL_SUCCESS)
			return 0;
#ifdef PRINT_RUNTIME_STATS
		clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
		clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
		elapsed_gpu_in_micro = (end_time-start_time)*1.0e-3f;
		total_kernel_execution_time += elapsed_gpu_in_micro;
#endif
		//printf("\n\nEnqueue exec time(usec)=%f usec\n", (end_time-start_time)*1.0e-3f);
		return ret;
}

double kernel_time_in_micro_seconds() {
	return elapsed_gpu_in_micro;
}

int try_gpu_apply_no_finish_f(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void (fun)(void*,size_t, void *),
	void* context,
	gpu_function_t* gpu_function,
	void *gContext) {
#ifdef __APPLE__
		return 0;
#endif
		/*if (!is_gpu_idle() 
		|| (gpu_function == NULL && (gpu_function = lookup_gpu_function_f(fun)) == NULL))
		return 0;*/
		int ret;
		ret = gpu_apply(numiters,offset,queue,gpu_function,context,fun,gContext);	
		return ret;
}

int cl_finish() {
#ifdef PRINT_RUNTIME_STATS
	unsigned long long start_time, end_time;
#endif
	cl_uint error = clFinish(ClCommandQueue);
	if (error != CL_SUCCESS) {
		return 0;
	}
#ifdef PRINT_RUNTIME_STATS
	clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
	clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
	elapsed_gpu_in_micro = (end_time-start_time)*1.0e-3f;
	total_kernel_execution_time += elapsed_gpu_in_micro;
#endif
	return CL_SUCCESS;
}


double try_gpu_apply_timer_f(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void (fun)(void*,size_t, void *),
	void* context,
	gpu_function_t* gpu_function,
	void *gContext) {
		cl_int error;
#ifdef __APPLE__
		return 0;
#endif
		/*if (!is_gpu_idle() 
		|| (gpu_function == NULL && (gpu_function = lookup_gpu_function_f(fun)) == NULL))
		return 0;*/
		int ret;
		unsigned long long start_time, end_time;
		double elapsed_gpu_in_micro;
		ret = gpu_apply(numiters,offset,queue,gpu_function,context,fun,gContext);	
		error = clFinish(ClCommandQueue);
		if (error != CL_SUCCESS)
			return 0;
		clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
		clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
		elapsed_gpu_in_micro = (end_time-start_time)*1.0e-3f;
#ifdef PRINT_RUNTIME_STATS
		total_kernel_execution_time += elapsed_gpu_in_micro;
#endif
		return elapsed_gpu_in_micro;
}


double get_kernel_time_in_micro_seconds() {
	unsigned long long start_time, end_time;
	clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
	clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
	elapsed_gpu_in_micro = (end_time-start_time)*1.0e-3f;
	return elapsed_gpu_in_micro;
}


#if 0

int try_gpu_reduce_f(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void (fun)(void*,size_t, void *),
	void (seq_join) (void*,void*),
	void* context,
	gpu_function_t* gpu_function,
	void *gContext) {
		if (!is_gpu_idle() 
			|| (gpu_function == NULL && (gpu_function = lookup_gpu_function_f(fun)) == NULL))
			return 0;
		return gpu_reduce(numiters,offset,queue,gpu_function,context,fun,seq_join, gContext);
}

int execute_reduce_ocl(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	void* functionBinary,
	void* context,
	//void (fun)(void*,size_t, void *),
	void *gContext) 
{
	cl_int error;
	//cl_event Event;
	//unsigned long long start_time, end_time;
	size_t local_size=LOCAL_SIZE;
	/* Set the arguments to the function and launch */
	/* This is what we would want to accelerate with a user-level queue
	between the GPU and CPU */
	cl_kernel kernel = (cl_kernel)functionBinary;
	/*	cl_mem ReductionBufferHandle;
	int total_reduction_object_size=numiters*12;
	printf("Here %d\n",total_reduction_object_size);	

	ReductionBufferHandle = clCreateBuffer(ClContext, CL_MEM_READ_WRITE,
	(size_t)total_reduction_object_size, 
	context,
	&error);
	if (error != CL_SUCCESS) {
	fprintf(stderr,"Error: clCreateBuffer of Reduction: %d\n",error);
	return error;
	}*/
	//	printf("Gpu got num%d off%d\n",numiters,offset);
	clSetKernelArg(kernel,0,sizeof(cl_mem),&SharedMemoryBufferHandle); /* cl_mem */
	clSetKernelArg(kernel,1,sizeof(cl_ulong),&SharedMemoryBuffer);
	clSetKernelArg(kernel,2,sizeof(cl_ulong),&context);
	//clSetKernelArg(kernel,2,sizeof(cl_mem),&ReductionBufferHandle);
	clSetKernelArg(kernel,3,sizeof(cl_ulong),&gContext);
	/****/
	clSetKernelArg(kernel,4,sizeof(cl_uint),&offset);
	/****/
	//	clSetKernelArg(kernel,4,sizeof(cl_mem),&ReductionBufferHandle);
	//printf("num_iters=%d local_size=%d\n", numiters, local_size);
	/****/
	/* Note: Using event this way is thread safe because only 1 thread
	at a time will use it per semantics of hetero queue */
	error = clEnqueueNDRangeKernel(ClCommandQueue,
		kernel,
		1, /* work_dim */
		NULL, /* global_work_offset */
		(size_t*)&numiters, /* global_work_size */
		&local_size, /* local_work_size */
		0, /* num_events_in_wait_list */
		NULL, /* event_wait_list */
		&Event
		);
	if (error != CL_SUCCESS) {
		fprintf(stderr, "clEnqueueNDRangeKernel failed. Error=%d.\n", error);
		fprintf(stderr, "Info num_items=%d, local_size=%d\n", numiters, local_size);
		return 0;
	}

	/* Wait for kernel to finish execution */
	error = clFinish(ClCommandQueue);
	if (error != CL_SUCCESS)
		return 0;
	//printf(". ");
	//clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, NULL);
	//clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
	//printf("\n\nEnqueue exec time(usec)=%f usec\n", (end_time-start_time)*1.0e-3f);
	return 1;
}

int gpu_reduce(int numiters,
	int offset,
	dispatch_queue_hetero_t queue,
	gpu_function_t* gpu_function,
	void* context,
	void (fun)(void*,size_t, void *),
	void (seq_join) (void*,void*),
	void *gContext) {
		void* functionBinary = gpu_function->binary;
		void* joinBinary = gpu_function->gpu_reduce_binary;
		char *reduction_buff;
		int NUM_BUFFERS=numiters;
		int reduction_object_size=0;
		int i=0,j=0;
		char* reduction_memory;
		void *binary;

		//printf("gpu_reduce\n");  
		if (functionBinary == NULL) {
			/* TODO: thread safety: acquire a lock on functionBinary so only 
			1 thread compiles and updates functionBinary */
			binary = compile_gpu_module(gpu_function->program,queue);
			functionBinary =  gpu_function->binary = get_function_from_program(binary,gpu_function->name);
			if (functionBinary == NULL) { // JITing failed!
				printf("Exiting: JITing failed for function=%s\n", gpu_function->name);
				return 0;
			}
		}
		if(seq_join!=NULL){
			joinBinary =  gpu_function->gpu_reduce_binary = get_function_from_program(binary,gpu_function->gpu_reduce_name);
			if (joinBinary == NULL) { // JITing failed!
				printf("Exiting: JITing failed for join function=%s\n", gpu_function->gpu_reduce_name);
				return 0;
			}

			//printf("Reduction\n");
			reduction_object_size=gpu_function->reduction_object_size;
			//printf("red_object_size:%d\n",reduction_object_size);
			//printf("reduction_buffer=%d\n",NUM_BUFFERS);	 
			reduction_memory=(char*)malloc_shared(NUM_BUFFERS*reduction_object_size);
			for(i=0;i<NUM_BUFFERS;i++){//memcopy
				for(j=0;j<reduction_object_size;j++){	
					*(reduction_memory+(i*reduction_object_size)+j)=*((char*)context+j);
				}
				//printf("%d->%d ",i,((dummy_s*)reduction_memory)->a[0]);
			}
		}
		else{
			reduction_memory=context;
		}

		execute_gpu_function(numiters,offset,queue,functionBinary,reduction_memory, gContext);
		//printf("Computation Finished\n");
		//for(i=0;i<NUM_BUFFERS;i++){//memcopy
		//printf("%d->%d ",i,((dummy_s*)reduction_memory)->sum);
		//}

		//  perform reduction
		if(seq_join!=NULL){

			// if numiters is not a function of 
			//printf("Reductin started #=%d\n",numiters);
			execute_reduce_ocl(numiters,offset,queue,joinBinary,reduction_memory,gContext);
			reduction_buff=(char*)reduction_memory;

			for(i=0;i<(NUM_BUFFERS/LOCAL_SIZE);i++){
				char *object=reduction_buff+(i*reduction_object_size);
				//printf("%d->%d ",i,((dummy_s*)object)->sum);
				seq_join(context,object);
			}

			free_shared(reduction_memory);
		}
		return 1;
}



#endif