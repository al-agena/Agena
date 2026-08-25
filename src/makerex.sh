#! /bin/sh
# compile and install skript for the plus package for Linux
# execute this batch file in the .../agena/src folder by typing:
# sh makepluslinux.sh

export OPTIONS="-O2 -Wno-attributes -fgnu89-inline -DLUA_USE_LINUX -DOPENSUSE -shared -I../src -L../src ../src/libagena.a"
export EXPORTTO="../lib"
export MYFLAGS="-fgnu89-inline -DOPENSUSE"

# delete *.o files not deleted by make clean
for i in regex.o
do
   if [ -f i ]; then
      rm i
   fi
done

printf "Compiling regex ... "
gcc $OPTIONS -DPCRE2_CODE_UNIT_WIDTH=8 -o regex.so regex.c regex_f.c regex.h regexcom.c regexcom.h regexalg.h -lagena -lpcre2-posix -lpcre2-8
strip regex.so
mv -f regex.so $EXPORTTO
printf "done.\n"

echo Installing net library into /lib folder ...
echo All done.

