#include "isr_handler.h"
#include "serial.h"
#include "scheduler.h"
#include "timer.h"
#include "irq.h"
#include "kbd_buf.h"

static void page_fault_handler(uint64_t error_code, uint64_t addr) {
    serial_printf("[PF] Page fault at %x, error %x, killing thread\n", addr, error_code);
    thread_exit();
}

static uint8_t scancode_to_ascii(uint8_t sc) {
    static const char tbl[128] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s',
        'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c',
        'v','b','n','m',',','.','/',0,0,0,' ',0
    };
    if (sc < 128) return tbl[sc];
    return 0;
}

void isr_handler(struct regs *r) {
    if (r->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        page_fault_handler(r->err_code, cr2);
        return;
    }
    if (r->int_no == 32) {
        timer_tick();
        schedule();
    }
    if (r->int_no == 33) {
        uint8_t st;
        do { __asm__ volatile ("inb $0x64, %0" : "=a"(st)); } while (!(st & 1));
        uint8_t sc;
        __asm__ volatile ("inb $0x60, %0" : "=a"(sc));
        if (!(sc & 0x80)) {
            char ch = scancode_to_ascii(sc);
            if (ch) kbd_buf_put(ch);
        }
        __asm__ volatile ("outb %%al, $0x20" : : "a"(0x20));
        return;
    }
    if (r->int_no >= 32 && r->int_no <= 47) {
        if (r->int_no >= 40) __asm__ volatile ("outb %%al, $0xA0" : : "a"(0x20));
        __asm__ volatile ("outb %%al, $0x20" : : "a"(0x20));
    }
}