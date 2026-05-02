#ifndef ISR_HANDLER_H
#define ISR_HANDLER_H
#include <stdint.h>

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void isr_handler(struct regs *r);
#endif