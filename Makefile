
all: Makefile neptune_read neptune_dump


neptune_read: neptune_read.c neptune_rec.c neptune_rec.h
	gcc -std=c99 -Wall -o neptune_read neptune_read.c neptune_rec.c -lm


neptune_dump: neptune_dump.c neptune_rec.c neptune_rec.h
	gcc -std=c99 -Wall -o neptune_dump neptune_dump.c neptune_rec.c -lm


distclean:
	-rm -f neptune_read
	-rm -f neptune_dump

