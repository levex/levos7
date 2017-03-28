#ifndef __LEVOS_COMPILER_H
#define __LEVOS_COMPILER_H

#define __noreturn __attribute__((noreturn))
#define __unused __attribute__((unused))
#define __page_align __attribute__((aligned(4096)))
#define __align(X) __attribute__((aligned(X)))
#define __packed __attribute__((packed))

#define __not_reached() __builtin_unreachable()

#define ___stringify(x) #x
#define __stringify(x) ___stringify(x)

#endif
