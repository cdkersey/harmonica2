/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
entry:  ori %r4, %r0, #0
        jali %r5, counttotenandstore
        jali %r5, counttotenandload

finish: jmpi finish

counttotenandstore: shli %r0, %r4, #3
loop1:              shli %r1, %r0, #2
                    st %r0, %r1, array
                    addi %r0, #1
                    subi %r1, %r0, #9
                    rtop @p0, %r1
              @p0 ? jmpi loop1

                    jmpr %r5

counttotenandload: shli %r0, %r4, #3
                   ldi %r3, #0
loop2:             shli %r1, %r0, #2
                   ld %r2, %r1, array
                   add %r3, %r3, %r2
                   addi %r0, #1
                   subi %r1, %r0, #8
                   rtop @p0, %r1
             @p0 ? jmpi loop2

                   jmpi %r5

array: .space 10

