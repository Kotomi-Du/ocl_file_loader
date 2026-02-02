# Generated results
## LNL
```
Mapped address: 00000210BE7A0000
OpenCL reported host ptr: 00000210BE7A0000
Host ptr matches mapped? yes
Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18
Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18
PASS: GPU saw the correct data from mmap-backed buffer
```
## BMG
```
Mapped address: 000001ECBBDE0000
OpenCL reported host ptr: 000001ECBBDE0000
Host ptr matches mapped? yes
Expected bytes: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 171 18
Output bytes:   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18
FAIL: Data mismatch
```