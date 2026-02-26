#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <CL/cl_ext_intel.h>
#include <windows.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <string>
#include <psapi.h>
#include <cstring>

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

// Check if an extension is supported by device
static bool is_extension_supported(cl_device_id device, const char* ext_name) {
  size_t ext_size = 0;
  clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &ext_size);
  if (ext_size == 0) return false;
  
  std::string extensions(ext_size, '\0');
  clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, ext_size, &extensions[0], nullptr);
  return extensions.find(ext_name) != std::string::npos;
}

// Print Intel-specific device info
static void print_intel_device_info(cl_device_id device) {
  std::cout << "\n[INTEL] Intel OpenCL Extensions Info:" << std::endl;
  
  // Check for Intel-specific extensions
  const char* intel_extensions[] = {
    "cl_intel_unified_shared_memory",
    "cl_intel_required_subgroup_size",
    "cl_intel_subgroups",
    "cl_intel_accelerator",
    "cl_intel_advanced_motion_estimation",
    "cl_intel_planar_yuv",
    "cl_intel_packed_yuv",
    "cl_intel_motion_estimation",
    "cl_intel_device_side_avc_motion_estimation",
    "cl_intel_media_block_io"
  };
  
  for (const char* ext : intel_extensions) {
    bool supported = is_extension_supported(device, ext);
    std::cout << "[INTEL]   " << ext << ": " 
              << (supported ? "SUPPORTED" : "NOT SUPPORTED") << std::endl;
  }
  
  // Get device version
  size_t version_size = 0;
  clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &version_size);
  std::string version(version_size, '\0');
  clGetDeviceInfo(device, CL_DEVICE_VERSION, version_size, &version[0], nullptr);
  std::cout << "[INTEL] Device Version: " << version << std::endl;
  
  // Get driver version
  size_t driver_size = 0;
  clGetDeviceInfo(device, CL_DRIVER_VERSION, 0, nullptr, &driver_size);
  std::string driver(driver_size, '\0');
  clGetDeviceInfo(device, CL_DRIVER_VERSION, driver_size, &driver[0], nullptr);
  std::cout << "[INTEL] Driver Version: " << driver << std::endl;
  std::cout << std::endl;
}

// Compile a simple copy kernel once and reuse it.
static cl_kernel build_copy_kernel(cl_context ctx, cl_device_id device) {
  const char* ksrc = R"CLC(
  __kernel void copy_buf(__global const uchar* src, __global uchar* dst) {
    size_t i = get_global_id(0);
    dst[i] = src[i];
  }
  )CLC";

  cl_int err = CL_SUCCESS;
  cl_program prog = clCreateProgramWithSource(ctx, 1, &ksrc, nullptr, &err);
  assert(err == CL_SUCCESS);
  err = clBuildProgram(prog, 1, &device, nullptr, nullptr, nullptr);
  if (err != CL_SUCCESS) {
    size_t log_size = 0;
    clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
    std::string log(log_size, '\0');
    clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
    std::cerr << "Build log:\n" << log << std::endl;
    assert(false);
  }

  cl_kernel kernel = clCreateKernel(prog, "copy_buf", &err);
  assert(err == CL_SUCCESS);
  // program can be released after kernel is created
  clReleaseProgram(prog);
  return kernel;
}

