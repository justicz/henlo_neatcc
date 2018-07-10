# output architecture: x64, x86, arm, henlo
OUT = henlo

CC = gcc
CFLAGS = -Wall -O2 -DNEATCC_`echo $(OUT) | tr xarmhenlo XARMHENLO`
LDFLAGS =

OBJS = ncc.o tok.o out.o cpp.o gen.o int.o reg.o mem.o $(OUT).o

all: ncc
%.o: %.c ncc.h
	$(CC) -c $(CFLAGS) $<
ncc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
clean:
	rm -f *.o ncc
