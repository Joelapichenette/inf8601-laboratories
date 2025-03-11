#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include "log.h"
#include "sinoscope.h"

typedef struct modified_sinoscope {
    unsigned int buffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int taylor;
    unsigned int interval;
} modified_sinoscope_t;

int sinoscope_opencl_init(sinoscope_opencl_t* opencl, cl_device_id opencl_device_id, unsigned int width, unsigned int height) {

	/* Initialiser le matériel, contexte et queue de commandes */
	cl_int error = 0;
	opencl->device_id = opencl_device_id;

	opencl->context = clCreateContext(0, 1, &opencl_device_id, NULL, NULL, &error);
	if (error != CL_SUCCESS) {
		LOG_ERROR("Failed to create OpenCL context: %d", error);
		goto fail_exit;
	}

	opencl->queue = clCreateCommandQueue(opencl->context, opencl_device_id, 0, &error);
	if (error != CL_SUCCESS) { 
		LOG_ERROR("Failed to create OpenCL command queue: %d", error);
		goto fail_exit;
	}

	const int buffer_size = width * height * 3;
	opencl->buffer = clCreateBuffer(opencl->context, CL_MEM_READ_WRITE, buffer_size, NULL, &error);
	if (error != CL_SUCCESS) {
		LOG_ERROR("Failed to create OpenCL buffer: %d", error);
		goto fail_exit;
	}

	size_t size;
	char* src = NULL;
	opencl_load_kernel_code(&src, &size);
	cl_program pgm = clCreateProgramWithSource(opencl->context, 1, (const char**) &src, &size, &error);
	free(src);

	if(error != CL_SUCCESS) {
		LOG_ERROR("Failed to create OpenCL program: %d", error);
		goto fail_exit;
	}

	const char* options = "-I " __OPENCL_INCLUDE__;

	error = clBuildProgram(pgm, 1, &opencl_device_id, options, NULL, NULL);
	opencl_print_build_log(pgm, opencl_device_id);
	if(error != CL_SUCCESS) {
		LOG_ERROR("Failed to build OpenCL program: %d", error);
		goto fail_exit;
	}


	opencl->kernel = clCreateKernel(pgm, "sinoscope_image_kernel", &error);
	if(error != CL_SUCCESS) {
		LOG_ERROR("Failed to create OpenCL kernel: %d", error);
		goto fail_exit;
	}

	return 0;

fail_exit:
	return -1;
}

void sinoscope_opencl_cleanup(sinoscope_opencl_t* opencl)
{
	clReleaseKernel(opencl->kernel);
	clReleaseCommandQueue(opencl->queue);
	clReleaseContext(opencl->context);
	clReleaseMemObject(opencl->buffer);
}

int sinoscope_image_opencl(sinoscope_t* sinoscope) {
	if (sinoscope == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }
	cl_int error = 0;

	modified_sinoscope_t mod_sinoscope;
	mod_sinoscope.buffer_size = sinoscope->buffer_size;
	mod_sinoscope.width = sinoscope->width;
	mod_sinoscope.height = sinoscope->height;
	mod_sinoscope.taylor = sinoscope->taylor;
	mod_sinoscope.interval = sinoscope->interval;

	error = clSetKernelArg(sinoscope->opencl->kernel, 0, sizeof(cl_mem), &(sinoscope->opencl->buffer));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 1, sizeof(modified_sinoscope_t), &mod_sinoscope);
	error |= clSetKernelArg(sinoscope->opencl->kernel, 2, sizeof(float), &(sinoscope->interval_inverse));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 3, sizeof(float), &(sinoscope->time));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 4, sizeof(float), &(sinoscope->max));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 5, sizeof(float), &(sinoscope->phase0));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 6, sizeof(float), &(sinoscope->phase1));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 7, sizeof(float), &(sinoscope->dx));
	error |= clSetKernelArg(sinoscope->opencl->kernel, 8, sizeof(float), &(sinoscope->dy));

	if(error != CL_SUCCESS) {
		LOG_ERROR("Failed to set OpenCL kernel arguments: %d", error);
		goto fail_exit;
	}

	const size_t total_size = sinoscope->width * sinoscope->height;
	error = clEnqueueNDRangeKernel(sinoscope->opencl->queue, sinoscope->opencl->kernel, 1, NULL, &total_size, NULL, 0, NULL, NULL);
	if(error != CL_SUCCESS) {
		LOG_ERROR("Failed to start OpenCL kernel threads: %d", error);
		goto fail_exit;
	}

    error = clEnqueueReadBuffer(sinoscope->opencl->queue, sinoscope->opencl->buffer, CL_TRUE, 0, sinoscope->buffer_size, sinoscope->buffer, 0 , NULL, NULL);
    if (error != CL_SUCCESS) {
        LOG_ERROR("Failed to read buffer: %d", error);
        goto fail_exit;
    }
	
	error = clFinish(sinoscope->opencl->queue);
    if (error != CL_SUCCESS) {
        LOG_ERROR("Failed to wait finish OpenCL kernel: %d", error);
        goto fail_exit;
    }

	return 0;

fail_exit:
	return -1;
}