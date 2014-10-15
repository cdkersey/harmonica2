/*******************************************************************************
 Harptools by Chad D. Kersey, Summer 2011
********************************************************************************

 Sample HARP assmebly program.

*******************************************************************************/
/* Divergent branch: test immediate postdominator branch divergence support. */
.def THREADS 4
.def WARPS 5

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


dthread: ldi %r1, #1234
	 ldi %r2, WARPS
	 bar %r1, %r2
	 ldi %r1, #10
         ldi %r2, #0
         muli %r7, %r7, #0x100
	 add %r0, %r0, %r7

loop:    andi %r3, %r0, #1                    
         rtop @p1, %r3                        
   @p1 ? split
   @p1 ? jmpi else
         add %r2, %r2, %r0
         jmpi after                           
else:    sub %r2, %r2, %r0
after:   join

         subi %r1, %r1, #1
         rtop @p0, %r1
   @p0 ? jmpi loop

         shli %r4, %r0, (`__WORD)
         st %r2, %r4, array

         jmprt %r5;

array: .space 64
