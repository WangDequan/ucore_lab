#include <types.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <sched_stride.h>

/* You should define the BigStride constant here */
/* LAB4:  */
#define BIG_STRIDE (1 << 30)   			/* ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int
proc_stride_comp_f(void *a, void *b)
{
	 struct proc_struct *p = le2proc(a, lab4_run_pool);
	 struct proc_struct *q = le2proc(b, lab4_run_pool);
	 int32_t c = p->lab4_stride - q->lab4_stride;
	 if (c > 0) return 1;
	 else if (c == 0) return 0;
	 else return -1;
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - lab4_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
stride_init(struct run_queue *rq) {
	 /* LAB4:  */
	 list_init(&(rq->run_list));
	 rq->lab4_run_pool = NULL;
	 rq->proc_num = 0;
}

/*
 * stride_enqueue inserts the process ``proc'' into the run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``lab4_run_pool'' node into the
 * queue(since we use priority queue here). The procedure should also
 * update the meta date in ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
	 /* LAB4:  */
    proc->time_slice = rq->max_time_slice;
	rq->lab4_run_pool = skew_heap_insert(rq->lab4_run_pool, &(proc->lab4_run_pool), proc_stride_comp_f);
    proc->rq = rq;
    rq->proc_num ++;	 
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
	 /* LAB4:  */
	 rq->lab4_run_pool = skew_heap_remove(rq->lab4_run_pool, &(proc->lab4_run_pool), proc_stride_comp_f);
	 rq->proc_num--;
}

/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see proj13.1/kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
	 /* LAB4:  */
	skew_heap_entry_t *le = rq->lab4_run_pool;
	 if (le != NULL) {
		struct proc_struct *proc = le2proc(le, lab4_run_pool);
		proc->lab4_stride += BIG_STRIDE / proc->lab4_priority;
		return proc;
	 }	 	
	 return NULL;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
	 /* LAB4:  */
	 if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}

struct sched_class stride_sched_class = {
	 .name = "stride_scheduler",
	 .init = stride_init,
	 .enqueue = stride_enqueue,
	 .dequeue = stride_dequeue,
	 .pick_next = stride_pick_next,
	 .proc_tick = stride_proc_tick,
};
