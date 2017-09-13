#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

//LAB4 challenge priority
//#define TEST_PRIORITY

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	int i;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING) and never choose an
	// idle environment (env_type == ENV_TYPE_IDLE).  If there are
	// no runnable environments, simply drop through to the code
	// below to switch to this CPU's idle environment.

	// LAB 4: Your code here.
#ifdef TEST_PRIORITY
	uint32_t maxpriority = 0;
	int32_t envid = -1;
	uint32_t env_id;
	if(curenv != NULL)
	{
		env_id = ENVX(curenv->env_id);
		for(i = (env_id+1)%NENV; i != env_id;)
		{
			if(envs[i].env_status == ENV_RUNNABLE && envs[i].env_type != ENV_TYPE_IDLE && envs[i].env_priority > maxpriority) 
			{
				maxpriority = envs[i].env_priority;
				envid = i;
			}
			i = (i+1)%NENV;
		}
		i = envid;
		if (i != env_id && i != -1) 
		{
			env_run(&envs[i]);
		}
		if (curenv->env_status == ENV_RUNNING)
		{
			env_run(curenv);
		}
	}
#else
	uint32_t env_id;
	if(curenv != NULL)
	{
		env_id = ENVX(curenv->env_id);
		i = (env_id+1)%NENV;
		while(i != env_id)
		{
			if(envs[i].env_status == ENV_RUNNABLE && envs[i].env_type != ENV_TYPE_IDLE) 
			{
				env_run(&envs[i]);
			}
			i = (i+1)%NENV;
		}
		if(curenv->env_status == ENV_RUNNING)
		{
			env_run(curenv);
		}
	}
#endif

	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if (envs[i].env_type != ENV_TYPE_IDLE &&
		    (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);
}
