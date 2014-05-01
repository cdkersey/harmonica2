/* sieve of eratosthanes: Find some primes. */
.def SIZE 32

.perm rwx
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

             /* mul   %r2, %r1, %r1; */
             add   %r2, %r1, %r1; /* This is, of course, idiotic. */
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
       @p2 ? jali %r5, printdec;
       @p1 ? jmpi printloop;

finished:    jmpi  finished; /*Instruction 41, PC=164*/

array:       .space 100 /* SIZE words of space. */
