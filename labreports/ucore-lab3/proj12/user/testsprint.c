#include <stdio.h>
#include <ulib.h>

int
main(void) {
	int len = sprint("sprint test\n");
	cprintf("len: %d\n", len);
	return 0;
}

