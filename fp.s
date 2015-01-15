/*******************************************************************************
 Harptools by Chad D. Kersey, Summer 2011
********************************************************************************

 Sample HARP assmebly program.

*******************************************************************************/
/* */
.def THREADS 4
.def WARPS 8
.def LOOPS 64
    
.align 4096
.perm rwx
.entry
.global

entry: ldi %r0, wmain
       ldi %r1, #1

wsloop: wspawn %r0, %r0, %r1

        addi %r1, %r1, #1
        subi %r2, %r1, WARPS
        rtop @p0, %r2
  @p0 ? jmpi wsloop

       ldi %r0, #0
	
wmain: ori %r7, %r0, #0
       ldi %r0, #1
       ldi %r1, THREADS
sloop: clone %r0                               
       
       addi %r0, %r0, #1                       
       sub %r2, %r1, %r0                       
       rtop @p0, %r2                           
 @p0 ? jmpi sloop                              

       ldi %r0, #0                             
       jalis %r5, %r1, dthread;                

finish: jmpi finish;
    /*	
       ldi %r0, #0                             
       ldi %r1, (__WORD * THREADS)             

ploop: ld %r7, %r0, array                      
       jali %r5, printdec                      
       
       addi %r0, %r0, __WORD                  
       sub %r7, %r1, %r0                      
       rtop @p0, %r7                          
 @p0 ? jmpi ploop                             

       trap;  */


dthread: ldi %r2, WARPS
	 bar %r1, %r2

         ldi %r3, THREADS
         mul %r7, %r7, %r2
         mul %r7, %r7, %r3
         add %r7, %r7, %r0

         ori %r1, %r7, #0
         ldi %r2, #0
         ldi %r4, #0
iloop:   itof %r3, %r2
         fadd %r4, %r4, %r3
         st %r3, %r1, array

         addi %r1, %r1, THREADS
         addi %r2, %r2, #1
         subi %r3, %r2, LOOPS
         rtop @p0, %r3
   @p0 ? jmpi iloop
    
         jmprt %r5;

array: .space 1024
