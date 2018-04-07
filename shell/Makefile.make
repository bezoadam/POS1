NAME=all

LOGIN=xvones02
PROJ=shell
CC=gcc
CFLAGS=-Wall -g -O -ansi -pedantic -lrt -pthread

all: clean compile

compile:
	$(CC) $(CFLAGS) $(PROJ).c -o $(PROJ)
clean:
	rm -f $(PROJ) core*
run:
	./$(PROJ)
zip:
	zip -r $(LOGIN).zip $(PROJ).c Makefile

