/* sieve of eratosthanes: Find some primes. */
.def SIZE 32

.perm rwx
.entry
.global
entry:    ldi %r0, warpmain
          ldi %r1, #1234
          wspawn %r0, %r1

          ldi %r1, #2345
          wspawn %r0, %r1

finished: jmpi  finished

warpmain: ldi %r1, #10
warploop: addi %r0, %r0, #1
          subi %r1, %r1, #1
          rtop @p0, %r1
    @p0 ? jmpi warploop
warpdone: jmpi  warpdone
