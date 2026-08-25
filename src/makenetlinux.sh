#! /bin/sh
# compile and install skript for the plus package for Linux
# execute this batch file in the .../agena/src folder by typing:
# sh makepluslinux.sh
export OPTIONS="-DLUA_USE_LINUX -Wall -O2 -shared -I../src -L../src ../src/libagena.a"
export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in net.o
do
   if [ -f i ]; then
      rm i
   fi
done

printf "Compiling net ... "
gcc $OPTIONS -o net.so net.c
strip net.so
mv -f net.so $EXPORTTO
printf "done.\n"

echo Installing net library into /lib folder ...
echo All done.

