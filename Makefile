KERN_NAME=kernel.img
ARCH=x86

QEMU_OPTS=-serial stdio -no-reboot


# -- end config

SOURCEDIR=.

C_SOURCES  := $(shell find $(SOURCEDIR) -name '*.c')
AS_SOURCES := $(shell find $(SOURCEDIR) -name '*.S')
OBJS = $(subst .c,.o,$(C_SOURCES))
OBJS += $(subst .S,.o,$(AS_SOURCES))

all: $(KERN_NAME)

start: $(KERN_NAME)
	@echo "  QEMU     $(KERN_NAME)"
	@$(QEMU) -kernel $(KERN_NAME) $(QEMU_OPTS)

debug: $(KERN_NAME)
	@echo "  QEMU [D] $(KERN_NAME)"
	@$(QEMU) -s -S -kernel $(KERN_NAME) $(QEMU_OPTS)

include arch/$(ARCH)/Makefile

CFLAGS += -Iinclude -D__LEVOS_ARCH_$(ARCH)__
CFLAGS += -g

%.o: %.c
	@echo "  CC       $@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@echo "  AS       $@"
	@$(AS) -c $< -o $@

$(KERN_NAME): $(OBJS)
	@echo "  LD       $@"
	@$(LD) $(LDFLAGS) -T arch/$(ARCH)/linker.ld -o $(KERN_NAME) $(OBJS)

preprocess: $(KERN_NAME)
	@echo "  PP       $(KERN_NAME)"
	@$(CC) $(CFLAGS) -E $(OBJS:.o=.c) 
	# -T arch/$(ARCH)/linker.ld $(OBJS)

clean:
	-rm $(OBJS)
	-rm $(KERN_NAME)
