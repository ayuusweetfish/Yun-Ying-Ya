.p2align 2
// 16 13 8 5 0
.option rvc
.LA1: nop
.LA2: nop
.LA3: nop
.LA4: nop
.LA5: j 0f
      nop


.p2align 2
// 19 16 11 8 3 0
.option rvc
.LB1: nop
.LB2: nop
.LB3: nop
.LB4: nop
.LB5: nop
.LB6: j 0f


.p2align 2
// 20 15 10 5 0
.option norvc
.LC1: nop
.LC2: nop
.LC3: nop
.LC4: nop
.LC5: j 0f


.p2align 2
// 17 14 9 6 0
.option rvc
.LD1: nop
.LD2: nop
.LD3: nop
.option norvc
.LD4: nop
.option rvc
.LD5: j 0f


.p2align 2
/*
// 21 18 12 6 0
.option rvc
.LE1: nop
.option norvc
.LE2: nop
.LE3: nop
.LE4: nop
.option rvc
.LE5: j 0f
*/
/*
// 0 5 10 15 20 25
.option norvc
.LE1: j 0f
.LE2: j .LE1
.LE3: j .LE2
.LE4: j .LE3
.LE5: j .LE4
.option rvc
*/
/*
// 36 27 18 9 0
.option rvc
.LE1: lb zero, 0(zero)
.LE2: lb zero, 0(zero)
.LE3: lb zero, 0(zero)
.LE4: lb zero, 0(zero)
.LE5: j 0f
*/
/*
// 32 25 16 9 0
.option rvc
.LE1: lw a1, 0(a0)
.LE2: lw a1, 0(a0)
.LE3: lw a1, 0(a0)
.LE4: lw a1, 0(a0)
.LE5: j 0f
*/
// 7 25 16 9 0
.option rvc
.LE1: lw a1, 0(a0)
      j 0f
      nop
.LE2: lw a1, 0(a0)
.LE3: lw a1, 0(a0)
.LE4: lw a1, 0(a0)
.LE5: j 0f


.p2align 2
/*
// 20 15 10 6 1
.LF1: j .LF2
.LF2: j .LF3
.LF3: j .LF4
.LF4: j .LF5
.LF5: j 0f
*/
// 25 20 15 10 5 0
.option rvc
.LF6: j 0f
.LF5: j .LF6
.LF4: j .LF5
.LF3: j .LF4
.LF2: j .LF3
.LF1: j .LF2


.p2align 2
      nop
      nop

0:
      // rdcycle t0
      ret

.p2align 2
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
  .word .LF6
