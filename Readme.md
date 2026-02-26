# Generated results
## LNL
```
[INFO] Starting file write of 2 GiB pattern...
[INFO] File write complete.
[MEM] working_set_mb=2054.14 private_mb=2053.21
[INFO] Mapped file created (full 2 GiB view).
[MEM] working_set_mb=2054.2 private_mb=2057.23
[INFO] Touching the entire mapped file to commit pages...
[MEM] working_set_mb=4102.2 private_mb=2057.23
[INFO] OpenCL context and queue created.
[MEM] working_set_mb=4281.83 private_mb=2235.66
[INFO] OpenCL kernel built -- copy value from src to dst.
[MEM] working_set_mb=4282.38 private_mb=2235.71
[INFO] RUM BUFFER mode.
[MEM] before cl buffer creation working_set_mb=4282.38 private_mb=2235.71
[INFO] CL src buffer created from mmapped file
[MEM] working_set_mb=4282.39 private_mb=2235.71
[INFO] CL dst buffer created
[MEM] working_set_mb=4282.41 private_mb=4287.73
[INFO] Mutated mapped file after buffer creation at offset 17
[MEM] working_set_mb=4282.41 private_mb=4287.73
[INFO] Kernel Execution Done
[MEM] working_set_mb=6338.14 private_mb=4295.17
[INFO] Readback to host completed.
[MEM] working_set_mb=8390.58 private_mb=6351.56
[RESULT] Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[RESULT] Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18 19 20 21 22 23 24 25 26 27 28 29 30 31
[MEM] exit buffer mode function working_set_mb=4294.6 private_mb=2247.54
[RESULT] Buffer mode PASS
[INFO] RUM USM mode.
[MEM] before usm buffer creation working_set_mb=4294.61 private_mb=2247.54
[USM] MapViewOfFileEx failed, GetLastError=487
[MEM] exit usm mode function working_set_mb=6374.66 private_mb=4331.61
[RESULT] USM mode FAIL
```


## Build command
`cmake -S . -B build -DOpenCL_INCLUDE_DIR="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2025.07.23-Win-x64/include" -DOpenCL_LIBRARY="C:/Users/yarudu/Documents/project/third-party/OpenCL-SDK-v2025.07.23-Win-x64/lib/OpenCL.lib"`


## OCL-intercept flag to dump driver information
```
set CLI_ContextCallbackLogging=1
set CLI_ContextHintLevel=7
```
