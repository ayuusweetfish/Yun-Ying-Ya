.p2align 2
.option rvc
.LA1: nop
.LA2: nop
.LA3: nop
.LA4: nop
.LA5: j 0f
      nop


.p2align 2
.option rvc
.LB1: nop
.LB2: nop
.LB3: nop
.LB4: nop
.LB5: nop
.LB6: j 0f


.p2align 2
.option norvc
.LC1: nop
.LC2: nop
.LC3: nop
.LC4: nop
.LC5: j 0f


.p2align 2
.option rvc
.LD1: nop
.LD2: nop
.LD3: nop
.option norvc
.LD4: nop
.option rvc
.LD5: j 0f


.p2align 2
.option rvc
.LE1: nop
.option norvc
.LE2: nop
.LE3: nop
.LE4: nop
.option rvc
.LE5: j 0f


.p2align 2
.LF1: j .LF2
.LF2: j .LF3
.LF3: j .LF4
.LF4: j .LF5
.LF5: j 0f


      nop
      nop


0:
      // rdcycle t0
      ret

.global sled
sled:
  .word .LA1
  .word .LA2
  .word .LA3
  .word .LA4
  .word .LA5
  .word .LB1
  .word .LB2
  .word .LB3
  .word .LB4
  .word .LB5
  .word .LB6
  .word .LC1
  .word .LC2
  .word .LC3
  .word .LC4
  .word .LC5
  .word .LD1
  .word .LD2
  .word .LD3
  .word .LD4
  .word .LD5
  .word .LE1
  .word .LE2
  .word .LE3
  .word .LE4
  .word .LE5
  .word .LF1
  .word .LF2
  .word .LF3
  .word .LF4
  .word .LF5

/*
.p2align 2  // 4-byte boundary
.option norvc
.L25: nop
.L20: nop
.L15: nop
.L10: nop
.option rvc
.L05:
      j 0f
      nop

.L23: nop
.L23: nop
.L18: nop
.L13: j 0f

      nop
      nop
      nop
      nop

0:
.p2align 2
      rdcycle t0

.global sled
sled:
  .word .L05
  .word .L06
  .word .L07
  .word .L08
  .word .L09
  .word .L10
  .word .L11
  .word .L12
  .word .L13
  .word .L14
  .word .L15
  .word .L16
  .word .L17
  .word .L18
  .word .L19
  .word .L20
  .word .L21
  .word .L22
  .word .L23
  .word .L24
  .word .L25
// print('\n'.join('  .word .L%02d' % i for i in range(5, 26)))
*/
