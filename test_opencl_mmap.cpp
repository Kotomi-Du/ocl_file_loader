#include <CL/cl.h>
#include <windows.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <string>
#include <psapi.h>

// Log process memory usage (working set and private bytes) in MB with a label.
static void log_mem_usage(const char* label) {
  //std::cout << "[MEM] Collecting - sleep 5s" << std::endl;
  //Sleep(5000); // sleep for specified milliseconds
  const char* lbl = (label && label[0]) ? label : nullptr;
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
    const double working_mb = pmc.WorkingSetSize / (1024.0 * 1024.0);
    const double private_mb = pmc.PrivateUsage   / (1024.0 * 1024.0);
    std::cout << "[MEM]";
    if (lbl) std::cout << " " << lbl;
    std::cout << " working_set_mb=" << working_mb
              << " private_mb=" << private_mb << std::endl;
  } else {
    std::cout << "[MEM]";
    if (lbl) std::cout << " " << lbl;
    std::cout << " GetProcessMemoryInfo failed" << std::endl;
  }
}


int main() {
  const wchar_t* file_path = L"ocl_test.bin";
  const unsigned long long FILE_SIZE_BYTES = 2ull * 1024 * 1024 * 1024;  // 2 GiB file for observing usage
  std::vector<unsigned char> expected(static_cast<size_t>(FILE_SIZE_BYTES));
  for (size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<unsigned char>(i & 0xFF);

  std::cout << "[INFO] Starting file write of 2 GiB pattern..." << std::endl;

  // 1) Write file
  {
    HANDLE hf = CreateFileW(file_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(hf != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    const unsigned long long chunk_size = 4ull * 1024 * 1024; // 4 MiB per write
    unsigned long long remaining = FILE_SIZE_BYTES;
    size_t offset = 0;
    while (remaining > 0) {
      const DWORD this_write = static_cast<DWORD>(std::min<unsigned long long>(chunk_size, remaining));
      BOOL ok = WriteFile(hf, expected.data() + offset, this_write, &written, nullptr);
      assert(ok && written == this_write);
      offset += this_write;
      remaining -= this_write;
    }
    CloseHandle(hf);
  }
  std::cout << "[INFO] File write complete." << std::endl;

  // 2) Map file
  HANDLE hf = CreateFileW(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(hf != INVALID_HANDLE_VALUE);
  HANDLE hmap = CreateFileMappingW(hf, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  assert(hmap);
  // Map the entire 2 GiB file; mapping is virtual-address backed and does not commit until touched.
  void* mapped = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(FILE_SIZE_BYTES));
  assert(mapped);
  std::cout << "[INFO] File mapped into memory (full 2 GiB view)." << std::endl;

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
  std::cout << "[INFO] OpenCL context and queue created." << std::endl;

  log_mem_usage("");

  // 4) Buffers: src uses host pointer (the mmap), dst is device-only
  cl_mem src = clCreateBuffer(ctx,
                              CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                              static_cast<size_t>(FILE_SIZE_BYTES),
                              mapped,
                              &err);
  assert(err == CL_SUCCESS);

  cl_mem dst = clCreateBuffer(ctx,
                              CL_MEM_WRITE_ONLY,
                              static_cast<size_t>(FILE_SIZE_BYTES),
                              nullptr,
                              &err);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] OpenCL buffers created (src from map, dst device-only)." << std::endl;

  log_mem_usage("");

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
  std::cout << "[INFO] Kernel arguments set." << std::endl;

  size_t global = static_cast<size_t>(FILE_SIZE_BYTES); // one work-item per byte
  err = clEnqueueNDRangeKernel(q, kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] Kernel enqueued with global size = " << global << std::endl;

  err = clFinish(q);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] Kernel execution finished." << std::endl;

  log_mem_usage("");

  // 6) Read back and validate
  std::vector<unsigned char> out(static_cast<size_t>(FILE_SIZE_BYTES), 0);
  err = clEnqueueReadBuffer(q, dst, CL_TRUE, 0, static_cast<size_t>(FILE_SIZE_BYTES), out.data(), 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] Readback to host completed." << std::endl;

  log_mem_usage("");

  // Show a small sample from both buffers to confirm contents.
  const size_t sample = static_cast<size_t>(std::min<unsigned long long>(32, FILE_SIZE_BYTES));
  std::cout << "[RESULT] Expected bytes: ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(expected[i]) << (i + 1 == sample ? '\n' : ' ');
  }
  std::cout << "[RESULT] Output bytes:   ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(out[i]) << (i + 1 == sample ? '\n' : ' ');
  }

  if (out == expected) {
    std::cout << "[RESULT] PASS: GPU saw the correct data from mmap-backed buffer\n";
  } else {
    std::cerr << "[RESULT] FAIL: Data mismatch\n";
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

