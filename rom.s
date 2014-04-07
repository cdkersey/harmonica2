/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
main: shli %r1, %r0, #5;
      st %r0, %r1, #0;
      addi %r1, %r1, __WORD;
      st %r0, %r1, #0;
      addi %r1, %r1, __WORD;
      st %r0, %r1, #0;
      addi %r1, %r1, __WORD;
      st %r0, %r1, #0;

      ldi %r2, #0;
      ld %r0, %r1, #0;
      add %r2, %r2, %r0;
      subi %r1, %r1, __WORD;
      ld %r0, %r1, #0;
      add %r2, %r2, %r0;
      subi %r1, %r1, __WORD;
      ld %r0, %r1, #0;
      add %r2, %r2, %r0;
      subi %r1, %r1, __WORD;
      ld %r0, %r1, #0;
      add %r2, %r2, %r0;

finish: jmpi finish;

array: .space 10

