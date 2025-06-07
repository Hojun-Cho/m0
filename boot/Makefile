CC = mipsel-elf-gcc
CPP = mipsel-elf-cpp
LD = mipsel-elf-ld.bfd
AS = mipsel-elf-as
OBJCP = mipsel-elf-objcopy

CFLAGS = -ffunction-sections -fdata-sections  -W -Wall -Wextra -Wundef -Os -nostdlib -ffreestanding -Wstrict-prototypes -pipe -std=gnu99 -funit-at-a-time -fno-delete-null-pointer-checks -fno-strict-overflow -march=mips32r2 -mno-mips16 -mno-long-calls -Umips -ffunction-sections -msoft-float -G 0 -Wno-parentheses -fomit-frame-pointer -Wno-pointer-sign -Wno-override-init
LDFLAGS = -T ld.script

PROG = spl
SRCS = begin.S main.c
OBJS = begin.o main.o

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG} ${OBJS}
	$(OBJCP) -O binary $(PROG) $(PROG).bin

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<	

%.o : %.S
	$(CC) $(CFLAGS) -o $@ -c $<	


clean:
	rm -f $(PROG) $(PROG).bin *.o


