#include <levos/types.h>
#include <levos/arch.h>

#if 0
inline void
arch_atomic_or(uint32_t *l, uint32_t m)
{
	/* This is equivalent to `b->bits[idx] |= mask' except that it
		is guaranteed to be atomic on a uniprocessor machine.  See
		the description of the OR instruction in [IA32-v2b]. */
	asm ("orl %1, %0" : "=m" (l) : "r" (m) : "cc");
}

inline void
arch_atomic_and(uint32_t *l, uint32_t m)
{
	/* This is equivalent to `b->bits[idx] &= ~mask' except that it
		is guaranteed to be atomic on a uniprocessor machine.  See
		the description of the AND instruction in [IA32-v2a]. */
	asm ("andl %1, %0" : "=m" (l) : "r" (m) : "cc");
}

inline void
arch_atomic_xor(uint32_t *l, uint32_t m)
{
	/* This is equivalent to `b->bits[idx] ^= mask' except that it
		is guaranteed to be atomic on a uniprocessor machine.  See
		the description of the XOR instruction in [IA32-v2b]. */
	asm ("xorl %1, %0" : "=m" (l) : "r" (m) : "cc");
}
#endif
