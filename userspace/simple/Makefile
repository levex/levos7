#LEVOS_SYSROOT=/Users/kurusalevente/dev/levos7/userspace/port/sysroot
#LEVOS_INCLUDE=-I$(LEVOS_SYSROOT)/usr/i686-levos/include
#LEVOS_CRT=$(LEVOS_SYSROOT)/usr/i686-levos/lib


#CC=i686-elf-gcc
CC=i686-levos-gcc
#CFLAGS=-nostdlib -ffreestanding -g $(LEVOS_INCLUDE) -L$(LEVOS_CRT) $(LEVOS_SYSROOT)/usr/i686-levos/lib/crt0.o
#POST_CFLAGS=-lc -lg -lm -lnosys

.PHONY: all clean

all: hello sh echo


clean:
	-@rm quiet hello sh >/dev/null 2>&1 || true

%: %.c
	@echo "   CC     $@"
	@$(CC) $(CFLAGS) -g $< -o $@ $(POST_CFLAGS)
