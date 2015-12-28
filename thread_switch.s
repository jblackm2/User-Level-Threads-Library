# void thread_switch(struct thread * old, struct thread * new) 

.globl thread_switch

thread_switch:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq %rsp, (%rdi)#Save current stack pointer in old threads table entry
	movq (%rsi), %rsp #Load the stack pointer from the new thread into rsp
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	ret
