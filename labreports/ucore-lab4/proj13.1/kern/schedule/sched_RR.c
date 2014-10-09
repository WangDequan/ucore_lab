#include <types.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <sched_RR.h>

/*
 * RR_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be empty after initialization.
 *   - proc_num: 0
 *   - max_time_slice: no requirement, the variable would be assigned by the caller.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
RR_init(struct run_queue *rq) {
	 list_init(&(rq->run_list));
	 rq->proc_num = 0;
}

/*
 * RR_enqueue inserts the process ``proc'' into the tail of run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``run_link'' node into correct
 * position. The procedure should also update the ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the process.
 * 
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq->run_list), &(proc->run_link));
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;
    rq->proc_num ++;
}

/*
 * RR_dequeue removes the process ``proc'' from the run-queue ``rq'',
 * the operation would be finished by simple list operations. Remember
 * to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
    rq->proc_num --;
}
/*
 * RR_pick_next pick the head of ``run-queue'' and returns the
 * corresponding process pointer. The pointer would be calculated by
 * macro le2proc, see proj13.1/kern/process/proc.h for
 * definition. Return NULL if there is no process in the queue.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static struct proc_struct *
RR_pick_next(struct run_queue *rq) {
    list_entry_t *le = list_next(&(rq->run_list));
    if (le != &(rq->run_list)) {
        return le2proc(le, run_link);
    }
    return NULL;
}

/*
 * RR_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}

struct sched_class RR_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};

