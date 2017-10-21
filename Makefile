.phony all:
all: acs

acs: acs.c
	gcc -Wall -pthread acs.c -o ACS

.PHONY clean:
clean:
	-rm -rf *.o *.exe