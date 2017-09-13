#include <inc/lib.h>
#include <inc/env.h>

void umain(int argc, char **argv)
{
	sys_env_set_priority(0, PRIORITY_3);
	int i;
	for (i = 0; i < 4; i++) 
	{
		cprintf("[%08x] This is priority_3 env\n\n", sys_getenvid());
	}
	return;
}
