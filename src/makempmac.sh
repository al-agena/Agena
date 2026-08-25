#! /bin/sh
# compile and install skript for the plus package for Linux
# execute this batch file in the .../agena/src folder by typing:
# sh makepluslinux.sh
#export OPTIONS="-DLUA_USE_LINUX -Wall -O2 -shared -fgnu89-inline -I../src -L../src ../src/libagena.a"

export OPTIONS="-fno-common -bundle -undefined dynamic_lookup -force_cpusubtype_ALL -mmacosx-version-min=10.4 -I../src -I/usr/local/include -L../src"

export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in mp.o
do
   if [ -f i ]; then
      rm i
   fi
done

printf "Compiling mp ... "
gcc $OPTIONS -fPIC -std=c99 -o mp.so mp.c -lgmp # /usr/local/lib/libgmp.a
strip mp.so
mv -f mp.so $EXPORTTO
printf "done.\n"

echo Installing mp library into /lib folder ...
echo All done.
