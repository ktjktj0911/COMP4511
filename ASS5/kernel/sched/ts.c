/* FILE: kernel/sched/ts.c */

#include "sched.h"
int prioQueue(int prio) {
	int x = -1 * (prio - 139);
	return x;
}

int prioTimeslice(int prio) {
	int x;
	switch (prio / 10) {
	case 13:
		x = 16;
		break;
	case 12:
		x = 12;
		break;
	case 11:
		x = 8;
		break;
	case 10:
		x = 4;
		break;
	}
	return x;
}

void init_ts_rq(struct ts_rq *ts_rq)
{
	int x;
	for (x = 0; x < 40; x++) {
		INIT_LIST_HEAD(&ts_rq->queue[x]);
	}
	printk(KERN_INFO"init_ts_rq");
}

static void enqueue_task_ts(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_ts_entity *ts_se = &p->ts;
	if (flags & ENQUEUE_WAKEUP) {
		if (p->prio / 10 == 13) {
			p->prio = 109;
		} else if (p->prio / 10 == 12) {
			p->prio = 108;
		} else if (p->prio / 10 == 10) {
			p->prio = 100;
		} else if (p->prio == 119) {
			p->prio = 107;
		} else if (p->prio == 118) {
			p->prio = 106;
		} else if (p->prio == 117) {
			p->prio = 105;
		} else if (p->prio == 116) {
			p->prio = 104;
		} else if (p->prio == 115 || p->prio == 114) {
			p->prio = 103;
		} else if (p->prio == 113 || p->prio == 112) {
			p->prio = 102;
		} else if (p->prio == 111 || p->prio == 110) {
			p->prio = 101;
		}
	}
	list_add_tail(&ts_se->list, &rq->ts.queue[prioQueue(p->prio)]);

	printk(KERN_INFO"[SCHED_TS] ENQUEUE: p->pid=%d, p->policy=%d, p->prio=%d "
		"curr->pid=%d, curr->policy=%d, rq->curr->prio=%d, flags=%d, prioQueue(p->prio)=%d\n",
		p->pid, p->policy, p->prio, rq->curr->pid, rq->curr->policy, rq->curr->prio, flags, prioQueue(p->prio));
}

static void dequeue_task_ts(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_ts_entity *ts_se = &p->ts;

	list_del(&ts_se->list);

	printk(KERN_INFO"[SCHED_TS] DEQUEUE: p->pid=%d, p->policy=%d, p->prio=%d "
		"curr->pid=%d, curr->policy=%d, rq->curr->prio=%d, flags=%d\n",
		p->pid, p->policy, p->prio, rq->curr->pid, rq->curr->policy, rq->curr->prio, flags);
}

static void yield_task_ts(struct rq *rq)
{
	struct sched_ts_entity *ts_se = &rq->curr->ts;
	struct ts_rq *ts_rq = &rq->ts;

	//yield the current task, put it to the end of the queue
	list_move_tail(&ts_se->list, &ts_rq->queue[prioQueue(rq->curr->prio)]);

	printk(KERN_INFO"[SCHED_TS] YIELD: Process-%d, rq->curr->prio=%d\n", rq->curr->pid, rq->curr->prio);
}

static void check_preempt_curr_ts(struct rq *rq, struct task_struct *p, int flags)
{
	printk(KERN_INFO"[SCHED_TS] preempt: curr-process=%d, rq->curr->prio=%d, p->pid=%d, p->prio=%d, rq->curr->flags=%d, flags=%d\n", rq->curr->pid, rq->curr->prio, p->pid, p->prio, rq->curr->flags, flags);
	if (rq->curr->prio > p->prio) {
		resched_curr(rq);
		printk(KERN_INFO"[SCHED_TS] preempt: rq->curr->flags=%d, flags=%d\n", rq->curr->flags, flags);
	}
	return;
}

static struct task_struct *pick_next_task_ts(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	printk(KERN_INFO"[SCHED_TS] pick_next_task_ts: Process-%d, rq->curr->prio=%d, prev->pid=%d, prev->prio=%d\n", rq->curr->pid, rq->curr->prio, prev->pid, prev->prio);
	struct sched_ts_entity *ts_se = NULL;
	struct task_struct *p = NULL;
	struct ts_rq *ts_rq = &rq->ts;
	int x;
	int count;
	for (x = 39; x >= 0; x--) {
		if (!list_empty(&ts_rq->queue[x])) {
			break;
		}
	}
	printk(KERN_INFO"[SCHED_TS] pick_next_task_ts: %d\n", x);
	if (x == -1) {
		return NULL;
	}
	put_prev_task(rq, prev);
	ts_se = list_entry(ts_rq->queue[x].next,
						 struct sched_ts_entity,
						 list);
	p = container_of(ts_se, struct task_struct, ts);
	return p;
}

static void put_prev_task_ts(struct rq *rq, struct task_struct *p)
{
	/* Not implemented in ts */
}

static void set_curr_task_ts(struct rq *rq)
{
	/* Not implemented in ts */
}

static void task_tick_ts(struct rq *rq, struct task_struct *p, int queued)
{
	if (p->policy != SCHED_TS)
		return;
	printk(KERN_INFO"[SCHED_TS] TASK TICK: Process-%d = %d, p->prio=%d\n", p->pid, p->ts.time_slice, p->prio);
	if(--p->ts.time_slice)
		return;

	if (p->prio + 10 >= 139) {
		p->prio = 139;
	}
	else {
		p->prio = p->prio + 10;
	}
	printk(KERN_INFO"[SCHED_TS] TASK TICK: Process-%d, p->prio=%d\n", p->pid, p->prio);

	p->ts.time_slice = prioTimeslice(p->prio);
	list_move_tail(&p->ts.list, &rq->ts.queue[prioQueue(p->prio)]);

	set_tsk_need_resched(p);
}

unsigned int get_rr_interval_ts(struct rq *rq, struct task_struct *p)
{
	/* Return the default time slice */
	return prioTimeslice(p->prio);
}

static void prio_changed_ts(struct rq *rq, struct task_struct *p, int oldprio)
{
	return; /* ts don't support priority */
}

static void switched_to_ts(struct rq *rq, struct task_struct *p)
{
	/* nothing to do */
}

static void update_curr_ts(struct rq *rq)
{
	/* nothing to do */
}

#ifdef CONFIG_SMP
static int select_task_rq_ts(struct task_struct *p, int cpu, int sd_flag, int flags)
{
	return task_cpu(p); /* ts tasks never migrate */
}
#endif

const struct sched_class ts_sched_class = {
	.next = &fair_sched_class,
	.enqueue_task = enqueue_task_ts,
	.dequeue_task = dequeue_task_ts,
	.yield_task = yield_task_ts,
	.check_preempt_curr = check_preempt_curr_ts,
	.pick_next_task = pick_next_task_ts,
	.put_prev_task = put_prev_task_ts,
	.set_curr_task = set_curr_task_ts,
	.task_tick = task_tick_ts,
	.get_rr_interval = get_rr_interval_ts,
	.prio_changed = prio_changed_ts,
	.switched_to = switched_to_ts,
	.update_curr = update_curr_ts,
#ifdef CONFIG_SMP
	.select_task_rq = select_task_rq_ts,
	.set_cpus_allowed = set_cpus_allowed_common,
#endif
};
