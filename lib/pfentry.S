#include <inc/mmu.h>
#include <inc/memlayout.h>

// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.
	//
	// Explanation:
	//   We must prepare the trap-time stack for our eventual return to
	//   re-execute the instruction that faulted.
	//   Unfortunately, we can't return directly from the exception stack:
	//   We can't call 'jmp', since that requires that we load the address
	//   into a register, and all registers must have their trap-time
	//   values after the return.
	//   We can't call 'ret' from the exception stack either, since if we
	//   did, %esp would have the wrong value.
	//   So instead, we push the trap-time %eip onto the *trap-time* stack!
	//   Below we'll switch to that stack and call 'ret', which will
	//   restore %eip to its pre-fault value.
	//
	//   In the case of a recursive fault on the exception stack,
	//   note that the word we're pushing now will fit in the
	//   blank word that the kernel reserved for us.
	//
	// Throughout the remaining code, think carefully about what
	// registers are available for intermediate calculations.  You
	// may find that you have to rearrange your code in non-obvious
	// ways as registers become unavailable as scratch space.
	//
	// LAB 4: Your code here.
	
	//I don't normally keep my psuedocode/thought process in the code, but this one was confusing
	//so we are leaving it for future Zak
	//Psuedocode from discord understanding: Come back to later when not tired!
	//We need to do eip last, which is obvious as it says what to do next. We
	//can do this by putting eip at esp-4. So that way when we are all done we do
	//ret which will pop the eip into place. We want to do that here as after we restore
	//registers we can't use our registers anymore. The ta hinted we can get around our
	//inability to use push with some clever movl instructions. Another issue is manuevering
	//around our two stacks. IMPORTANT. We are on the exception stack currently, and want to retrieve
	//values from the trap-time stack so once we leave the exception stack we go where we want to go


	//Analyzing the utf struct tells us tf_eip is 40 away from esp at this point, and tf_esp is 48
	movl 40(%esp), %eax	//Store tf_eip into eax 
	movl 48(%esp), %ebx	//Grab tf_esp, put it into ebx

	//Make space for eip (recursive case?), put esp back into the stack, store tf_eip into tf_esp-4
	subl $4, %ebx
	movl %ebx, 48(%esp)
	movl %eax, (%ebx)

	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.
	addl $8, %esp	//move to Registers	
	popa			//pop all registers

	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.
	addl $4, %esp	//skip past eip to do last
	popf			//Pop flags

	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
	pop %esp
	
	//lea -4(%esp), %esp //I don't quite understand why adding this line breaks everything, further research required
	//I think it's just because my movl calls above situated things correctly that lea was unneeded?

	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
	ret