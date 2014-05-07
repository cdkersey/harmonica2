/* sieve of eratosthanes: Find some primes. */
.def SIZE 32

.perm rwx
.entry
.global
entry:    ldi %r0, warpmain
          ldi %r1, #1234
          wspawn %r1, %r0

          ldi %r1, #2345
          wspawn %r1, %r0

          ldi %r1, #3456
          wspawn %r1, %r0

finished: jmpi  finished

warpmain:  ldi %r2, #10
           muli %r3, %r1, #40
storeloop: addi %r0, %r0, #1
           st %r0, %r3, #0
           addi %r3, %r3, #4
           subi %r2, %r2, #1
           rtop @p0, %r2
     @p0 ? jmpi storeloop

           muli %r3, %r1, #40
           ldi %r2, #10
           ldi %r0, #1
loadloop:  ld %r4, %r3, #0
           subi %r2, %r2, #1
           addi %r3, %r3, #4
           mul %r0, %r0, %r4
           rtop @p0, %r2
     @p0 ? jmpi loadloop

warpdone:  jmpi  warpdone
