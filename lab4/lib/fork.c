// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0||(vpd[PDX(addr)] & PTE_P) == 0||(vpt[PGNUM(addr)] & PTE_COW) == 0)
	{
		panic("invalid parameter!\n");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, (void*)PFTEMP, PTE_U | PTE_W | PTE_P);
	if (r < 0)
	{
		panic("sys_page_alloc failed: %e", r);
	}
	void* va = (void*)ROUNDDOWN(addr, PGSIZE);
	memmove((void*)PFTEMP, va, PGSIZE);
	r = sys_page_map(0, (void*)PFTEMP, 0, va, PTE_U | PTE_W | PTE_P);
	if (r < 0)
	{
		panic("sys_page_map failed: %e", r);
	}
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void* addr = (void*)(pn*PGSIZE);
	if ((uint32_t)addr >= UTOP)
	{
		cprintf("lib/fork.c:duppage: duplicate page above UTOP!");
		return -1;
	}
	if ((vpt[PGNUM(addr)] & PTE_P) ==0)
	{
		cprintf("lib/fork.c:duppage: page table not present!");
		return -1;
	}
	if ((vpd[PDX(addr)] & PTE_P) == 0)
	{
		cprintf("[lib/fork.c duppage]: page directory not present!");
		return -1;
	}
	if (vpt[PGNUM(addr)] & (PTE_W | PTE_COW)) 
	{
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P | PTE_COW);
		if (r < 0) 
		{
			cprintf("lib/fork.c:duppage: sys_page_map failed1!\n");
			return -1;
		}
		r = sys_page_map(0, addr, 0, addr, PTE_U | PTE_P | PTE_COW);
		if (r < 0)
		{
			cprintf("lib/fork.c:duppage: sys_page_map failed2!\n");
			return -1;
		}
	} else 
	{
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P);
		if (r < 0) 
		{
			cprintf("lib/fork.c:duppage:sys_page_map failed3!\n", r);
			return -1;
		}
	}
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	extern void _pgfault_upcall (void);
	int i;
	int pagei;
	set_pgfault_handler(pgfault);
	envid_t childid;
	childid = sys_exofork();
	if (childid < 0) 
	{
		panic("lib:fork.c:fork():fork does not success:%e",childid);
	}
	else if (childid == 0) 
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	for(pagei = UTEXT/PGSIZE; pagei < UTOP/PGSIZE; pagei++) 
	{
		if (pagei != (UXSTACKTOP-PGSIZE) / PGSIZE)
		{
			if (((vpd[pagei/NPTENTRIES] & PTE_P) != 0) && ((vpt[pagei] & PTE_P) != 0) && ((vpt[pagei] & PTE_U) != 0)) 
			{
				duppage(childid, pagei);
			}
		}
	}
	i = sys_page_alloc(childid,(void *)(UXSTACKTOP-PGSIZE),PTE_U|PTE_W|PTE_P);
	if(i < 0)
	{
		panic("lib:fork.c:fork(): exception stack error %e\n",i);	
	}
	i = sys_env_set_pgfault_upcall(childid, (void *)_pgfault_upcall);
	if(i < 0)
	{
		panic("lib:fork.c:fork(): pgfault_upcall error %e\n",i);
	}
	i = sys_env_set_status(childid,ENV_RUNNABLE);
	if(i < 0)
	{
		panic("lib:fork.c:fork(): status error %e\n",i);
	}
	return childid;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
