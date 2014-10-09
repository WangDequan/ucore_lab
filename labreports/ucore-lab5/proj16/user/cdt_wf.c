#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#define scprintf(...)               \
    do {                            \
        klock_aquire(lock);         \
        cprintf(__VA_ARGS__);       \
        klock_release(lock);        \
    } while (0)

int *active_reader ;  	// count of active readers
int *active_writer ; 	// count of active writers
int *waiting_reader ; 	// count of waiting readers
int *waiting_writer ; 	// count of waiting writers
cdt_t cdt_okToRead;
cdt_t cdt_okToWrite;
klock_t lock;

void
failed(void) {
    cprintf("FAIL: T.T\n");
    exit(-1);
}

void
init(void) {
    if ((cdt_okToRead = cdt_init()) < 0 || (cdt_okToWrite = cdt_init()) < 0) {
        failed();
    }
    if ((lock = klock_init()) < 0) {
        failed();
    }
    if ((active_reader = shmem_malloc(sizeof(int))) == NULL || (active_writer = shmem_malloc(sizeof(int))) == NULL
	|| (waiting_reader = shmem_malloc(sizeof(int))) == NULL || (waiting_writer = shmem_malloc(sizeof(int))) == NULL) {
        failed();
    }
    *active_reader = *active_writer = *waiting_reader = *waiting_writer = 0;
}

void
check_init_value(void) {
    if (cdt_okToRead < 0 || cdt_okToWrite < 0 ) {
	failed();
    }
    if (lock < 0 ) {
        failed();
    }
    if (*active_reader != 0 || *active_writer != 0 || *waiting_reader != 0 || *waiting_writer != 0) {
        failed();
    }
}

void
free_wf(void){
	if (  cdt_free(cdt_okToRead) < 0 ||  cdt_free(cdt_okToWrite) < 0 ){
		scprintf(" conditon free failed! \n");
		exit(-1);
	}
	if (  klock_free(lock) < 0 ){
		scprintf(" kernal lock free failed! \n");
		exit(-1);
	}    
}

/*
 * start_read is used when a read wants to enter the critical region. You
 * should check whether there are writers waiting or do writing at present. 
 * if so the process has to wait and modify the number of waiting readers, 
 * otherwise the process enter the critical region.
 * 
 * You can get help from the courseware of class lecture
 * 
 */

void
start_read(void) {
	klock_aquire(lock);
       while (((*active_writer)+(*waiting_writer)) > 0) { 
	   	// "LAB5:  " 
	   		(*waiting_reader)++;
			cdt_wait(cdt_okToRead, lock);
			(*waiting_reader)--;
      	}
     	(*active_reader) ++;
      	klock_release(lock);
}

/*
 * done_read is used when a reader want to leave the critical region. The reader
 * should check whether it is the last reader leaving and wether there are writers 
 * waiting outside. if so then it has to let a writer in.  
 * 
 * You can get help from the courseware of class lecture
 * 
 */

void
done_read(void) {
	klock_aquire(lock);
	// "LAB5:  "
	(*active_reader)--;
	if (*active_reader== 0 && *waiting_writer > 0) 
		cdt_signal(cdt_okToWrite);
	klock_release(lock);
}

/*
 * start_write is used when a writer want to enter  the critical region. The writer 
 * should check whether there are readers or writer still in the critical region,
 * if so writer has to wait, if not the writer enter.
 * 
 * You can get help from the courseware of class lecture
 * 
 */

void
start_write(void) {
	klock_aquire(lock);
	// "LAB5:  " replace the condition "1" with proper condition in next line
	while (*active_reader + *active_writer > 0) {
		(*waiting_writer) ++;
		cdt_wait(cdt_okToWrite,lock);
		(*waiting_writer) --;
       	}
     	(*active_writer) ++;
	klock_release(lock);
}

/*
 * done_write is used when a writer want to leave  the critical region. The writer 
 * should let next waiting writer in, if there are writers waiting. if it is the last writer
 * leaving, it has to signal all the waiting reader to enter.
 * 
 * You can get help from the courseware of class lecture
 * 
 */

void
done_write(void) {
	klock_aquire(lock);
	// "LAB5: "
	(*active_writer)--;
	if (*waiting_writer > 0)
		cdt_signal(cdt_okToWrite);
	else if (*waiting_reader > 0) {
		int n = *waiting_reader;
		while (n > 0) {
			cdt_signal(cdt_okToRead);
			n--;
		}
	}
		
	klock_release(lock);
}

void
writer(int id, int time) {
	scprintf("writer %d: (pid:%d) arrive \n", id, getpid());
       	start_write();
       	scprintf("    writer_wf start %d: (pid:%d)  %d\n", id, getpid(), time);
    	sleep(time);
    	scprintf("    writer_wf end %d: (pid:%d) %d\n", id, getpid(), time);
       	done_write(); 
}

void
reader(int id, int time) {
    	scprintf("reader %d: (pid:%d) arrive\n", id, getpid());
   	start_read();
    	scprintf("    reader_wf start %d: (pid:%d) %d\n", id, getpid(), time);
    	sleep(time);
    	scprintf("    reader_wf end %d: (pid:%d) %d\n", id, getpid(), time);
	done_read();
}


void
read_test_wf(void) {
    cprintf("---------------------------------\n");
    check_init_value();
    srand(0);
    int i, total = 10, time;
    for (i = 0; i < total; i ++) {
        time = (unsigned int)rand() % 3;
        if (fork() == 0) {
            yield();
            reader(i, 100 + time * 10);
            exit(0);
        }
    }

    for (i = 0; i < total; i ++) {
        if (wait() != 0) {
            failed();
        }
    }
    cprintf("read_test_wf ok.\n");
}

void
write_test_wf(void) {
    cprintf("---------------------------------\n");
    check_init_value();
    srand(100);
    int i, total = 10, time;
    for (i = 0; i < total; i ++) {
        time = (unsigned int)rand() % 3;
        if (fork() == 0) {
            yield();
            writer(i, 100 + time * 10);
            exit(0);
        }
    }

    for (i = 0; i < total; i ++) {
        if (wait() != 0) {
            failed();
        }
    }
    cprintf("write_test_wf ok.\n");
}

void
read_write_test_wf(void) {
    cprintf("---------------------------------\n");
    check_init_value();
    srand(200);
    int i, total = 10, time;
    for (i = 0; i < total; i ++) {
        time = (unsigned int)rand() % 3;
        if (fork() == 0) {
            yield();
            if (time == 0) {
                writer(i, 100 + time * 10);
            }
            else {
                reader(i, 100 + time * 10);
            }
            exit(0);
        }
    }

    for (i = 0; i < total; i ++) {
        if (wait() != 0) {
            failed();
        }
    }
    cprintf("read_write_test_wf ok.\n");
}

int
main(void) {
    init();
    read_test_wf();
    write_test_wf();
    read_write_test_wf();
    free_wf();
    cprintf("condition reader_writer_wf_test pass..\n");
    return 0;
}

