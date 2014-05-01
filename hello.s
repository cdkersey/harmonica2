/* Simple example. */

.perm rwx
.entry
.global
entry: /* We assume that RAM does not contain our string, so we load it in
          manually. */
       ldi %r2, hello

       ldi %r1, #0x72
       ldi %r0, #0x61
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x48
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x22
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #0

       ldi %r1, #0x20
       ldi %r0, #0x22
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x21
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x70
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #4

       ldi %r1, #0x68
       ldi %r0, #0x20
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x73
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x69
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #8

       ldi %r1, #0x61
       ldi %r0, #0x20
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x77
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x6f
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #12
        
       ldi %r1, #0x72
       ldi %r0, #0x61
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x68
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x20
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #16
        
       ldi %r1, #0x65
       ldi %r0, #0x73
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x20
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x70
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #20

       ldi %r1, #0x73
       ldi %r0, #0x20
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x6c
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x61
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #24

       ldi %r1, #0x20
       ldi %r0, #0x73
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x79
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x61
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #28

       ldi %r1, #0x6c
       ldi %r0, #0x6c
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x65
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x68
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #32

       ldi %r1, #0x00
       ldi %r0, #0x0a
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x21
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       ldi %r0, #0x6f
       shli %r1, %r1, #8
       or %r1, %r1, %r0
       st %r1, %r2, #36
        
       ldi %r7, hello
       jali %r5, puts

finish: jmpi finish;

hello:
.byte 0x22
.string "Harp!\" is how a harp seal says hello!\n"
