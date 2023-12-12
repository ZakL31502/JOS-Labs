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
	void* fault_addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	envid_t envid = sys_getenvid();

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	//Check FEX_WR to see if it was a write attempt, and to check if page table index was COW
	if(!(err & FEC_WR) || !(uvpt[PGNUM(fault_addr)] & PTE_COW)){
		panic("pgfault: Shouldn't have been trying to access this!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	//Allocate page at PFTEMPT
	int error = sys_page_alloc(envid, PFTEMP, PTE_U | PTE_P | PTE_W);
	if(error < 0){
		panic("pgfault: page_alloc returned %e", error);
	}

	//memcpy
	memcpy(PFTEMP, ROUNDDOWN(fault_addr, PGSIZE), PGSIZE);

	//map page at fault addr from PFTEMP
	error = sys_page_map(envid, PFTEMP, envid, ROUNDDOWN(fault_addr, PGSIZE), PTE_U | PTE_P | PTE_W);
	if(error < 0){
		panic("pgfault: page_map returned %e", error);
	}

	//unmap(PFTEMP)
	error = sys_page_unmap(envid, PFTEMP);
	if(error < 0){
		panic("pgfault: page_unmap returned %e", error);
	}
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
	// int r;
	// LAB 4: Your code here.

	//get pte for permissions
	pte_t pte = uvpt[pn];
	int error = 0;
	envid_t p_envid = sys_getenvid();
	//writtable mappings
	if(pte & PTE_W || pte & PTE_COW){
		int perm = PTE_P | PTE_U | PTE_COW;
		//Map child
		error = sys_page_map(p_envid, (void*)(pn * PGSIZE), envid, (void*)(pn * PGSIZE), perm);
		if(error < 0){
			panic("duppage: sys_page map failed! %e", error);
		}

		//remap parent with new permissions!
		error = sys_page_map(p_envid, (void*)(pn * PGSIZE), p_envid, (void*)(pn * PGSIZE), perm);
		if(error < 0){
			panic("duppage: sys_page map failed! %e", error);
		}
	}
	//Read only mappings
	else{
		int perm = PTE_P | PTE_U;
		//Only map child as read only if read only
		error = sys_page_map(p_envid, (void*)(pn * PGSIZE), envid, (void*)(pn * PGSIZE), perm);
		if(error < 0){
			panic("duppage: sys_page map failed! %e", error);
		}
	}

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
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	//taking inspiration from dumbfork:
	//Set up handler, create a child
	set_pgfault_handler(&pgfault);
	envid_t envid = sys_exofork();

// 	//Panic on error
	if(envid < 0){
		panic("fork: sys_exofork returned %e", envid);
	}
// 	//It's the child process
	if(envid == 0){
		thisenv = &envs[ENVX(sys_getenvid())]; 
		return envid;
	}

// 	//Its the parent process
// 	//copy all address space
	for(uintptr_t i = 0; i < UTOP; i += PGSIZE){
		//duppage handles if PTE_W/PTE_COW or PTE_R, we just need to check if present!
		//Needed an additional condition: we need to do everything below UTOP, but can't do UXSTACKTOP-PGSIZE as that is handled below
		if((uvpd[PDX(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_P) && i != (UXSTACKTOP - PGSIZE)){
			duppage(envid, PGNUM(i));
		}
	}

	
// 	//Allocate a fresh page for the Exception Stack at USTACKTOP-PGSIZE
	int error = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_P | PTE_U | PTE_W);
	if(error < 0){
		panic("fork: page_alloc returned %e", error);
	}

// 	//set up page fault handler for child
	error = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
	if(error < 0){
		panic("fork: set_pgfault_upcall returned %e", error);
	}

// 	//Mark child as runnable and return
	error = sys_env_set_status(envid, ENV_RUNNABLE);
	if(error < 0){
		panic("fork: set_status Runnable returned %e", error);
	}

	return envid;
}


// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
