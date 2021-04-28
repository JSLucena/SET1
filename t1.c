/*
 * example of coroutines / fibers using setjmp()/longjmp()
 */

#include <hf-risc.h>

#define N_TASKS		10
#define READY		1
#define BLOCKED		2
#define RUNNING		3

#define DEBUG 1


typedef volatile uint32_t jmp_buf[20];

int32_t _interrupt_set(int32_t s);
int32_t setjmp(jmp_buf env);
void longjmp(jmp_buf env, int32_t val);


/* kernel data / structures */

jmp_buf jmp[N_TASKS];
void (*tasks[N_TASKS])(void) = {[0 ... N_TASKS - 1] = 0};
volatile int cur = 0, n_tasks = 0;
volatile unsigned int ctx_switches = 0;

volatile int running = 0;
typedef struct
{
	volatile uint32_t priority;
	volatile uint32_t curr_priority;
	volatile uint32_t state;
	void (*task)();
	jmp_buf regs;
	
} TCB;

TCB tasksTCB[N_TASKS];




/* kernel functions */

void schedule(void)
{
	if (!setjmp(tasksTCB[cur].regs)) {
		if (n_tasks == ++cur)
			cur = 0;
		ctx_switches++;
		_interrupt_set(1);
		longjmp(tasksTCB[cur].regs, 1);
	}
}

void timer1ctc_handler(void)
{
	schedule();
}

void task_yield()
{
	if (!setjmp(tasksTCB[running].regs)) {
		if (n_tasks == ++running)
			running = 0;
		ctx_switches++;
		longjmp(tasksTCB[running].regs, 1);
	}
}

void task_wfi()
{
	volatile unsigned int s;
	
	s = ctx_switches;
	while (s == ctx_switches);
}

int task_add(void *task, int priority)
{
	tasksTCB[cur].task = task;
	tasksTCB[cur].priority = priority;
	tasksTCB[cur].curr_priority = priority;
	tasksTCB[cur].state = READY;
	
	//tasks[cur] = task;
	
	n_tasks++;
	
	return cur++;
}

void task_block(int tsk)
{
	tasksTCB[tsk].state = BLOCKED;
}

void task_resume(int tsk)
{
	tasksTCB[tsk].state = READY;
}

void task_init(volatile char *guard, int guard_size)
{
	memset((char *)guard, 0, guard_size);
	
	
	if (!setjmp(tasksTCB[cur].regs)) {
		if (n_tasks-1 != cur)
			(tasksTCB[++cur].task)();
	}	
	
}

void timer_init()
{
	TIMER1PRE = TIMERPRE_DIV64;

	/* unlock TIMER1 for reset */
	TIMER1 = TIMERSET;
	TIMER1 = 0;

	/* TIMER1 frequency: (39063 * 16) = 250000 cycles (10ms timer @ 25MHz) */
	TIMER1CTC = 15625;
}

void sched_init()
{
	running = 0;
	cur = 0;
	TIMERMASK |= MASK_TIMER1CTC;		/* enable interrupt mask for TIMER1 CTC events */
	
	tasksTCB[0].state = RUNNING;
	(*tasksTCB[0].task)();
}


/* tasks */
void idle_task(void)
{
	volatile char guard[1000];		/* reserve some stack space */

	
	task_init(guard, sizeof(guard));
	
	#ifdef DEBUG
	printf("IDLE\n");
	#endif
	
	while (1) {				/* task body */
		printf("[idle]\n");
		task_wfi();			/* wait for an interrupt, to avoid too much text */
	}
}



void task2(void)
{
	volatile char guard[1000];		/* reserve some stack space */
	int cnt = 300000;

	task_init(guard, sizeof(guard));

	#ifdef DEBUG
	printf("TASK 2\n");
	#endif


	while (1) {				/* task body */
		printf("[task 2 %d]\n", cnt++);
		task_wfi();			/* wait for an interrupt, to avoid too much text */
	}
}

void task1(void)
{
	volatile char guard[1000];		/* reserve some stack space */
	int cnt = 200000;

	task_init(guard, sizeof(guard));

	#ifdef DEBUG
	printf("TASK 1\n");
	#endif

	while (1) {				/* task body */
		printf("[task 1 %d]\n", cnt++);
		task_wfi();
	}
}

void task0(void)
{
	volatile char guard[1000];		/* reserve some stack space */
	int cnt = 100000;

	task_init(guard, sizeof(guard));

	#ifdef DEBUG
	printf("TASK 0\n");
	#endif

	while (1) {				/* task body */
		printf("[task 0 %d]\n", cnt++);
		task_wfi();
	}
}


/* kernel initialization */

int main(void)
{
	
	task_add(idle_task,255);
	task_add(task0,2);
	task_add(task1,3);
	task_add(task2,2);
	
	timer_init();
	#ifdef DEBUG
	//	printf("[task idle prio %d]\n",tasksTCB[0].priority);
	//	printf("[task 0 prio %d]\n",tasksTCB[1].priority);
	//	printf("[task 1 prio %d]\n",tasksTCB[2].priority);
	//	printf("[task 2 prio %d]\n",tasksTCB[3].priority);
	#endif
	
	sched_init();

	return 0;
}
