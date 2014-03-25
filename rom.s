/* Example program: sum numbers 0 through 100 on thread 0, 1 through 101 on
   thread 1, etc.*/

.perm x
.entry
entry: ldi %r0, #1
       clone %r0
       clonew %r0
       ldi %r0, #2
       clone %r0
       clonew %r0
       ldi %r0, #3
       clone %r0
       clonew %r0
       ldi %r0, #4
       clone %r0
       ldi %r0, #5
       clone %r0
       ldi %r0, #0
       ldi %r1, #5
       jalis %r5, %r1, kernel

finish: jmpi finish

kernel: mul %r0, %r0, %r0
	mul %r0, %r0, %r0
        addi %r1, %r0, #17
        st %r1, %r0, #0
        addi %r1, %r1, #123
        st %r1, %r0, #1
        ld %r0, %r0, #0
	mul %r0, %r0, %r0
        ld %r0, %r0, #0
	jmprt %r5
