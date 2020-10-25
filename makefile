CC = cc -pedantic -std=c11
PTCC = cc -pedantic -pthread -g
PTRUN = srun -c
LINKS = -lm

all: run

run: Programming1.c
	$(PTCC) -o run Programming1.c $(LINKS)

clean :
	rm -f run
