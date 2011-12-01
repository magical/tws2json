objects="$1.o solution.o fileio.o err.o bstrlib.o"
redo-ifchange $objects
gcc -o $3 $objects
