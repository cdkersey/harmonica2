/* Clone test. Find powers of 1, 2, 3, and 4 in 4 lanes. */

.perm rwx
.entry
.global
entry:
       ldi %r0, #1; /* 0 */
       ldi %r1, #2; /* 4 */
       ldi %r3, #1; /* 8 */
       clone  %r3;  /* 12 */

       ldi %r0, #1; /* 16 */
       ldi %r1, #3; /* 20 */
       ldi %r3, #2; /* 24 */
       clone  %r3;  /* 28 */

       ldi %r0, #1; /* 32 */
       ldi %r1, #4; /* 36 */
       ldi %r3, #3; /* 40 */
       clone  %r3;  /* 44 */

       ldi %r0, #1; /* 48 */
       ldi %r1, #1; /* 52 */
       ldi %r3, #4; /* 56 */
       jalis %r5, %r3, lanemain; /* 60 */

finish: subi %r0, %r0, #1; /* 64 */
        jmpi finish;       /* 68 */

lanemain: ldi %r4, #10;        /* 72 */

loop:     subi %r4, %r4, #1;   /* 76 */
          mul %r0, %r0, %r1;   /* 80 */
          rtop @p0, %r4;       /* 84 */
    @p0 ? jmpi loop            /* 88 */

          jmprt %r5;           /* 92 */
