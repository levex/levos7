.globl arch_spin_lock
arch_spin_lock:
.retry:
    movl %esp, %eax
    addl $4, %eax
    movl (%eax), %eax
    lock bts $0, (%eax)
    jc .spin_wait
    ret
.spin_wait:
    testl $1, (%eax)
    jnz .spin_wait
    jmp .retry
 
.globl arch_spin_unlock
arch_spin_unlock:
    movl %esp, %eax
    addl $4, %eax
    movl (%eax), %eax
    lock btr $0, (%eax)
    ret
