#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* Initialize COM1 serial port at 0x3F8 */
void serial_init(void);

/* Write a null-terminated string to serial */
void serial_write(const char *str);

/* Write a single character */
void serial_putc(char c);

/* Printf-like debug output (supports %s, %x, %d, %c, %%) */
void serial_printf(const char *fmt, ...);

#endif