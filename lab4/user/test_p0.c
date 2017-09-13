#include <inc/lib.h>
#include <inc/env.h>

void umain(int argc, char **argv)
{
	sys_env_set_priority(0, PRIORITY_0);
	int i;
	for (i = 0; i < 4; i++) 
	{
		cprintf("[%08x] This is priority_0 env\n", sys_getenvid());
	}
	return;
}
