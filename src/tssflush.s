.section .text
.global tss_flush
.type tss_flush, @function
tss_flush:
    mov $0x18, %ax   /* TSS selector */
    ltr %ax
    ret