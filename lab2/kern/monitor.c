// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "time","Display time",mon_time},
	{ "backtrace","Stack backtrace",mon_backtrace},
	{ "showmappings","Show memory mappings",mon_showmappings},
	{ "changeperm","Set, clear or change the permission of any mapping in the current address space",mon_changeperm},
	{ "dumpmem","Set, clear or change the permission of any mapping in the current address space",mon_dumpmem},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}

int mon_time(int argc, char **argv, struct Trapframe *tf)
{
	if(argc==1)
	{
		cprintf("Usage: time [command]\n");
		return 0;
	}
	int i=0;
	while(i<NCOMMANDS)
	{
		if(strcmp(argv[1],commands[i].name)==0)
		{
			unsigned long long time1=read_tsc();
			commands[i].func(argc-1,argv+1,tf);
			unsigned long long time2=read_tsc();
			cprintf("%s cycles: %llu\n",argv[1],time2-time1);
			return 0;
		}
		i+=1;
	}
	cprintf("Unknown command\n",argv[1]);
	return 0;
}

uint32_t strtoint(char* str)
{
	if(str==NULL)return 0;
	if((*str=='\0')||(*(str+1)=='\0'))return 0;
	char* buf=str+2;//start after 0x
	uint32_t result=0;
	while((*buf)!='\0')
	{
		uint32_t num=0;
		if((*buf>='a')&&(*buf<='f'))
		{
			num=*buf-'a'+10;
		}
		else if((*buf>='A')&&(*buf<='F'))
		{
			num=*buf-'A'+10;
		}
		else if((*buf>='0')&&(*buf<='9'))
		{
			num=*buf-'0';
		}
		result=result*16+num;
		buf+=1;
	}
	return result;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc !=3 ) {
		cprintf("Usage: showmappings 0xbegin_addr 0xend_addr\n");
		return 0;
	}
	uint32_t begin_addr=strtoint(argv[1]);
	uint32_t end_addr=strtoint(argv[2]);
	for(;begin_addr<=end_addr;begin_addr+=PGSIZE)
	{
		pte_t* pte1=pgdir_walk(kern_pgdir,(void*)begin_addr,1);
		if(pte1==NULL)cprintf("Showmappings: memory not enough\n");
		else if((*pte1)&PTE_P)
		{
			cprintf("0x%x :physical mapping page 0x%x,permission PTE_P %d PTE_W %d PTE_U %d\n" ,begin_addr,PTE_ADDR(*pte1),(*pte1)&PTE_P, ((*pte1)&PTE_W)>>1, ((*pte1)&PTE_U)>>2);
		}
	}

	return 0;
}

int mon_changeperm(int argc, char **argv, struct Trapframe *tf) 
{
	if (argc <= 3) {
		cprintf("Usage: changeperm 0xaddr [commandtype] [permtype] [permvalue]\n");
		cprintf("[commandtype]:0/1/2	0 represents set, 1 represents change, 2 represents clear\n");
		cprintf("[permtype]:0/1/2	0 represents PTE_P, 1 represents PTE_W, 2 represents PTE_U\n");
		cprintf("[permvalue]:0/1	be null if command type is clear\n");
		return 0;
	}
	uint32_t addr=strtoint(argv[1]);
	uint32_t commandtype=0;
	char* str1=argv[2];
	if((*str1>='0')&&(*str1<='2')&&(*(str1+1)=='\0'))commandtype=*str1-'0';
	else
	{
		cprintf("Usage: changeperm 0xaddr [commandtype] [permtype] [permvalue]\n");
		cprintf("[commandtype]:0/1/2	0 represents set, 1 represents change, 2 represents clear\n");
		cprintf("[permtype]:0/1/2	0 represents PTE_P, 1 represents PTE_W, 2 represents PTE_U\n");
		cprintf("[permvalue]:0/1	be null if command type is clear\n");
		return 0;
	}
	if(((commandtype==0||commandtype==1)&&argc!=5)||(commandtype==2&&argc!=4))
	{
		cprintf("Usage: changeperm 0xaddr [commandtype] [permtype] [permvalue]\n");
		cprintf("[commandtype]:0/1/2	0 represents set, 1 represents change, 2 represents clear\n");
		cprintf("[permtype]:0/1/2	0 represents PTE_P, 1 represents PTE_W, 2 represents PTE_U\n");
		cprintf("[permvalue]:0/1	be null if command type is clear\n");
		return 0;
	}
	uint32_t permtype=0;
	char* str2=argv[3];
	if(*str2>='0'&&*str2<='2'&&*(str2+1)=='\0')permtype=*str2-'0';
	else
	{
		cprintf("Usage: changeperm 0xaddr [commandtype] [permtype] [permvalue]\n");
		cprintf("[commandtype]:0/1/2	0 represents set, 1 represents change, 2 represents clear\n");
		cprintf("[permtype]:0/1/2	0 represents PTE_P, 1 represents PTE_W, 2 represents PTE_U\n");
		cprintf("[permvalue]:0/1	be null if command type is clear\n");
		return 0;
	}
	uint32_t permvalue=0;
	if(commandtype!=2)
	{
		char* str3=argv[4];
		if(*str3>='0'&&*str3<='2'&&*(str3+1)=='\0')permvalue=*str3-'0';
		else
		{
			cprintf("Usage: changeperm 0xaddr [commandtype] [permtype] [permvalue]\n");
			cprintf("[commandtype]:0/1/2	0 represents set, 1 represents change, 2 represents clear\n");
			cprintf("[permtype]:0/1/2	0 represents PTE_P, 1 represents PTE_W, 2 represents PTE_U\n");
			cprintf("[permvalue]:0/1	be null if command type is clear\n");
			return 0;
		}
	}
	pte_t* pte1=pgdir_walk(kern_pgdir,(void*)addr,1);
	cprintf("Before: 0x%x :permission PTE_P %d PTE_W %d PTE_U %d\n",addr,(*pte1)&PTE_P, ((*pte1)&PTE_W)>>1, ((*pte1)&PTE_U)>>2);
	if(pte1==NULL)return 0;
	uint32_t perm=0;
	if(permtype==0)perm=PTE_P;
	if(permtype==1)perm=PTE_W;
	if(permtype==2)perm=PTE_U;
	if(permvalue==1)*pte1=*pte1|perm;
	else *pte1=*pte1&(~perm);
	if(commandtype==2)*pte1=*pte1&(~perm);
	cprintf("After: 0x%x :permission PTE_P %d PTE_W %d PTE_U %d\n",addr,(*pte1)&PTE_P, ((*pte1)&PTE_W)>>1, ((*pte1)&PTE_U)>>2);
	return 0;
}

