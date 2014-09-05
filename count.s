/* sieve of eratosthanes: Find some primes. */
.def SIZE 32

.perm rwx
.entry
.global
entry:       ldi  %r0, #0x00;
	     ldi %r1, #256;
	     ldi %r7, #1;
	     shli %r7, %r7, (__WORD*8 - 1);
	
loop1:       not %r3, %r0
             st %r3, %r7, #0;

             jali %r5, delay;

             ldi %r3, #1
             ldi %r4, #7
loop2:       neg %r3, %r3
	     st %r3, %r7, #0;
	     neg %r3, %r3
             jali %r5, delay;
             shli %r3, %r3, #1;
             subi %r4, %r4, #1;
             rtop @p0, %r4;
       @p0 ? jmpi loop2;

             ldi %r4, #7
loop3:       neg %r3, %r3
	     st %r3, %r7, #0;
	     neg %r3, %r3
             jali %r5, delay;
             shri %r3, %r3, #1;
             subi %r4, %r4, #1;
             rtop @p0, %r4;
       @p0 ? jmpi loop3;
	
	     subi %r1, %r1, #1;
	     addi %r0, %r0, #1;
	     rtop @p0, %r1;
       @p0 ? jmpi loop1;

finished:    jmpi  finished;

delay:   ldi %r2, #1;
         shli %r2, %r2, #20;
loopd:   subi %r2, %r2, #1;
         rtop @p0, %r2;
   @p0 ? jmpi loopd;

         jmpr %r5;
