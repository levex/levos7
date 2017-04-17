KERN_NAME=kernel.img
KERN_SYM=kernel.sym
LEVOS_CONFIG_FILE=LConfig
ARCH=x86

QEMU_OPTS=-serial stdio -no-reboot


# -- end config

SOURCEDIR=.

C_SOURCES  := $(shell find $(SOURCEDIR) -name '*.c' -not -path "$(SOURCEDIR)/userspace/*")
AS_SOURCES := $(shell find $(SOURCEDIR) -name '*.S' -not -path "$(SOURCEDIR)/userspace/*")
OBJS = $(sort $(subst .c,.o,$(C_SOURCES)))
OBJS += $(sort $(subst .S,.o,$(AS_SOURCES)))

all: $(KERN_NAME) $(KERN_SYM)

$(KERN_SYM): $(KERN_NAME)
	@$(OBJCOPY) --only-keep-debug $(KERN_NAME) $(KERN_SYM)
	@echo "  STRIP    $(KERN_SYM)"

start: $(KERN_NAME)
	@echo "  QEMU     $(KERN_NAME)"
	@$(QEMU) -kernel $(KERN_NAME) $(QEMU_OPTS)

debug: $(KERN_NAME)
	@echo "  QEMU [D] $(KERN_NAME)"
	@$(QEMU) -s -S -kernel $(KERN_NAME) $(QEMU_OPTS)

include arch/$(ARCH)/Makefile

LEVOS_CONFIG_DEFS=$(shell ./gen_config_line.py < $(LEVOS_CONFIG_FILE))

CFLAGS += -Iinclude -D__LEVOS_ARCH_$(ARCH)__ $(LEVOS_CONFIG_DEFS) -fno-omit-frame-pointer
CFLAGS += -g

%.o: %.c
	@echo "  CC       $@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@echo "  AS       $@"
	@$(AS) -g -c $< -o $@

depend:
	makedepend -f Make.dep -Iinclude $(C_SOURCES)

$(KERN_NAME): $(OBJS)
	@echo "  LD       $@"
	@$(LD) $(LDFLAGS) -T arch/$(ARCH)/linker.ld -o $(KERN_NAME) $(OBJS)

preprocess: $(KERN_NAME)
	@echo "  PP       $(KERN_NAME)"
	@$(CC) $(CFLAGS) -E $(OBJS:.o=.c) 
	# -T arch/$(ARCH)/linker.ld $(OBJS)

clean:
	-@rm $(OBJS) >/dev/null 2>&1|| true
	-@rm $(KERN_NAME) >/dev/null 2>&1 || true

include Make.dep
