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
int first_exec = 1;



/* kernel functions */
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
	tasksTCB[tsk+1].state = BLOCKED;
}

void task_resume(int tsk)
{
	tasksTCB[tsk+1].state = READY;
}





void schedule(void)
{
	if(first_exec == 1)
	{
			first_exec = 0;
			if (!setjmp(tasksTCB[running].regs))
			{
				running = 0;
				longjmp(tasksTCB[running].regs, 1);
			}
			
			
	}
	
	
	
	
	int next_task = 0;
	for(int i = 1; i < n_tasks-1; i++)
	{
		if(tasksTCB[i].state == READY && tasksTCB[i].curr_priority > 0)
			tasksTCB[i].curr_priority--;
		#ifdef DEBUG
			printf("task = %d : prio = %d : curr_prio %d : state %d \t",i-1, tasksTCB[i].priority, tasksTCB[i].curr_priority, tasksTCB[i].state);
		#endif DEBUG
		
		if(tasksTCB[i].curr_priority == 0 && next_task == 0 && tasksTCB[i].state == READY)
				next_task = i;
	}
		#ifdef DEBUG
			printf(" next task %d \n", next_task-1);
		#endif DEBUG
		
		
	
		if (!setjmp(tasksTCB[running].regs)) 
		{
			tasksTCB[running].curr_priority = tasksTCB[running].priority;
			if(tasksTCB[running].state != BLOCKED)
				tasksTCB[running].state = READY;
			
			running = next_task;
			tasksTCB[running].state = RUNNING;
			ctx_switches++;
			_interrupt_set(1);
			longjmp(tasksTCB[running].regs, 1);
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
	
	switch (ctx_switches)
	{
		case 20:
			task_block(0);
			schedule();
			break;
		case 25:
			task_block(1);
			task_block(2);
			task_resume(0);
			schedule();
			break;
		case 30:
			task_resume(1);
			break;
		case 35:
			task_resume(2);
			break;

	}
	
	
	while (s == ctx_switches);
}


void task_init(volatile char *guard, int guard_size)
{
	memset((char *)guard, 0, guard_size);
	
	
	if (!setjmp(tasksTCB[running].regs)) {
		if (n_tasks-1 != running)
			(tasksTCB[++running].task)();
	}	
	
}

void timer_init()
{
	TIMER1PRE = TIMERPRE_DIV256;

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



	while (1) {				/* task body */
		printf("[task 0 %d]\n", cnt++);
		task_wfi();
	}
}


/* kernel initialization */

int main(void)
{
	
	task_add(idle_task,255);
	task_add(task0,3);
	task_add(task1,6);
	task_add(task2,3);
	task_add(idle_task,255);
	
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
