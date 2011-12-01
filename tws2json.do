objects="$1.o solution.o fileio.o err.o bstrlib.o"
redo-ifchange $objects
gcc -O2 -fwhole-program -flto -o $3 $objects
