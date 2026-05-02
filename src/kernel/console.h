#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>
#include "limine.h"

/* Initialize the framebuffer console */
void console_init(struct limine_framebuffer *fb);

/* Write a character to the screen */
void console_putc(char c);

/* Write a null-terminated string */
void console_write(const char *str);

/* Printf with framebuffer output */
void console_printf(const char *fmt, ...);

/* Clear screen */
void console_clear(void);

#endif