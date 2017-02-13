
all: Makefile neptune_read neptune_dump


neptune_read: neptune_read.c neptune_rec.c neptune_rec.h
	gcc -Wall -lm -o neptune_read neptune_read.c neptune_rec.c


neptune_dump: neptune_dump.c neptune_rec.c neptune_rec.h
	gcc -Wall -lm -o neptune_dump neptune_dump.c neptune_rec.c


distclean:
	-rm -f neptune_read
	-rm -f neptune_dump

