# void thread_start(struct thread * old, struct thread * new) 

.globl thread_start

thread_start:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq %rsp, (%rdi)#Save current stack pointer in old threads table entry
	movq (%rsi), %rsp #Load the stack pointer from the new thread into rsp
	jmp thread_wrap
