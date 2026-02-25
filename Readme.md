# Generated results
## LNL
```
[INFO] Starting file write of 2 GiB pattern...
[INFO] File write complete.
[INFO] File mapped into memory (full 2 GiB view).
[INFO] Touching the entire mapped file to commit pages...
[MEM] working_set_mb=4101.19 private_mb=2056.85
[INFO] OpenCL context and queue created.
[MEM] working_set_mb=4281.64 private_mb=2235.45
=======> Context Callback (private_info = 0000004EC2DBEDD0, cb = 8):
Performance hint: clCreateBuffer with pointer 000001C7F2560000 and size 2147483648 meets alignment restrictions and buffer will share the same physical memory with CPU.
<======= End of Context Callback
=======> Context Callback (private_info = 0000004EC2DBE8A8, cb = 8):
Performance hint: Buffer 000001C8774E3770 will not use compressed memory.
<======= End of Context Callback
=======> Context Callback (private_info = 0000004EC2DBEDD0, cb = 8):
Performance hint: clCreateBuffer needs to allocate memory for buffer. For subsequent operations the buffer will share the same physical memory with CPU.
<======= End of Context Callback
=======> Context Callback (private_info = 0000004EC2DBE8A8, cb = 8):
Performance hint: Buffer 000001C772334AC0 will use compressed memory.
<======= End of Context Callback 
[INFO] OpenCL buffers created (src from map, dst device-only).
[MEM] working_set_mb=4281.66 private_mb=4287.46
[INFO] Kernel arguments set.
[INFO] Kernel enqueued with global size = 2147483648
[INFO] Kernel execution finished.
[MEM] working_set_mb=6423.5 private_mb=4322.59  ##  iGPU memory increased for output buffer
[INFO] Readback to host completed.
[MEM] working_set_mb=8475.68 private_mb=6378.71
[RESULT] Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] PASS: GPU saw the correct data from mmap-backed buffer
```
## BMG
```
[INFO] Starting file write of 2 GiB pattern...
[INFO] File write complete.
[INFO] File mapped into memory (full 2 GiB view).
[INFO] Touching the entire mapped file to commit pages...
[MEM] working_set_mb=4103.02 private_mb=2059.36
[INFO] OpenCL context and queue created.
[MEM] working_set_mb=4174.95 private_mb=2259.89
[INFO] OpenCL buffers created (src from map, dst device-only).
[MEM] working_set_mb=4175.04 private_mb=4311.97
[INFO] Kernel arguments set.
[INFO] Kernel enqueued with global size = 2147483648
[INFO] Kernel execution finished.
[MEM] working_set_mb=4176.05 private_mb=4314.87  ##  dGPU memory increased for output buffer
[INFO] Readback to host completed.
[MEM] working_set_mb=6228.32 private_mb=6371.86
[RESULT] Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] PASS: GPU saw the correct data from mmap-backed buffer
```


## Build command
`cmake -S . -B build -DOpenCL_INCLUDE_DIR="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2025.07.23-Win-x64/include" -DOpenCL_LIBRARY="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2025.07.23-Win-x64/lib/OpenCL.lib"`


## OCL-intercept flag to dump driver information
```
set CLI_ContextCallbackLogging=1
set CLI_ContextHintLevel=7
```