// Mode 1: classic buffer using CL_MEM_USE_HOST_PTR on the mmap view.
static bool run_buffer_mode(cl_context ctx,
              cl_command_queue q,
              cl_kernel kernel,
              void* mapped,
              std::vector<unsigned char>& expected,
              size_t bytes,
              bool mutate_after_mapped) {
  cl_int err = CL_SUCCESS;
  std::cout << "[INFO] RUM BUFFER mode." << std::endl;
  log_mem_usage("before cl buffer creation");
  cl_mem src = clCreateBuffer(ctx,
      CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL,
      bytes,
      mapped,
      &err);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] CL src buffer created from mmapped file" << std::endl;
  log_mem_usage("");

  cl_mem dst = clCreateBuffer(ctx,
                              CL_MEM_WRITE_ONLY,
                              bytes,
                              nullptr,
                              &err);
  assert(err == CL_SUCCESS);
  std::cout << "[INFO] CL dst buffer created" << std::endl;
  log_mem_usage("");

  if (mutate_after_mapped) {
      unsigned char* mapped_bytes = static_cast<unsigned char*>(mapped);
      const size_t check_offset = 17;
      const unsigned char injected_value = 0xAB;
      mapped_bytes[check_offset] = injected_value;
      expected[check_offset] = injected_value;
	  std::cout << "[INFO] Mutated mapped file after buffer creation at offset " << check_offset << std::endl;
      log_mem_usage("");
  }

  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &src);
  err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &dst);
  assert(err == CL_SUCCESS);


  size_t global = bytes;
  err = clEnqueueNDRangeKernel(q, kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);
  clFinish(q);
  std::cout << "[INFO] Kernel Execution Done" << std::endl;
  log_mem_usage("");

  std::vector<unsigned char> out(bytes, 0);
  err = clEnqueueReadBuffer(q, dst, CL_TRUE, 0, bytes, out.data(), 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);

  std::cout << "[INFO] Readback to host completed." << std::endl;

  log_mem_usage("");

  // Show a small sample from both buffers to confirm contents.
  const size_t sample = static_cast<size_t>(std::min<unsigned long long>(32, bytes));
  std::cout << "[RESULT] Expected bytes: ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(expected[i]) << (i + 1 == sample ? '\n' : ' ');
  }
  std::cout << "[RESULT] Output bytes:   ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(out[i]) << (i + 1 == sample ? '\n' : ' ');
  }

  clReleaseMemObject(src);
  clReleaseMemObject(dst);

  return out == expected;
}

