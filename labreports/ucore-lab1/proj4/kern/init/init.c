#include <types.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>

int kern_init(void) __attribute__((noreturn));

int
kern_init(void) {
	extern char edata[], end[];
	memset(edata, 0, end - edata);

	cons_init();				// init the console

	const char *message = "(THU.CST) os is loading ...";
	cprintf("%s\n\n", message);

	print_kerninfo();

	pic_init();					// init interrupt controller 对中断控制器的初始化
	idt_init();					// init interrupt descriptor table 对整个中断门描述符表的创建

	clock_init();				// init clock interrupt

	sti();						// enable irq interrupt

	/* do nothing */
	while (1);
}

