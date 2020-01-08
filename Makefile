
all: tws2json

tws2json: tws2json.o solution.o fileio.o err.o bstrlib.o
	$(CC) -O2 -fwhole-program -flto -o $@ $^

%.o: %.c Makefile
	$(CC) -O2 -flto -g -c -o $@ $< -Wall

# :read !gcc -MM *.c
bstrlib.o: bstrlib.c bstrlib.h
err.o: err.c err.h
fileio.o: fileio.c err.h fileio.h
solution.o: solution.c err.h fileio.h solution.h
tws2json.o: tws2json.c bstrlib.h solution.h fileio.h err.h version.h

clean:
	rm tws2json tws2json.o solution.o fileio.o err.o bstrlib.o
