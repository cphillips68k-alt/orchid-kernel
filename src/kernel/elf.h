#ifndef ELF_H
#define ELF_H
#include <stdint.h>
#include <stddef.h>

int elf_load(const uint8_t *elf_data, size_t elf_size);
void sys_exec(const uint8_t *elf_data, size_t elf_size);

#endif