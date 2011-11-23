redo-ifchange $1.c
gcc -g -c -o "$3" "$1.c" -Wall
gcc -MM "$1.c" | read headers
redo-ifchange ${headers#*:}
