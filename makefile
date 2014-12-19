all:
	gcc -ansi -Wall -o aout main.c
clean:
	rm -rf *.o *.c~ aout makefile~
