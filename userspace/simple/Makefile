#LEVOS_SYSROOT=/Users/kurusalevente/dev/levos7/userspace/port/sysroot
#LEVOS_INCLUDE=-I$(LEVOS_SYSROOT)/usr/i686-levos/include
#LEVOS_CRT=$(LEVOS_SYSROOT)/usr/i686-levos/lib


#CC=i686-elf-gcc
CC=i686-levos-gcc
CXX=i686-levos-g++
#CFLAGS=-nostdlib -ffreestanding -g $(LEVOS_INCLUDE) -L$(LEVOS_CRT) $(LEVOS_SYSROOT)/usr/i686-levos/lib/crt0.o
#POST_CFLAGS=-lc -lg -lm -lnosys

.PHONY: all clean copy

APPS=echo lsh cat ls memstat uname mkdir alarm-test testcpp scpp constr mmap-test fb-test wc
all: $(APPS)

copy:
	cp echo /Volumes/ext2/bin/echo
	cp lsh /Volumes/ext2/init
	cp cat /Volumes/ext2/bin/cat
	cp ls /Volumes/ext2/bin/ls
	cp memstat /Volumes/ext2/bin/memstat
	cp uname /Volumes/ext2/bin/uname
	cp mkdir /Volumes/ext2/bin/mkdir
	cp alarm-test /Volumes/ext2/bin/alarm-test
	cp testcpp /Volumes/ext2/bin/testcpp
	cp scpp /Volumes/ext2/bin/scpp
	cp constr /Volumes/ext2/bin/constr
	cp mmap-test /Volumes/ext2/bin/mmap-test
	cp fb-test /Volumes/ext2/bin/fb-test
	cp wc /Volumes/ext2/bin/wc


clean:
	-@rm quiet $(APPS) >/dev/null 2>&1 || true

%: %.c
	@echo "   CC     $@"
	@$(CC) $(CFLAGS) -g $< -o $@ $(POST_CFLAGS)

%: %.cpp
	@echo "   CXX    $@"
	@$(CXX) $(CXXFLAGS) -g $< -o $@ $(POST_CXXFLAGS)
