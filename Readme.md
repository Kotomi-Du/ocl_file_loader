# Generated results
## LNL
```
[INFO] Starting file write of 2 GiB pattern...
[INFO] File write complete.
[INFO] File mapped into memory (full 2 GiB view).
[INFO] OpenCL context and queue created.
[MEM] working_set_mb=2273.51 private_mb=2276.51
[INFO] OpenCL buffers created (src from map, dst device-only).
[MEM] working_set_mb=4321.64 private_mb=4328.58
[INFO] Kernel arguments set.
[INFO] Kernel enqueued with global size = 2147483648
[INFO] Kernel execution finished.
[MEM] working_set_mb=6377.85 private_mb=4336.02
[INFO] Readback to host completed.
[MEM] working_set_mb=8430.29 private_mb=6392.41
[RESULT] Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] PASS: GPU saw the correct data from mmap-backed buffer
```
## BMG
```
[INFO] Starting file write of 2 GiB pattern...
[INFO] File write complete.
[INFO] File mapped into memory (full 2 GiB view).
[INFO] OpenCL context and queue created.
[MEM] working_set_mb=2126.11 private_mb=2257.68
[INFO] OpenCL buffers created (src from map, dst device-only).
[MEM] working_set_mb=4174.32 private_mb=6361.8
[INFO] Kernel arguments set.
[INFO] Kernel enqueued with global size = 2147483648
[INFO] Kernel execution finished.
[MEM] working_set_mb=4175.31 private_mb=6365.51
[INFO] Readback to host completed.
[MEM] working_set_mb=6227.57 private_mb=8423.18
[RESULT] Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] FAIL: Data mismatch
```


## Build command
`cmake -S . -B build -DOpenCL_INCLUDE_DIR="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2024.05.08-Win-x64/include" -DOpenCL_LIBRARY="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2024.05.08-Win-x64/lib/OpenCL.lib"`