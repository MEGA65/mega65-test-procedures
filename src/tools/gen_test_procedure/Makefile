COPT=	-Wall -g -std=gnu99
CC=	gcc

all:	test_procedure_format

test_procedure_format:	test_procedure_format.c Makefile
	$(warning =============================================================)
	$(warning ~~~~~~~~~~~~~~~~> Making: $@)
	$(CC) $(COPT) -o test_procedure_format test_procedure_format.c

clean:
	rm -f test_procedure_format
