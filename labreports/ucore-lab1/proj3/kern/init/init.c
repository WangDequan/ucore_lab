#include <types.h>
#include <stdio.h>
#include <string.h>
#include <console.h>

int kern_init(void) __attribute__((noreturn));

int
kern_init(void) {
	extern char edata[], end[];
	memset(edata, 0, end - edata);	//把BSS段对应的内存空间清零，确保执行代码的正确运行

	cons_init();				// init the console

	const char *message = "(THU.CST) os is loading ...";
	cprintf("[%d]%s\n", strlen(message), message);

	while (1);
}

