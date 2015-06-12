redo-ifchange $2.c
gcc -O2 -flto -g -c -o "$3" "$2.c" -Wall
gcc -MM "$2.c" | read headers
redo-ifchange ${headers#*:}
