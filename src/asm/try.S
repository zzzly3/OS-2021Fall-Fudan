.global MmProbeRead // x0: addr
MmProbeRead:
mov x1, x0
mov x0, #0
ldr x2, [x1]
mov x0, #1
.global MmiProbeReadCatch
MmiProbeReadCatch:
ret