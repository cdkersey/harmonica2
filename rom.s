/* sieve of eratosthanes: Find some primes. */
.def SIZE 32

.align 4096
.perm x
.entry
.global
entry:       ldi  %r0, #0;
             st   %r0, %r0, array;
             addi %r1, %r0, __WORD;
             st   %r0, %r1, array;
             ldi  %r0, #2; /* i = 2 */
loop1:       shli %r1, %r0, (`__WORD);
             st   %r0, %r1, array;
             addi %r0, %r0, #1;
             subi %r1, %r0, SIZE;
             rtop @p0, %r1;
       @p0 ? jmpi loop1;

             ldi  %r0, #1;    
loop2:       addi %r0, %r0, #1;
             shli %r1, %r0, (`__WORD);
             ld   %r1, %r1, array;
             iszero @p0, %r1;
       @p0 ? jmpi loop2;

             mul   %r2, %r1, %r1;
             subi  %r3, %r2, SIZE;
             neg   %r3, %r3
             isneg @p0, %r3;
       @p0 ? jmpi  end;

             ldi   %r3, #0;
loop3:       shli  %r4, %r2, (`__WORD);
             st    %r3, %r4, array;
             add   %r2, %r2, %r1;
             ldi   %r4, SIZE;
             sub   %r4, %r2, %r4;
             isneg @p0, %r4;
             notp  @p0, @p0;
       @p0 ? jmpi  loop2;
             jmpi  loop3;

end:         ldi   %r0, #0;
printloop:   ld    %r7, %r0, array;
             subi  %r2, %r0, (__WORD*SIZE);
             rtop  @p0, %r7;
             rtop  @p1, %r2;
             addi  %r0, %r0, __WORD;
             andp  @p2, @p0, @p1
       /*@p2 ? jali %r5, printdec;*/
       @p1 ? jmpi printloop;

finished:    jmpi  finished; /*Instruction 41, PC=164*/

printdec:    ldi  %r3, #1; /*Instruction 42, PC=168*/
             shli %r3, %r3, (__WORD*8 - 1);
             and  %r6, %r3, %r7;
             rtop @p0, %r6;
       @p0 ? ldi  %r6, #0x2d;
       @p0 ? st   %r6, %r3, #0;
       @p0 ? neg  %r7, %r7;
             ldi  %r4, #0;
printdec_l1: modi %r6, %r7, #10;
             divi %r7, %r7, #10;
             addi %r6, %r6, #0x30;
             st   %r6, %r4, digstack;
             addi %r4, %r4, __WORD;
             rtop @p0, %r7;
       @p0 ? jmpi printdec_l1;
printdec_l2: subi %r4, %r4, __WORD;
             ld   %r6, %r4, digstack;
             st   %r6, %r3, #0;
             rtop @p0, %r4;
       @p0 ? jmpi printdec_l2;
             ldi  %r6, #0x0a;
             st   %r6, %r3, #0;
             jmpr %r5 /* Instruction 64, PC=256*/

.perm rw
.global
.align 1024
array:       .space 100 /* SIZE words of space. */
digstack:    .space 10
