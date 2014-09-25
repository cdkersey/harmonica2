/*******************************************************************************
 Harptools by Chad D. Kersey, Summer 2011
********************************************************************************

 Sample HARP assmebly program.

*******************************************************************************/
/* Divergent branch: test immediate postdominator branch divergence support. */
.def THREADS 4

.align 4096
.perm rwx
.entry
.global
entry:
       ldi %r0, #1                               /*  0 */
       ldi %r1, THREADS                          /*  4 */
sloop: clone %r0                                 /*  8 */
       
       addi %r0, %r0, #1                         /*  c */
       sub %r2, %r1, %r0                         /* 10 */
       rtop @p0, %r2                             /* 14 */
 @p0 ? jmpi sloop                                /* 18 */

       ldi %r0, #0                               /* 1c */
       jalis %r5, %r1, dthread;                  /* 20 */
	
       ldi %r0, #0                               /* 24 */
       ldi %r1, (__WORD * THREADS)               /* 28 */

ploop: ld %r7, %r0, array                        /* 2c */
       jali %r5, printdec                        /* 30 */
       
       addi %r0, %r0, __WORD                     /* 34 */
       sub %r7, %r1, %r0                         /* 38 */
       rtop @p0, %r7                             /* 3c */
 @p0 ? jmpi ploop                                /* 40 */

       trap;                                     /* 44 */


dthread: ldi %r1, #10                            /* 48 */
         ldi %r2, #0                             /* 4c */

loop:    andi %r3, %r0, #1                       /* 50 */
         rtop @p1, %r3                           /* 54 */
   @p1 ? split                                   /* 58 */
   @p1 ? jmpi else                               /* 5c */
         add %r2, %r2, %r0                       /* 60 */
         jmpi after                              /* 64 */
else:    sub %r2, %r2, %r0                       /* 68 */
after:   join                                    /* 6c */

         subi %r1, %r1, #1                       /* 70 */
         rtop @p0, %r1                           /* 74 */
   @p0 ? jmpi loop                               /* 78 */

         shli %r4, %r0, (`__WORD)                /* 7c */
         st %r2, %r4, array                      /* 80 */

         jmprt %r5;                              /* 84 */

array: .space 64
