#include <CL/cl.h>
#include <windows.h>
#include <vector>
#include <iostream>
#include <cassert>

int main() {
  const wchar_t* file_path = L"ocl_test.bin";
  const size_t N = 1024;
  std::vector<unsigned char> expected(N);
  for (size_t i = 0; i < N; ++i) expected[i] = static_cast<unsigned char>(i & 0xFF);

  // 1) Write file
  {
    HANDLE hf = CreateFileW(file_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(hf != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    WriteFile(hf, expected.data(), static_cast<DWORD>(expected.size()), &written, nullptr);
    CloseHandle(hf);
  }

  // 2) Map file
  HANDLE hf = CreateFileW(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(hf != INVALID_HANDLE_VALUE);
  HANDLE hmap = CreateFileMappingW(hf, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  assert(hmap);
  void* mapped = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, N);
  assert(mapped);

   // 3) Set up OpenCL
  cl_int err = CL_SUCCESS;
  cl_uint num_platforms = 0;
  cl_platform_id platform = nullptr;
  err = clGetPlatformIDs(1, &platform, &num_platforms);
  assert(err == CL_SUCCESS && platform);

  cl_uint num_devices = 0;
  cl_device_id device = nullptr;
  err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, &num_devices);
  assert(err == CL_SUCCESS && device);

  cl_context ctx = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
  assert(err == CL_SUCCESS);

  cl_command_queue q = clCreateCommandQueue(ctx, device, 0, &err);
  assert(err == CL_SUCCESS);

  // 4) Buffers: src uses host pointer (the mmap), dst is device-only
  cl_mem src = clCreateBuffer(ctx,
                              CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                              N,
                              mapped,
                              &err);
  assert(err == CL_SUCCESS);

  cl_mem dst = clCreateBuffer(ctx,
                              CL_MEM_WRITE_ONLY,
                              N,
                              nullptr,
                              &err);
  assert(err == CL_SUCCESS);

  // Check whether the buffer sees host edits after creation (detects copy vs. zero-copy).
  unsigned char* mapped_bytes = static_cast<unsigned char*>(mapped);
  const size_t check_offset = 17;  // arbitrary byte to mutate
  const unsigned char injected_value = 0xAB;
  mapped_bytes[check_offset] = injected_value;   // mutate the file-backed mapping
  expected[check_offset] = injected_value;       // mirror expected so validation reflects this change

  // See what host pointer OpenCL reports for the buffer; if it differs, driver likely copied.
  void* reported_host_ptr = nullptr;
  err = clGetMemObjectInfo(src, CL_MEM_HOST_PTR, sizeof(void*), &reported_host_ptr, nullptr);
  assert(err == CL_SUCCESS);
  std::cout << "Mapped address: " << mapped << "\n";
  std::cout << "OpenCL reported host ptr: " << reported_host_ptr << "\n";
  std::cout << "Host ptr matches mapped? " << (reported_host_ptr == mapped ? "yes" : "no") << "\n";

  // 5) Kernel to copy bytes
  const char* ksrc = R"CLC(
  __kernel void copy_buf(__global const uchar* src, __global uchar* dst) {
    size_t i = get_global_id(0);
    dst[i] = src[i];
  }
  )CLC";

  cl_program prog = clCreateProgramWithSource(ctx, 1, &ksrc, nullptr, &err);
  assert(err == CL_SUCCESS);
  err = clBuildProgram(prog, 1, &device, nullptr, nullptr, nullptr);
  if (err != CL_SUCCESS) {
    // Print build log on failure
    size_t log_size = 0;
    clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
    std::string log(log_size, '\0');
    clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
    std::cerr << "Build log:\n" << log << std::endl;
    assert(false);
  }

  cl_kernel kernel = clCreateKernel(prog, "copy_buf", &err);
  assert(err == CL_SUCCESS);

  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &src);
  err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &dst);
  assert(err == CL_SUCCESS);

  size_t global = N; // one work-item per byte
  err = clEnqueueNDRangeKernel(q, kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);

  err = clFinish(q);
  assert(err == CL_SUCCESS);

  // 6) Read back and validate
  std::vector<unsigned char> out(N, 0);
  err = clEnqueueReadBuffer(q, dst, CL_TRUE, 0, N, out.data(), 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);

  // Show a small sample from both buffers to confirm contents.
  const size_t sample = check_offset+2;
  std::cout << "Expected bytes: ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(expected[i]) << (i + 1 == sample ? '\n' : ' ');
  }
  std::cout << "Output bytes:   ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(out[i]) << (i + 1 == sample ? '\n' : ' ');
  }

  if (out == expected) {
    std::cout << "PASS: GPU saw the correct data from mmap-backed buffer\n";
  } else {
    std::cerr << "FAIL: Data mismatch\n";
    return 1;
  }

  // Cleanup
  clReleaseKernel(kernel);
  clReleaseProgram(prog);
  clReleaseMemObject(src);
  clReleaseMemObject(dst);
  clReleaseCommandQueue(q);
  clReleaseContext(ctx);
  if (mapped)   UnmapViewOfFile(mapped);
  if (hmap)     CloseHandle(hmap);
  if (hf)       CloseHandle(hf);
  DeleteFileW(file_path);  // e.g., L"ocl_test.bin"
  return 0;
}

