ava_name("CUDA");
ava_version("10.0.0");
ava_identifier(CU);
ava_number(3);
//ava_cflags(-DAVA_PRINT_TIMESTAMP);
ava_libs(-lcuda);
ava_export_qualifier();

ava_non_transferable_types {
    ava_handle;
}

ava_functions {
    ava_time_me;
}

#include <cuda.h>

ava_begin_utility;
#include <stdio.h>
ava_end_utility;

//ava_type(CUdeviceptr) {
//    ava_handle;
//}

ava_type(CUresult) {
    ava_success(CUDA_SUCCESS);
}

typedef struct {
    /* argument types */
    int func_argc;
    char func_arg_is_handle[64];
} Metadata;

ava_register_metadata(Metadata);

CUresult CUDAAPI
cuInit(unsigned int Flags);

CUresult CUDAAPI
cuDeviceGet(CUdevice *device,
            int ordinal)
{
    ava_argument(device) {
        ava_out; ava_buffer(1);
    }
}

CUresult CUDAAPI
cuCtxCreate(CUcontext *pctx,
            unsigned int flags,
            CUdevice dev)
{
    ava_argument(pctx) {
        ava_out; ava_element(ava_allocates); ava_buffer(1);
    }
}

ava_utility size_t __helper_load_cubin_size(const char *fname) {
    FILE *fp;
    size_t cubin_size;

    fp = fopen(fname, "rb");
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    cubin_size = ftell(fp);
    fclose(fp);

    return cubin_size;
}

ava_utility void *__helper_load_cubin(const char *fname, size_t size) {
    FILE *fp;
    void *cubin;

    fp = fopen(fname, "rb");
    if (!fp) {
        return NULL;
    }
    cubin = malloc(size);
    fread(cubin, 1, size, fp);
    fclose(fp);

    return cubin;
}

CUresult CUDAAPI
cuModuleLoad(CUmodule *module,
             const char *fname)
{
    ava_disable_native_call;
    int res;

    ava_argument(module) {
        ava_out; ava_buffer(1);
    }
    ava_argument(fname) {
        ava_in; ava_buffer(strlen(fname) + 1);
    }

    ava_implicit_argument
    size_t size = __helper_load_cubin_size(fname);

    ava_implicit_argument
    void *cubin = __helper_load_cubin(fname, size);
    ava_argument(cubin) {
        ava_in; ava_buffer(size);
    }

    if (ava_is_worker) {
        if (!cubin)
            return CUDA_ERROR_FILE_NOT_FOUND;
        res = cuModuleLoadData(module, cubin);
        return res;
    }
}

CUresult CUDAAPI
cuModuleUnload(CUmodule hmod) {
    ava_async;
}

ava_utility void ava_parse_function_args(const char *name, int *func_argc,
                                         char *func_arg_is_handle)
{
    int i = 0, skip = 0;

    *func_argc = 0;
    if (strncmp(name, "_Z", 2)) abort();

    i = 2;
    while (i < strlen(name) && isdigit(name[i])) {
        skip = skip * 10 + name[i] - '0';
        i++;
    }

    i += skip;
    while (i < strlen(name)) {
        switch(name[i]) {
            case 'P':
                func_arg_is_handle[(*func_argc)++] = 1;
                if (i + 1 < strlen(name) && (name[i+1] == 'f' || name[i+1] == 'i'))
                    i++;
                else if (i + 1 < strlen(name) && isdigit(name[i+1])) {
                    skip = 0;
                    while (i + 1 < strlen(name) && isdigit(name[i+1])) {
                        skip = skip * 10 + name[i+1] - '0';
                        i++;
                    }
                    i += skip;
                }
                else
                    abort();
                break;

            case 'f':
            case 'i':
            case 'l':
                func_arg_is_handle[(*func_argc)++] = 0;
                break;

            case 'S':
                func_arg_is_handle[(*func_argc)++] = 1;
                while (i < strlen(name) && name[i] != '_') i++;
                break;

            case 'v':
                i = strlen(name);
                break;

            default:
                abort();
        }
        i++;
    }

    for (i = 0; i < *func_argc; i++) {
        DEBUG_PRINT("function arg#%d it is %sa handle\n", i, func_arg_is_handle[i]?"":"not ");
    }

    /*
    char *demangled_name;
    demangled_name = abi::__cxa_demangle(mangled_name, NULL, NULL, &status);
    printf("name = %s\n", demangled_name);
    free(demangled_name);
    */
}

