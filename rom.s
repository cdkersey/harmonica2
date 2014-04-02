/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
entry:  jali %r5, counttoten
        jali %r5, counttoten
        jali %r5, counttoten
        jali %r5, counttoten

finish: jmpi finish

counttoten: ldi %r0, #0
loop:       addi %r0, #1
            subi %r1, %r0, #10
            rtop @p0, %r1
      @p0 ? jmpi loop

            jmpr %r5
