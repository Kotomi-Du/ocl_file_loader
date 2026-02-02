cmake_minimum_required(VERSION 3.20)
project(ocl_mmap_test LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCL REQUIRED)

add_executable(ocl_mmap_test test_opencl_mmap.cpp)
target_link_libraries(ocl_mmap_test PRIVATE OpenCL::OpenCL)

if (WIN32)
	target_link_libraries(ocl_mmap_test PRIVATE Psapi)
endif()