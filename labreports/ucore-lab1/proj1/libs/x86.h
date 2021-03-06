#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include <types.h>

static inline uint8_t inb(uint16_t port) __attribute__((always_inline));
static inline void outb(uint16_t port, uint8_t data) __attribute__((always_inline));

static inline uint8_t
inb(uint16_t port) {
	uint8_t data;
	asm volatile ("inb %1, %0" : "=a" (data) : "d" (port));
	return data;
}

static inline void
outb(uint16_t port, uint8_t data) {
	asm volatile ("outb %0, %1" :: "a" (data), "d" (port));
}

#endif /* !__LIBS_X86_H__ */

