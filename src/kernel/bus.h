#ifndef BUS_H
#define BUS_H
#include <stdint.h>

/* Register a named port and get a port number.
   Port numbers are allocated from 0 upward. */
int bus_register(const char *name);

/* Look up a port by name. Returns -1 if not found. */
int bus_lookup(const char *name);

#endif