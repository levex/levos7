LEVOS_SYSROOT=/Users/kurusalevente/dev/levos7/userspace/port/sysroot
LEVOS_INCLUDE=-I$(LEVOS_SYSROOT)/usr/i686-levos/include
LEVOS_CRT=$(LEVOS_SYSROOT)/usr/i686-levos/lib


CC=i686-elf-gcc
BASE_CFLAGS=-nostdlib -ffreestanding -g $(LEVOS_INCLUDE)
CFLAGS=$(BASE_CFLAGS) -L$(LEVOS_CRT) $(LEVOS_SYSROOT)/usr/i686-levos/lib/crt0.o
POST_CFLAGS=-lc -lg -lm -lnosys

TESTS=basic-boot \
      write-robust \
      open-robust \
      open-dir \
      close-robust \
      fork-robust \
      fork-stress \
      getpid-robust \
      uname-match

TEST_RESULTS=$(addsuffix .result,$(TESTS))

.PHONY: all clean test clean_test

all: base.o $(TESTS)

clean:
	-@rm *.output results testoutput test.img base.o $(TESTS) >/dev/null 2>&1 || true
	-@rm qemu.pid >/dev/null 2>&1 || true
	-@diskutil umount mnt >/dev/null 2>&1 || true
	-@rmdir mnt >/dev/null 2>&1 || true

clean_test:
	-@rm results >/dev/null 2>&1 || true
	-@rm qemu.pid >/dev/null 2>&1 || true

test: clean_test $(TEST_RESULTS)
	-@bin/show_results.sh

%.result: %
	@echo "  TEST    $<"
	-@dd if=/dev/zero of=test.img bs=512 count=8192 >/dev/null 2>&1
	-@/usr/local/opt/e2fsprogs/sbin/mkfs.ext2 test.img >/dev/null 2>&1
	-@mkdir -p mnt >/dev/null 2>&1
	-@hdiutil attach test.img -mountpoint $(shell pwd)/mnt >/dev/null 2>&1
	-@cp $< mnt/init >/dev/null 2>&1
	-@sync >/dev/null 2>&1
	-@diskutil umount $(shell pwd)/mnt >/dev/null 2>&1
	-@bin/run_test.sh $<

base.o: base.c
	@echo "  CC      $@"
	@$(CC) $(BASE_CFLAGS) -g -c $< -o $@ 

%: %.c base.o
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -g $< -o $@ base.o $(POST_CFLAGS)
