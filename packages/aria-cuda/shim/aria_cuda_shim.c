/*
 * aria_cuda_shim.c — CUDA runtime FFI bindings for Aria
 *
 * Wraps CUDA Runtime API + cuBLAS into a C-callable interface.
 * Compile: gcc -shared -fPIC -O2 -o libaria_cuda_shim.so aria_cuda_shim.c
 *          -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart -lcublas
 *
 * IMPORTANT ABI NOTES (Aria gotchas):
 *   - Aria's flt32 passes as double at C ABI level → all float params are double
 *   - "free" in extern names triggers compiler bug → renamed to "release"
 *   - Device pointers are int64 opaque handles (no raw pointers cross FFI)
 *   - Host float buffers managed via int64 handles (Aria has no float arrays)
 */

#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Error handling
 * ============================================================ */

static char g_last_error[512] = "";

static void set_error(const char *msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static void set_cuda_error(cudaError_t err) {
    set_error(cudaGetErrorString(err));
}

static void set_cublas_error(cublasStatus_t status) {
    switch (status) {
        case CUBLAS_STATUS_SUCCESS:          set_error("cuBLAS success"); break;
        case CUBLAS_STATUS_NOT_INITIALIZED:  set_error("cuBLAS not initialized"); break;
        case CUBLAS_STATUS_ALLOC_FAILED:     set_error("cuBLAS alloc failed"); break;
        case CUBLAS_STATUS_INVALID_VALUE:    set_error("cuBLAS invalid value"); break;
        case CUBLAS_STATUS_ARCH_MISMATCH:    set_error("cuBLAS architecture mismatch"); break;
        case CUBLAS_STATUS_MAPPING_ERROR:    set_error("cuBLAS mapping error"); break;
        case CUBLAS_STATUS_EXECUTION_FAILED: set_error("cuBLAS execution failed"); break;
        case CUBLAS_STATUS_INTERNAL_ERROR:   set_error("cuBLAS internal error"); break;
        default:                             set_error("cuBLAS unknown error"); break;
    }
}

const char* aria_cuda_last_error(void) {
    return g_last_error;
}

/* ============================================================
 * Device management
 * ============================================================ */

int aria_cuda_device_count(void) {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return count;
}

int aria_cuda_set_device(int device) {
    cudaError_t err = cudaSetDevice(device);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_get_device(void) {
    int device = 0;
    cudaError_t err = cudaGetDevice(&device);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return device;
}

static struct cudaDeviceProp g_props;
static int g_props_loaded = 0;
static int g_props_device = -1;

static int ensure_props(int device) {
    if (g_props_loaded && g_props_device == device) return 0;
    cudaError_t err = cudaGetDeviceProperties(&g_props, device);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    g_props_loaded = 1;
    g_props_device = device;
    return 0;
}

const char* aria_cuda_device_name(int device) {
    if (ensure_props(device) != 0) return "unknown";
    return g_props.name;
}

long long aria_cuda_device_total_memory(int device) {
    if (ensure_props(device) != 0) return -1;
    return (long long)g_props.totalGlobalMem;
}

int aria_cuda_device_compute_major(int device) {
    if (ensure_props(device) != 0) return -1;
    return g_props.major;
}

int aria_cuda_device_compute_minor(int device) {
    if (ensure_props(device) != 0) return -1;
    return g_props.minor;
}

int aria_cuda_device_max_threads(int device) {
    if (ensure_props(device) != 0) return -1;
    return g_props.maxThreadsPerBlock;
}

int aria_cuda_device_sm_count(int device) {
    if (ensure_props(device) != 0) return -1;
    return g_props.multiProcessorCount;
}

int aria_cuda_device_warp_size(int device) {
    if (ensure_props(device) != 0) return -1;
    return g_props.warpSize;
}

int aria_cuda_device_sync(void) {
    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_device_reset(void) {
    cudaError_t err = cudaDeviceReset();
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    g_props_loaded = 0;
    return 0;
}

/* ============================================================
 * GPU memory management (int64 opaque handles)
 * ============================================================ */

long long aria_cuda_malloc(long long size_bytes) {
    void *ptr = NULL;
    cudaError_t err = cudaMalloc(&ptr, (size_t)size_bytes);
    if (err != cudaSuccess) { set_cuda_error(err); return 0; }
    return (long long)(uintptr_t)ptr;
}

/* Named "release" to avoid Aria "free" keyword bug */
int aria_cuda_release(long long device_ptr) {
    cudaError_t err = cudaFree((void*)(uintptr_t)device_ptr);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_memcpy_h2d(long long device_dst, long long host_src, long long size_bytes) {
    cudaError_t err = cudaMemcpy((void*)(uintptr_t)device_dst,
                                  (const void*)(uintptr_t)host_src,
                                  (size_t)size_bytes, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_memcpy_d2h(long long host_dst, long long device_src, long long size_bytes) {
    cudaError_t err = cudaMemcpy((void*)(uintptr_t)host_dst,
                                  (const void*)(uintptr_t)device_src,
                                  (size_t)size_bytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_memcpy_d2d(long long device_dst, long long device_src, long long size_bytes) {
    cudaError_t err = cudaMemcpy((void*)(uintptr_t)device_dst,
                                  (void*)(uintptr_t)device_src,
                                  (size_t)size_bytes, cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

int aria_cuda_memset(long long device_ptr, int value, long long size_bytes) {
    cudaError_t err = cudaMemset((void*)(uintptr_t)device_ptr, value, (size_t)size_bytes);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return 0;
}

long long aria_cuda_mem_get_free(void) {
    size_t free_b = 0, total_b = 0;
    cudaError_t err = cudaMemGetInfo(&free_b, &total_b);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return (long long)free_b;
}

long long aria_cuda_mem_get_total(void) {
    size_t free_b = 0, total_b = 0;
    cudaError_t err = cudaMemGetInfo(&free_b, &total_b);
    if (err != cudaSuccess) { set_cuda_error(err); return -1; }
    return (long long)total_b;
}

/* ============================================================
 * Host float buffer management (int64 handles for Aria)
 *
 * Aria has no raw float arrays, so we manage host-side buffers
 * here and return opaque int64 handles.
 * ============================================================ */

long long aria_cuda_host_alloc(int count) {
    float *buf = (float*)calloc((size_t)count, sizeof(float));
    if (!buf) { set_error("host alloc failed"); return 0; }
    return (long long)(uintptr_t)buf;
}

void aria_cuda_host_release(long long handle) {
    if (handle != 0) free((void*)(uintptr_t)handle);
}

/* Aria flt32 passes as double at ABI level — receive double, store as float */
void aria_cuda_host_set(long long handle, int index, double value) {
    ((float*)(uintptr_t)handle)[index] = (float)value;
}

/* Return double because Aria flt32 returns as double at ABI level */
double aria_cuda_host_get(long long handle, int index) {
    return (double)((float*)(uintptr_t)handle)[index];
}

/* Upload host float buffer to new GPU allocation, return device handle */
long long aria_cuda_host_upload(long long host_handle, int count) {
    long long nbytes = (long long)count * (long long)sizeof(float);
    long long d_ptr = aria_cuda_malloc(nbytes);
    if (d_ptr == 0) return 0;
    if (aria_cuda_memcpy_h2d(d_ptr, host_handle, nbytes) != 0) {
        aria_cuda_release(d_ptr);
        return 0;
    }
    return d_ptr;
}

/* Download GPU data into existing host buffer */
int aria_cuda_host_download(long long host_handle, long long device_handle, int count) {
    return aria_cuda_memcpy_d2h(host_handle, device_handle, (long long)count * (long long)sizeof(float));
}

/* ============================================================
 * cuBLAS — GPU-accelerated linear algebra
 * ============================================================ */

static cublasHandle_t g_cublas_handle = NULL;

int aria_cuda_blas_init(void) {
    if (g_cublas_handle) return 0;
    cublasStatus_t status = cublasCreate(&g_cublas_handle);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

int aria_cuda_blas_shutdown(void) {
    if (!g_cublas_handle) return 0;
    cublasStatus_t status = cublasDestroy(g_cublas_handle);
    g_cublas_handle = NULL;
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

/*
 * SGEMM: C = alpha * A * B + beta * C
 * A is (M x K), B is (K x N), C is (M x N)
 * Column-major order (cuBLAS convention)
 * alpha/beta are double (Aria flt32 ABI), cast to float internally
 */
int aria_cuda_blas_sgemm(int M, int N, int K,
                         double alpha_d,
                         long long d_A, int lda,
                         long long d_B, int ldb,
                         double beta_d,
                         long long d_C, int ldc) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return -1;
    }
    float alpha = (float)alpha_d;
    float beta  = (float)beta_d;
    cublasStatus_t status = cublasSgemm(g_cublas_handle,
                                         CUBLAS_OP_N, CUBLAS_OP_N,
                                         M, N, K,
                                         &alpha,
                                         (const float*)(uintptr_t)d_A, lda,
                                         (const float*)(uintptr_t)d_B, ldb,
                                         &beta,
                                         (float*)(uintptr_t)d_C, ldc);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

/*
 * SGEMM with transpose options
 * trans_a/trans_b: 0 = no transpose, 1 = transpose
 */
int aria_cuda_blas_sgemm_ex(int trans_a, int trans_b,
                            int M, int N, int K,
                            double alpha_d,
                            long long d_A, int lda,
                            long long d_B, int ldb,
                            double beta_d,
                            long long d_C, int ldc) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return -1;
    }
    float alpha = (float)alpha_d;
    float beta  = (float)beta_d;
    cublasOperation_t opA = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t opB = trans_b ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasStatus_t status = cublasSgemm(g_cublas_handle,
                                         opA, opB,
                                         M, N, K,
                                         &alpha,
                                         (const float*)(uintptr_t)d_A, lda,
                                         (const float*)(uintptr_t)d_B, ldb,
                                         &beta,
                                         (float*)(uintptr_t)d_C, ldc);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

/* SAXPY: y = alpha * x + y (on device) */
int aria_cuda_blas_saxpy(int n, double alpha_d, long long d_x, long long d_y) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return -1;
    }
    float alpha = (float)alpha_d;
    int incx = 1, incy = 1;
    cublasStatus_t status = cublasSaxpy(g_cublas_handle, n, &alpha,
                                         (const float*)(uintptr_t)d_x, incx,
                                         (float*)(uintptr_t)d_y, incy);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

/* SSCAL: x = alpha * x (on device) */
int aria_cuda_blas_sscal(int n, double alpha_d, long long d_x) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return -1;
    }
    float alpha = (float)alpha_d;
    int incx = 1;
    cublasStatus_t status = cublasSscal(g_cublas_handle, n, &alpha,
                                         (float*)(uintptr_t)d_x, incx);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return -1;
    }
    return 0;
}

/* SDOT: returns dot product of two device vectors (as double for Aria ABI) */
double aria_cuda_blas_sdot(int n, long long d_x, long long d_y) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return 0.0;
    }
    float result = 0.0f;
    int incx = 1, incy = 1;
    cublasStatus_t status = cublasSdot(g_cublas_handle, n,
                                        (const float*)(uintptr_t)d_x, incx,
                                        (const float*)(uintptr_t)d_y, incy,
                                        &result);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return 0.0;
    }
    return (double)result;
}

/* SNRM2: returns L2 norm of device vector (as double for Aria ABI) */
double aria_cuda_blas_snrm2(int n, long long d_x) {
    if (!g_cublas_handle) {
        set_error("cuBLAS not initialized");
        return 0.0;
    }
    float result = 0.0f;
    int incx = 1;
    cublasStatus_t status = cublasSnrm2(g_cublas_handle, n,
                                          (const float*)(uintptr_t)d_x, incx,
                                          &result);
    if (status != CUBLAS_STATUS_SUCCESS) {
        set_cublas_error(status);
        return 0.0;
    }
    return (double)result;
}

/* ============================================================
 * Convenience: GPU vector ops via cuBLAS
 * ============================================================ */

/* y = y + x (in-place on device) */
int aria_cuda_vec_add(int n, long long d_x, long long d_y) {
    return aria_cuda_blas_saxpy(n, 1.0, d_x, d_y);
}

/* x = alpha * x (in-place on device) */
int aria_cuda_vec_scale(int n, double alpha, long long d_x) {
    return aria_cuda_blas_sscal(n, alpha, d_x);
}
