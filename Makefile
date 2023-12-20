#
#Makefile for ProDOS Emulator v0.1
#

WCC=i686-w64-mingw32-gcc
DOCKER=podman

all : prodos

# Linux doesn't seem to like Randy Frank's beep code,
# if you're compiling on something else, you could try not defining NOBEEP
OPT = -O2 -g

# Necessary libraries
LIB = -lcurses -ltermcap

# Object files
OBJ = main.o 6502.o mega2.o debug.o prodos.o

# Build modules from source:
main.o: main.c apple.h
	cc -c  $(OPT) main.c
6502.o: 6502.c apple.h
	cc -c  $(OPT) 6502.c
mega2.o: mega2.c apple.h
	cc -c  $(OPT) mega2.c
debug.o: debug.c apple.h
	cc -c  $(OPT) debug.c
prodos.o: prodos.c apple.h
	cc -c  $(OPT) prodos.c

# Build the executale
prodos  : $(OBJ)
	cc $(OPT) -rdynamic -o prodos $(OBJ) $(LIB)

prodos.exe:
	$(WCC) -o prodos.exe mega2.c debug.c prodos.c 6502.c main.c -L/usr/local/crooss-tools/lib -lcurses

install:
	install -b prodos ~/bin

# Clean up
clean:
	rm *.o *~

build:
	$(DOCKER) build -t prodos .

run:
	$(DOCKER) run -it --rm -p 1977:1977/udp prodos