int mon_dumpmem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc !=4 ) 
	{
		cprintf("Usage: dumpmem [ADDR_TYPE] 0xbegin_addr 0xend_addr\n");
		cprintf("[ADDR_TYPE]:P/V	P represents physicval address type,V represents virtual address type\n");
		return 0;
	}
	char type='0';
	if((*(argv[1]))=='P'||(*(argv[1]))=='V')
		type=(*(argv[1]));
	else 
	{
		cprintf("Usage: dumpmem [ADDR_TYPE] 0xbegin_addr 0xend_addr\n");
		cprintf("[ADDR_TYPE]:P/V	P represents physicval address type,V represents virtual address type\n");
		return 0;
	}
	uint32_t begin_addr=strtoint(argv[2]);
	uint32_t end_addr=strtoint(argv[3]);
	if(begin_addr>=end_addr||(begin_addr%4)!=0||(end_addr%4)!=0)
	{
		cprintf("Invalid addresses\n");
		return 0;
	}
	if(type=='P')
	{
		if(PGNUM(begin_addr) >= npages||PGNUM(end_addr) >= npages)
		{
			cprintf("Invalid addresses\n");
			return 0;
		}
		begin_addr=(uint32_t)KADDR(((physaddr_t)begin_addr));
		end_addr=(uint32_t)KADDR(((physaddr_t)end_addr));
	}
	while(begin_addr<=end_addr)
	{
		pte_t* pte1=pgdir_walk(kern_pgdir,(void*)begin_addr,0);
		if(pte1==NULL)cprintf("0x%08lx:NULL\n",begin_addr);
		else cprintf("0x%08lx:0x%x\n",begin_addr,*((uint32_t*)(begin_addr)));
		begin_addr+=4;
	}
	
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;

	// Your code here.
	pret_addr=(char*)(read_pretaddr());
	void (*funcp)()=do_overflow;
	uint32_t funcaddr1=((uint32_t)funcp)+3;
	char* funcaddr=(char*)(&funcaddr1);
	int i=0;
	while(i<4)
	{
		int j=*funcaddr;
		j=j&0xff;
		memset(str, 0xd, j);
		str[j]='\0';
		cprintf("%s%n",str,pret_addr);
		funcaddr+=1;
		pret_addr+=1;
		i+=1;
	}

	//uint32_t *addr=(uint32_t*)read_pretaddr();
	//void (*funcp)=do_overflow;
	//*addr=((uint32_t)funcp)+3;
	//cprintf("%08x\n",*((uint32_t*)read_pretaddr()));
	//cprintf("%08x\n",funcaddr1);
}

void
__attribute__((noinline)) overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	int ebp=read_ebp();
	int eip=0;
	cprintf("Stack backtrace:\n");
	while(ebp!=0)
	{
		int *ebpptr=(int*)ebp;
		eip=*(ebpptr+1);
		cprintf("  eip %08x  ebp %08x  args",eip,ebp);
		int i=0;//= =! Actually I prefer for(int i=0;i<5;++i)
		while(i<5)
		{
			cprintf(" %08x",*(ebpptr+2+i));
			++i;
		}
		cprintf("\n");
		struct Eipdebuginfo eipinfo;
		if(debuginfo_eip(eip, &eipinfo)==0)
		{
			cprintf("	 %s:%d: %s+%d\n",eipinfo.eip_file,eipinfo.eip_line,eipinfo.eip_fn_name,eip-eipinfo.eip_fn_addr);;;
		}

		ebp=*ebpptr;
	}
	overflow_me();
	cprintf("Backtrace success\n");
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
