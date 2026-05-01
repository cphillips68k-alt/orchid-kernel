.section .text
.global tss_flush
.type tss_flush, @function
tss_flush:
    mov $0x28, %ax   /* TSS selector = 5 * 8 = 0x28 */
    ltr %ax
    ret