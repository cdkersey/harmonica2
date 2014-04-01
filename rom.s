/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
entry: ldi %r0, #1
       ldi %r1, #0
       rtop @p0, %r0
       rtop @p1, %r0
       andp @p2, @p0, @p1
       iszero @p3, %r1 
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