CUresult CUDAAPI
cuModuleGetFunction(CUfunction *hfunc,
                    CUmodule hmod,
                    const char *name)
{
    ava_argument(hfunc) {
        ava_out; ava_buffer(1);
    }
    ava_argument(name) {
        ava_in; ava_buffer(strlen(name) + 1);
    }

    ava_execute();
    ava_parse_function_args(name, &ava_metadata(*hfunc)->func_argc,
            ava_metadata(*hfunc)->func_arg_is_handle);
}

ava_utility size_t cuLaunchKernel_extra_size(void **extra) {
    size_t size = 1;
    while (extra[size - 1] != CU_LAUNCH_PARAM_END)
        size++;
    return size;
}

CUresult CUDAAPI
cuLaunchKernel(CUfunction f,
               unsigned int gridDimX,
               unsigned int gridDimY,
               unsigned int gridDimZ,
               unsigned int blockDimX,
               unsigned int blockDimY,
               unsigned int blockDimZ,
               unsigned int sharedMemBytes,
               CUstream hStream,
               void **kernelParams,
               void **extra)
{
    ava_async;
    ava_argument(kernelParams) {
        ava_in; ava_buffer(ava_metadata(f)->func_argc);
        ava_element {
            if (ava_metadata(f)->func_arg_is_handle[ava_index]) {
                ava_type_cast(CUdeviceptr*);
                ava_buffer(sizeof(CUdeviceptr));
                //ava_handle;
            }
            else {
                ava_type_cast(int*);
                ava_buffer(sizeof(int));
            }
        }
    }
    ava_argument(extra) {
        ava_in; ava_buffer(cuLaunchKernel_extra_size(extra));
        ava_element ava_buffer(1);
    }
}

CUresult CUDAAPI
cuCtxDestroy(CUcontext ctx)
{
    ava_async;
    ava_argument(ctx) ava_deallocates;
}

CUresult CUDAAPI
cuMemAlloc(CUdeviceptr *dptr,
           size_t bytesize)
{
    ava_argument(dptr) {
        ava_out; ava_buffer(1);
        ava_element { /*ava_handle;*/ ava_allocates; }
    }
}

CUresult CUDAAPI
cuMemcpyHtoD(CUdeviceptr dstDevice,
             const void *srcHost,
             size_t ByteCount)
{
    ava_async;
    ava_argument(srcHost) {
        ava_in; ava_buffer(ByteCount);
    }
}

CUresult CUDAAPI
cuMemcpyDtoH(void *dstHost,
             CUdeviceptr srcDevice,
             size_t ByteCount)
{
    ava_argument(dstHost) {
        ava_out; ava_buffer(ByteCount);
    }
}

CUresult CUDAAPI
cuCtxSynchronize(void);

CUresult CUDAAPI
cuDriverGetVersion(int *driverVersion)
{
    ava_argument(driverVersion) {
        ava_out; ava_buffer(1);
    }
}

CUresult CUDAAPI
cuMemFree(CUdeviceptr dptr)
{
    ava_async;
    ava_argument(dptr) ava_deallocates;
}

CUresult CUDAAPI
cuModuleGetGlobal(CUdeviceptr *dptr, size_t *bytes, CUmodule hmod, const char *name)
{
    ava_argument(dptr) {
        ava_out; ava_buffer(1);
    }
    ava_argument(bytes) {
        ava_out; ava_buffer(1);
    }
    ava_argument(name) {
        ava_in; ava_buffer(strlen(name) + 1);
    }
}

CUresult CUDAAPI cuGetExportTable(const void **ppExportTable,
                                  const CUuuid *pExportTableId)
{
    ava_unsupported;
    ava_argument(ppExportTable) {
        ava_out; ava_buffer(3);
        // https://devtalk.nvidia.com/default/topic/482869/cudagetexporttable-a-total-hack/
    }
    ava_argument(pExportTableId) {
        ava_in; ava_buffer(1);
    }
}
