/* Harp example program 1: Sum 1-100, first writing the numbers to RAM */

.perm rwx
.entry
entry:  ori %r4, %r0, #0

        shli %r0, %r4, (`(__WORD * 4))
        ldi  %r1, #4
        shli  %r3, %r4, #16
    
iloop:  st %r3, %r0, array
        addi %r3, %r3, #1
        addi %r0, %r0, __WORD
        subi %r1, %r1, #1
        rtop @p0, %r1
  @p0 ? jmpi iloop
    
sloop:  ld %r2, %r0, array
        subi %r1, %r1, #1
        addi %r0, %r0, __WORD
        add %r3, %r3, %r2
        rtop @p0, %r1
  @p0 ? jmpi sloop
        
finish: jmpi finish

.global
.perm rw
array: .space 16