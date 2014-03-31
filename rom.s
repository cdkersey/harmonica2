/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
entry: ldi %r0, #0xfeed
       ldi %r0, #0xfeed
       ldi %r1, #1
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
       add %r1, %r0, %r1
       add %r1, %r1, %r0
