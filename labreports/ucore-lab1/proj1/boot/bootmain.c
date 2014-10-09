/*
���岢ʵ����bootmain����ʵ����ͨ����Ļ�����ںͲ�����ʾ�ַ���
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
���ǵ����ԣ���proj1��û�жԲ����豸���г�ʼ����ͨ�����ڽ�������Ĺ���Ҳ�ܼ򵥣�
��һ����ִ��inbָ���ȡ���ڵ�I/O��ַ��LPTPORT + 1����ֵ��������ַ��ֶ�����ֵ������æ��
���תһС���ٶ���
������ַ��ֶ�����ֵ�����ڿ��У���ִ��outbָ����ַ�д�����ڵ�I/O��ַ��LPTPORT ����
�����������һ���ַ��Ĳ��������
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
ͨ��CGA��ʾ��������������Ĺ���Ҳ�ܼ򵥣�����ͨ��in/outָ���ȡ��ǰ���λ�ã�
Ȼ����ݵõ���λ�ü�����Դ�ĵ�ַ��ֱ��ͨ���ô�ָ��д�ڴ�������ַ��������
���ͨ��in/outָ����µ�ǰ���λ�á�
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
ͨ�����ڽ�������Ĺ���Ҳ�ܼ򵥣���һ����ִ��inbָ���ȡ���ڵ�I/O��ַ��COM1 + COM_LSR����ֵ��
������ַ��ֶ�����ֵ������æ�����תһС�ᣨ0x84��ʲô��ַ???����
������ַ��ֶ�����ֵ�����ڿ��У���ִ��outbָ����ַ�д�����ڵ�I/O��ַ��COM1 + COM_TX����
�����������һ���ַ��Ĵ��������
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

/* ��ʾ�ַ��ĺ����ӿ�*/
/* һ��cons_putc�����ӿڣ�����ַ������*/
/* cons_putc - print a single character to console*/
static void
cons_putc(int c) {
	lpt_putc(c);
	cga_putc(c);
	serial_putc(c);
}

/* �ṩ��һ��cons_puts�����ӿڣ�����ַ��������*/
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

