// asmsyntax=asm

// GNU assembler does not support conveniently interleaving
// uncompressed and compressed instructions; custom macros to the rescue
.macro C mnemonic, args:vararg
  .option push
  .option rvc
  \mnemonic \args
  .option pop
.endm
.macro F mnemonic, args:vararg
  .option push
  .option norvc
  \mnemonic \args
  .option pop
.endm

.p2align 2
.L25: F nop
.L20: F nop
.L15: F nop
.L10: F nop
.L05: F nop
      F ret


.p2align 2
.L24: F nop
.L19: F nop
.L14: F nop
.L09: C nop
.L06: F nop
      C ret


.p2align 2
.L23: F nop
.L18: F nop
.L13: F nop
.L08: F nop
.L03: C nop
      C ret


.p2align 2
.L22: F nop
.L17: F nop
.L12: F nop
.L07: C lw a0, 0(a0)  // Assumes a0 is a valid memory location
      C ret


.p2align 2
.L21: F nop
.L16: F nop
.L11: C nop
      C nop   // 8
      C nop   // 3
      C ret


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
