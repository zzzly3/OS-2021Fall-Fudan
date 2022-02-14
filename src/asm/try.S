.global MmProbeRead // x0: addr
MmProbeRead:
and x1, x0, #-8 // make addr 8-aligned
mov x0, #0
ldr x2, [x1]
mov x0, #1
.global MmiProbeReadCatch
MmiProbeReadCatch:
ret