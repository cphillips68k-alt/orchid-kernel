#include "serial.h"
#include <stdarg.h>

#define COM1 0x3F8

/* All port I/O is done with inline assembly so we have zero dependencies.
   Even something as simple as outb() is often pulled from a library.
   We don't need a library for one CPU instruction. */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    /* Disable all interrupts on the UART */
    outb(COM1 + 1, 0x00);

    /* Set baud rate divisor to 1 -> 115200 baud */
    outb(COM1 + 3, 0x80);    /* Enable DLAB */
    outb(COM1 + 0, 0x01);    /* Divisor low byte */
    outb(COM1 + 1, 0x00);    /* Divisor high byte */

    /* 8 bits, no parity, one stop bit */
    outb(COM1 + 3, 0x03);

    /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1 + 2, 0xC7);

    /* IRQs enabled, RTS/DSR set (actually we leave interrupts off for now) */
    outb(COM1 + 4, 0x0B);

    /* Test the serial chip */
    outb(COM1 + 7, 0xAE);
    if (inb(COM1 + 7) != 0xAE) {
        /* Serial port not functional - silently fail.
           We're a kernel, we don't have a lot of recovery options. */
        return;
    }

    /* Set to normal operation mode */
    outb(COM1 + 4, 0x0F);
}

static int serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    /* Wait for the transmitter to be ready.
       In a real kernel you might want a timeout here,
       but for now just spin. */
    while (!serial_tx_empty());

    /* Convert newline to carriage return + newline */
    if (c == '\n') {
        outb(COM1, '\r');
        while (!serial_tx_empty());
    }
    outb(COM1, c);
}

void serial_write(const char *str) {
    while (*str) {
        serial_putc(*str++);
    }
}

/* Minimal printf for serial debugging.
   Supports: %s, %x, %d, %c, %%
   No floating point, no width specifiers, no buffer overflow protection.
   This is a debug tool, not a general-purpose printf. */
static void serial_print_hex(uint64_t num) {
    const char hex[] = "0123456789abcdef";
    char buf[17];  /* 16 hex digits + null */
    int i = 15;

    buf[16] = '\0';
    
    if (num == 0) {
        serial_putc('0');
        return;
    }

    while (num > 0 && i >= 0) {
        buf[i--] = hex[num & 0xF];
        num >>= 4;
    }
    serial_write(&buf[i + 1]);
}

static void serial_print_dec(int64_t num) {
    char buf[21];  /* max 20 digits for int64_t + sign */
    int i = 19;

    buf[20] = '\0';

    if (num == 0) {
        serial_putc('0');
        return;
    }

    int is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num > 0 && i >= 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }

    if (is_negative) {
        buf[i--] = '-';
    }

    serial_write(&buf[i + 1]);
}

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (s) serial_write(s);
                    else serial_write("(null)");
                    break;
                }
                case 'x': {
                    uint64_t x = va_arg(args, uint64_t);
                    serial_putc('0');
                    serial_putc('x');
                    serial_print_hex(x);
                    break;
                }
                case 'd': {
                    int64_t d = va_arg(args, int64_t);
                    serial_print_dec(d);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    serial_putc(c);
                    break;
                }
                case '%': {
                    serial_putc('%');
                    break;
                }
                default:
                    serial_putc('%');
                    serial_putc(*fmt);
                    break;
            }
        } else {
            serial_putc(*fmt);
        }
        fmt++;
    }

    va_end(args);
}