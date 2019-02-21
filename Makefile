
all: fs-miei01


fs-miei01: shell.o fs.o disk.o
	gcc -g shell.o fs.o disk.o -lm -o fs-miei01

shell.o: shell.c
	gcc -Wall shell.c -c -o shell.o -g

fs.o: fs.c fs.h
	gcc -Wall fs.c -c -o fs.o -g

disk.o: disk.c disk.h
	gcc -Wall disk.c -c -o disk.o -g

clean:
	rm fs-miei01 disk.o fs.o shell.o
