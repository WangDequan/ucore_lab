#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define TOTAL 5
volatile int *acc;

static void *
safe_shmem_malloc(size_t size) {
    void *ret;
    if ((ret = shmem_malloc(size)) == NULL) {
        exit(-0xdead);
    }
    return ret;
}

static void
spin_delay(void)
{
	 int i;
	 volatile int j;
	 for (i = 0; i != 100; ++ i)
	 {
		  j = !j;
	 }
}

int
main(void) {
	 acc = safe_shmem_malloc(sizeof(int) * TOTAL);
	 int pids[TOTAL];
	 memset(pids, 0, sizeof(pids));
	 lab4_set_priority(TOTAL + 1);
	
	 int i;
	 for (i = 0; i < TOTAL; i ++) {
		  if ((pids[i] = fork()) == 0) {
			   lab4_set_priority(i + 1);
			   acc[i] = 0;
			   while (1) {
					spin_delay();
					++ acc[i];
			   }
		  }
		  if (pids[i] < 0) {
			   goto failed;
		  }
	 }

	 cprintf("fork ok.\n");

	 /* for (i = 0; i < 1000000; i ++) { */
	 /* 	  spin_delay(); */
	 /* } */
	 sleep(10000);

	 for (i = 0; i < TOTAL; i ++) {
		  if (pids[i] > 0) {
			   kill(pids[i]);
		  }
	 }

	 cprintf("acc:");
	 for (i = 0; i != TOTAL; i ++)
	 {
		  cprintf(" %d", (acc[i] * 2 / acc[0] + 1) / 2);
	 }
	 cprintf("\n");

	 return 0;

failed:
	 for (i = 0; i < TOTAL; i ++) {
		  if (pids[i] > 0) {
			   kill(pids[i]);
		  }
	 }
	 panic("FAIL: T.T\n");
}

