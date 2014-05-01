/* Library functions for Harmonica sample programs */

/* printdec: print signed decimal number */
.perm rwx
.global
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

digstack:    .space 10

/* puts: print string */
.global
puts:        ldi %r4, #1;
             shli %r4, %r4, (__WORD*8 - 1);

puts_l:      ld   %r6, %r7, #0;
             andi %r6, %r6, #0xff;
             rtop @p0, %r6;
             notp @p0, @p0;
       @p0 ? jmpi puts_end;
             st %r6, %r4, #0;
             addi %r7, %r7, #1;
             jmpi puts_l;
puts_end:    jmpr %r5