/*
定义并实现了bootmain函数实现了通过屏幕、串口和并口显示字符串
*/

#include <types.h>
#include <x86.h>

#define COM1			0x3F8
#define CRTPORT			0x3D4
#define LPTPORT			0x378
#define COM_TX			0			// Out: Transmit buffer (DLAB=0)
#define COM_LSR			5			// In:  Line Status Register
#define COM_LSR_TXRDY	20			// Transmit buffer avail

static uint16_t *crt = (uint16_t *) 0xB8000;		// CGA memory

/* stupid I/O delay routine necessitated by historical PC design flaws */
static void
delay(void) {
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

/*
考虑到简单性，在proj1中没有对并口设备进行初始化，通过并口进行输出的过程也很简单：
第一步：执行inb指令读取并口的I/O地址（LPTPORT + 1）的值，如果发现发现读出的值代表并口忙，
则空转一小会再读；
如果发现发现读出的值代表并口空闲，则执行outb指令把字符写到并口的I/O地址（LPTPORT ），
这样就完成了一个字符的并口输出。
*/
/* lpt_putc - copy console output to parallel port */
static void
lpt_putc(int c) {
	int i;
	for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i ++) {
		delay();
	}
	outb(LPTPORT + 0, c);
	outb(LPTPORT + 2, 0x08 | 0x04 | 0x01);
	outb(LPTPORT + 2, 0x08);
}

/*
通过CGA显示控制器进行输出的过程也很简单：首先通过in/out指令获取当前光标位置；
然后根据得到的位置计算出显存的地址，直接通过访存指令写内存来完成字符的输出；
最后通过in/out指令更新当前光标位置。
*/
/* cga_putc - print character to console */
static void
cga_putc(int c) {
	int pos;

	// cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT + 1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT + 1);

	if (c == '\n') {
		pos += 80 - pos % 80;
	}
	else {
		crt[pos ++] = (c & 0xff) | 0x0700;
	}

	outb(CRTPORT, 14);
	outb(CRTPORT + 1, pos >> 8);
	outb(CRTPORT, 15);
	outb(CRTPORT + 1, pos);
}

/*
通过串口进行输出的过程也很简单：第一步：执行inb指令读取串口的I/O地址（COM1 + COM_LSR）的值，
如果发现发现读出的值代表串口忙，则空转一小会（0x84是什么地址???）；
如果发现发现读出的值代表串口空闲，则执行outb指令把字符写到串口的I/O地址（COM1 + COM_TX），
这样就完成了一个字符的串口输出。
*/
/* serial_putc - copy console output to serial port */
static void
serial_putc(int c) {
    int i;
	for (i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i ++) {
		delay();
	}
	outb(COM1 + COM_TX, c);
}

/* 显示字符的函数接口*/
/* 一个cons_putc函数接口，完成字符的输出*/
/* cons_putc - print a single character to console*/
static void
cons_putc(int c) {
	lpt_putc(c);
	cga_putc(c);
	serial_putc(c);
}

/* 提供了一个cons_puts函数接口：完成字符串的输出*/
/* cons_puts - print a string to console */
static void
cons_puts(const char *str) {
	int i;
	for (i = 0; *str != '\0'; i ++) {
		cons_putc(*str ++);
	}
}

/* bootmain - the entry of bootloader */
void
bootmain(void) {
	cons_puts("This is a bootloader: Hello world!!");

	/* do nothing */
	while (1);
}

