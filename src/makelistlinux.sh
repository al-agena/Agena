#! /bin/sh
# compile and install skript for the plus package for Linux
# execute this batch file in the .../agena/src folder by typing:
# sh makepluslinux.sh
export OPTIONS="-DLUA_USE_LINUX -Wall -O2 -shared -I../src -L../src ../src/libagena.a"
export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in slist.o
do
   if [ -f i ]; then
      rm i
   fi
done

printf "Compiling slist ... "
gcc $OPTIONS -o slist.so slist.c
strip slist.so
mv -f slist.so $EXPORTTO
printf "done.\n"

echo Installing slist library into /lib folder ...
echo All done.

