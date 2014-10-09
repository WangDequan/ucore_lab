#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#define BUFFER_SIZE 2
#define PROC_COUNT 4
#define ITEM_COUNT 4

#define scprintf(...)               	\
    do {                           	\
        sem_wait(mutex);      		\
        cprintf(__VA_ARGS__);       	\
        sem_post(mutex);      		\
    } while (0)

//int buffer[BUFFER_SIZE];

int *count;
cdt_t notFull, notEmpty;
klock_t lock;
sem_t mutex;

void
failed(void) {
    scprintf("FAIL: T.T\n");
    exit(-1);
}

void Deposit(int item, int id) {
    klock_aquire(lock);
    while (*count == BUFFER_SIZE){
		scprintf("buffer is full!\n");
		cdt_wait(notFull,lock);
    }
	
    //buffer[count]=item;
    //scprintf("put item%d into buffer.\n", item);

    (*count)++;
    scprintf("producer %d :	Deposit an item!	rest_count == %d  \n" , id ,(*count));
    cdt_signal(notEmpty);
    klock_release(lock);
}

int Remove(int id) {
    int item=0;
    klock_aquire(lock);
    while (*count == 0){
	scprintf("buffer is Empty!\n");
        cdt_wait(notEmpty,lock);
    }

    //item = buffer[count];
    //scprintf("get item%d from buffer.\n", item);

    (*count)--;
    scprintf("consumer %d :	Remove an item! 	rest_count == %d  \n", id ,(*count));
    cdt_signal(notFull);
    klock_release(lock);
    return item;
}

void
producer_consumer_init(){
	if ((mutex = sem_init(1)) < 0) {
        	scprintf(" mutex initialization failed! \n");
		exit(-1);
    	}
	if ( (notFull = cdt_init()) ==0 || (notEmpty = cdt_init()) ==0 ){
		scprintf(" conditon initialization failed! \n");
		exit(-1);
	}
	if ( (lock = klock_init()) == 0 ){
		scprintf(" kernal lock initialization failed! \n");
		exit(-1);
	}
	if ((count = shmem_malloc(sizeof(int))) == NULL ) {
		scprintf(" count initialization failed! \n");
        	exit(-1);
    	}
    	*count =  0;
    
}

void
producer_consumer_free(){
	if (  cdt_free(notFull) < 0 ||  cdt_free(notEmpty) < 0 ){
		scprintf(" conditon free failed! \n");
		exit(-1);
	}
	if (  klock_free(lock) < 0 ){
		scprintf(" kernal lock free failed! \n");
		exit(-1);
	}    
}

void producer(int producer_count, int time){
	int count,item,p_number,sleeptime;
	count=0;
	p_number=producer_count;
	sleeptime=time;
	while ( count++ < ITEM_COUNT ){
		item=count;
		Deposit(item, p_number);
		sleep(sleeptime);
	}
	return;
}

void consumer(int consumer_count, int time){
	int count,item,c_number,sleeptime;
	count = 0;
	c_number=consumer_count;
	sleeptime=time;
	while ( count++ < ITEM_COUNT ){
		item = Remove(c_number);
		sleep(sleeptime);
	}
	return;
}

void
producer_consumer_test(void) {	 
	scprintf("-------------  Now in producer consumer test  -------------\n");
	srand(200);	
	int pid,i, total = PROC_COUNT, consumer_count=0, producer_count=0, time;

	for (i = 0; i < total ; i ++) {
		time = (unsigned int)rand() % 20;		
		if( producer_count < total/2 && time % 2 == 0 ){
			producer_count ++;
			if((pid=fork()) == 0){
				yield();
				producer(producer_count,time);
				exit(0);
			}
		}
		else if (consumer_count < total/2 ){
			consumer_count ++;
			if((pid=fork()) == 0){
				yield();
				consumer(consumer_count, time);
				exit(0);
			}
		}
	}

	for (i = 0; i < total; i ++) {
		if (wait() != 0) {
			failed();
		}
	}

	scprintf("condition producer_consumer_test ok.\n");

}

int
main(void) {
	producer_consumer_init();
	producer_consumer_test();
	producer_consumer_free();
	scprintf("condition producer_consumer_test pass.\n");
	return 0;
}


