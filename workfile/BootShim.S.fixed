_Head:
  adr   x1, _Payload    /* Set _Entry address */
  b     _Start          /* Jump to the real code */

.equ STAGE_ADDR, FD_BASE + FD_SIZE + 0x70000

#if REQUIRES_KERNEL_HEADER == 1
_StackBase:
  .quad FD_BASE         /* Text Offset */

_StackSize:
  .quad FD_SIZE + (_Payload - _Head) /* Complete shim and payload */
  .quad 0               /* Flags */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .ascii "ARM\x64"      /* ARM64 Magic */
  .long 0               /* Reserved */
#endif

_Start:
  mov   x4, x1
  ldr   x5, =FD_BASE
  cmp   x4, x5
  beq   _Entry

  /* Relocate the copy/clean block out of the FD destination range. */
  adr   x2, _CopyLoop
  ldr   x3, =STAGE_ADDR
  ldr   x6, =(_RelocEnd - _CopyLoop)
  mov   x9, x6
_RelocLoop:
  ldp   x10, x11, [x2], #16
  stp   x10, x11, [x3], #16
  subs  x9, x9, #16
  b.ne  _RelocLoop

  /* Publish the relocated instructions, then enter the copy loop. */
  ldr   x8, =STAGE_ADDR
  ldr   x7, =(_RelocEnd - _CopyLoop)
  add   x7, x8, x7
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  sub   x10, x9, #1
  bic   x8, x8, x10
_RelocCleanLoop:
  dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  _RelocCleanLoop
  dsb   sy
  ic    iallu
  dsb   sy
  isb

  ldr   x7, =STAGE_ADDR
  mov   x4, x1
  ldr   x5, =FD_BASE
  ldr   x6, =FD_SIZE
  br    x7

.balign 16
_CopyLoop:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyLoop
  ldr   x5, =FD_BASE

_Entry:
  /* Publish the copied instructions before executing them. */
  dsb   sy
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  sub   x10, x9, #1
  bic   x8, x5, x10
  ldr   x7, =FD_SIZE
  add   x7, x5, x7

_CleanLoop:
  dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  _CleanLoop
  dsb   sy
  ic    iallu
  dsb   sy
  isb
  br    x5

_Dead:
  b     _Dead

.text
.ltorg
.balign 16
_RelocEnd:
.align 4

_Payload:
