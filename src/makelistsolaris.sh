printf "Compiling slist ... "
gcc $OPTIONS -c -o slist.o slist.c
gcc -O -shared -fpic -o slist.so slist.o # -lsocket -lnsl
printf "done.\n"

