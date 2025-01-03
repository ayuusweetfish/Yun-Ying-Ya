.p2align 2
.option norvc
.L25: nop
.L20: nop
.L15: nop
.L10: nop
.L05: nop
      ret


.p2align 2
.option norvc
.L24: nop
.L19: nop
.L14: nop
.option rvc
.L09: nop
.option norvc
.L06: nop
.option rvc
      ret


.p2align 2
.option norvc
.L23: nop
.L18: nop
.L13: nop
.L08: nop
.option rvc
.L03: nop
      ret


.p2align 2
.option norvc
.L22: nop
.L17: nop
.L12: nop
.option rvc
.L07: lw a0, 0(a0)  // Assumes a0 is a valid memory location
      ret


.p2align 2
.option norvc
.L21: nop
.L16: nop
.option rvc
.L11: nop
      nop   // 8
      nop   // 3
      ret


.p2align 2
.global sled
sled:
  .word .L25
  .word .L24
  .word .L23
  .word .L22
  .word .L21
  .word .L20
  .word .L19
  .word .L18
  .word .L17
  .word .L16
  .word .L15
  .word .L14
  .word .L13
  .word .L12
  .word .L11
  .word .L10
  .word .L09
  .word .L08
  .word .L07
  .word .L06
  .word .L05
// print('\n'.join('  .word .L%02d' % i for i in range(25, 4, -1)))
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
  .word .L05
