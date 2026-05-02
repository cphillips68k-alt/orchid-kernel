#ifndef KBD_BUF_H
#define KBD_BUF_H
#include <stdint.h>

void kbd_buf_init(void);
void kbd_buf_put(char c);
char kbd_buf_get(void);   /* blocks until character available */

#endif