// Mode 2: USM (Intel) using host-alloc (CPU resident but device-visible) and shared alloc for dst.
static bool run_usm_mode(cl_platform_id platform,
                         cl_context ctx,
                         cl_device_id device,
                         cl_command_queue q,
                         cl_kernel kernel,
                         void* mapped,
                         std::vector<unsigned char>& expected,
                         size_t bytes,
                         bool mutate_after_mapped) {
  if (!is_extension_supported(device, "cl_intel_unified_shared_memory")) {
    std::cerr << "[USM] cl_intel_unified_shared_memory not supported on this device" << std::endl;
    return false;
  }

  std::cout << "[INFO] RUM USM mode." << std::endl;
  log_mem_usage("before usm buffer creation");

  auto pHostAlloc = reinterpret_cast<clHostMemAllocINTEL_fn>(
      clGetExtensionFunctionAddressForPlatform(platform, "clHostMemAllocINTEL"));
  auto pSharedAlloc = reinterpret_cast<clSharedMemAllocINTEL_fn>(
      clGetExtensionFunctionAddressForPlatform(platform, "clSharedMemAllocINTEL"));
  auto pMemFree = reinterpret_cast<clMemFreeINTEL_fn>(
      clGetExtensionFunctionAddressForPlatform(platform, "clMemFreeINTEL"));

  if (!pHostAlloc || !pSharedAlloc || !pMemFree) {
    std::cerr << "[USM] Failed to load USM entry points" << std::endl;
    return false;
  }

  cl_int err = CL_SUCCESS;
  void* usm_src = pHostAlloc(ctx, nullptr, bytes, 0, &err); // host-visible, device-visible
  assert(err == CL_SUCCESS && usm_src);
  std::cout << "[INFO] USM src buffer created" << std::endl;
  log_mem_usage("");

  std::memcpy(usm_src, mapped, bytes);
  std::cout << "[INFO] USM src buffer initialized from mapped file" << std::endl;
  log_mem_usage("");

  void* usm_dst = pSharedAlloc(ctx, device, nullptr, bytes, 0, &err); // shared USM
  assert(err == CL_SUCCESS && usm_dst);
  std::cout << "[INFO] USM dst buffer created" << std::endl;
  log_mem_usage("");


  if (mutate_after_mapped) {
    unsigned char* mapped_bytes = static_cast<unsigned char*>(mapped);
    const size_t check_offset = 17;
    const unsigned char injected_value = 0xAB;
    mapped_bytes[check_offset] = injected_value;
    expected[check_offset] = injected_value;
    std::cout << "[INFO] Mutated mapped file after buffer creation at offset " << check_offset << std::endl;
    log_mem_usage("");
  }

  err  = clSetKernelArgSVMPointer(kernel, 0, usm_src);
  err |= clSetKernelArgSVMPointer(kernel, 1, usm_dst);
  assert(err == CL_SUCCESS);

  size_t global = bytes;
  err = clEnqueueNDRangeKernel(q, kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
  assert(err == CL_SUCCESS);
  clFinish(q);
  std::cout << "[INFO] Kernel Execution Done" << std::endl;
  log_mem_usage("");

  std::vector<unsigned char> out(bytes, 0);
  std::memcpy(out.data(), usm_dst, bytes);
  std::cout << "[INFO] Readback to host completed." << std::endl;
  log_mem_usage("");

  // Show a small sample from both buffers to confirm contents.
  const size_t sample = static_cast<size_t>(std::min<unsigned long long>(32, bytes));
  std::cout << "[RESULT] Expected bytes: ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(expected[i]) << (i + 1 == sample ? '\n' : ' ');
  }
  std::cout << "[RESULT] Output bytes:   ";
  for (size_t i = 0; i < sample; ++i) {
    std::cout << static_cast<unsigned>(out[i]) << (i + 1 == sample ? '\n' : ' ');
  }

  pMemFree(ctx, usm_src);
  pMemFree(ctx, usm_dst);

  return out == expected;
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
  log_mem_usage("");

  // 2) Map file
  HANDLE hf = CreateFileW(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(hf != INVALID_HANDLE_VALUE);
  HANDLE hmap = CreateFileMappingW(hf, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  assert(hmap);
  // Map the entire 2 GiB file; mapping is virtual-address backed and does not commit until touched.
  void* mapped = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(FILE_SIZE_BYTES));
  assert(mapped);
  std::cout << "[INFO] Mapped file created (full 2 GiB view)." << std::endl;
  log_mem_usage("");

  // 2.5) Touch the entire mapped file
  std::cout << "[INFO] Touching the entire mapped file to commit pages..." << std::endl;
  {
      volatile unsigned char* touch = static_cast<unsigned char*>(mapped);
      for (size_t i = 0; i < expected.size(); ++i) {
          touch[i] = touch[i];
      }
  }
  log_mem_usage("");

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


  cl_kernel kernel = build_copy_kernel(ctx, device);
  std::cout << "[INFO] OpenCL kernel built -- copy value from src to dst." << std::endl;

  log_mem_usage("");

  // Run classic buffer path
  bool ok_buffer = run_buffer_mode(ctx, q, kernel, mapped, expected, static_cast<size_t>(FILE_SIZE_BYTES), true /*mutate_after_create*/);
  log_mem_usage("exit buffer mode function");
  std::cout << "[RESULT] Buffer mode " << (ok_buffer ? "PASS" : "FAIL") << std::endl;

  // Run USM path

  bool ok_usm = run_usm_mode(platform, ctx, device, q, kernel, mapped, expected,  static_cast<size_t>(FILE_SIZE_BYTES), true /*mutate_after_create*/);
  log_mem_usage("exit usm mode function");
  std::cout << "[RESULT] USM mode " << (ok_usm ? "PASS" : "FAIL") << std::endl;

  // Cleanup
  clReleaseKernel(kernel);
  clReleaseCommandQueue(q);
  clReleaseContext(ctx);
  if (mapped)   UnmapViewOfFile(mapped);
  if (hmap)     CloseHandle(hmap);
  if (hf)       CloseHandle(hf);
  DeleteFileW(file_path);  // e.g., L"ocl_test.bin"
  return (ok_buffer && ok_usm) ? 0 : 1;
}

