BINS = BBD9000init BBD9000timer BBD9000fsm BBD9000SmartIO BBD9000server BBD9000twilight  \
BBD9000authorizeDotNet BBD9000cell BBD9000_JSON serial8N1

all: $(BINS)


BBD9000init: BBD9000init.c BBD9000cfg.o BBD9000mem.h BBD9000cfg.h
	gcc -Wall -o BBD9000init BBD9000cfg.o BBD9000init.c -lconfuse
	strip BBD9000init

BBD9000cfg.o: BBD9000cfg.c BBD9000mem.h BBD9000cfg.h
	gcc -Wall -c BBD9000cfg.c

BBD9000SmartIO: BBD9000SmartIO.c BBD9000mem.h
	gcc -Wall -o BBD9000SmartIO BBD9000SmartIO.c
	strip BBD9000SmartIO

BBD9000twilight: BBD9000twilight.c BBD9000mem.h
	gcc -Wall -o BBD9000twilight BBD9000twilight.c -lm
	strip BBD9000twilight

BBD9000server: BBD9000server.c BBD9000cfg.o BBD9000mem.h
	gcc -Wall -o BBD9000server BBD9000cfg.o BBD9000server.c -lcrypto -lcurl -lconfuse -lm
	strip BBD9000server

BBD9000authorizeDotNet: BBD9000authorizeDotNet.c BBD9000mem.h BBD9000cfg.h
	gcc -Wall -o BBD9000authorizeDotNet BBD9000cfg.o BBD9000authorizeDotNet.c -lcurl -lconfuse
	strip BBD9000authorizeDotNet

BBD9000timer: BBD9000timer.c BBD9000mem.h
	gcc -Wall -g -o BBD9000timer BBD9000timer.c -ltimer_q
	strip BBD9000timer

BBD9000fsm: BBD9000fsm.c BBD9000cfg.o BBD9000mem.h BBD9000cfg.h
	gcc -Wall -o BBD9000fsm BBD9000cfg.o BBD9000fsm.c -lcrypt -lconfuse
	strip BBD9000fsm

BBD9000cell: BBD9000cell.c
	gcc -Wall -o BBD9000cell BBD9000cell.c
	strip BBD9000cell

BBD9000cellTest: BBD9000cellTest.c
	gcc -Wall -o BBD9000cellTest BBD9000cellTest.c
	strip BBD9000cellTest

BBD9000_JSON: BBD9000_JSON.c BBD9000mem.h
	gcc -Wall -o BBD9000_JSON BBD9000_JSON.c
	strip BBD9000_JSON
	gcc -Wall -DDEVEL -o BBD9000_JSON_DEVEL BBD9000_JSON.c
	strip BBD9000_JSON_DEVEL

serial8N1: serial8N1.c
	gcc -Wall -o serial8N1 serial8N1.c
	strip serial8N1

install:
	cp $(BINS) ../BBD9000

clean:
	rm -f *.o
	rm -f $(BINS)
