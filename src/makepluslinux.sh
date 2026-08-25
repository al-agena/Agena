#!/bin/sh
# compile and install skript for the plus package for Linux
# execute this batch file in the .../agena/src folder by typing:
# sh makepluslinux.sh
export OPTIONS="-DLUA_USE_LINUX -DDEBIAN -Wall -O2 -g -fgnu89-inline -shared -I../src -L../src ../src/libagena.a"
export MYFLAGS="-fgnu89-inline"
export DDMATHFLAGS="-fno-builtin -ffloat-store -fno-unsafe-math-optimizations"

export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in abci.o ads.o fractals.o phq.o net.o interp.o moon.o sunriset.o astro.o skycrane.o iniparse.o hashes.o fastmath.o \
	gmp.o mpfr.o luasys.o charbuf.o com.o heaps.o regex.o testlib.o rbtree.o regex.o testlib.o double.o bloom.o \
	cuckoo.o clock.o zx.o bimaps.o
do
   if [ -f i ]; then
      rm i
   fi
done

printf "Compiling abci ... "
gcc $OPTIONS -o abci.so abci.c
strip abci.so
mv -f abci.so $EXPORTTO
printf "done.\n"

printf "Compiling ADS ... "
gcc -O2 $MYFLAGS -fpic -c -o ads.o ads.c
gcc $OPTIONS -fpic -o ads.so ads.o vecoff64.o
strip ads.so
mv -f ads.so $EXPORTTO
printf "done.\n"

if [ -f ../phq/phq.c ]; then
   printf "Compiling phonetiQs ... "
   gcc $OPTIONS -o phq.so ../phq/phq.c
   strip phq.so
   mv -f phq.so ../phq
   printf "done.\n"
fi

printf "Compiling fractals ... "
gcc $OPTIONS -o fractals.so fractals.c
strip fractals.so
mv -f fractals.so $EXPORTTO
printf "done.\n"

printf "Compiling astro ... "
gcc -O2 $MYFLAGS -fpic -c -o astro.o astro.c
gcc -O2 $MYFLAGS -fpic -c -o sunriset.o sunriset.c
gcc -O2 $MYFLAGS -fpic -c -o moon.o moon.c
gcc $OPTIONS -fpic -o astro.so astro.o sunriset.o moon.o sofa.o
strip astro.so
mv -f astro.so $EXPORTTO
printf "done.\n"

printf "Compiling net ... "
gcc $OPTIONS -o net.so net.c
strip net.so
mv -f net.so $EXPORTTO
printf "done.\n"

printf "Compiling skycrane ... "
gcc $OPTIONS -o skycrane.so skycrane.c
strip skycrane.so
mv -f skycrane.so $EXPORTTO
printf "done.\n"

printf "Compiling hashes ... "
gcc $OPTIONS -o hashes.so hashes.c
strip hashes.so
mv -f hashes.so $EXPORTTO
printf "done.\n"

printf "Compiling zx ... "
gcc $OPTIONS -o zx.so zx.c
strip zx.so
mv -f zx.so $EXPORTTO
printf "done.\n"

printf "Compiling bloom ... "
gcc $OPTIONS -o bloom.so bloom.c
strip bloom.so
mv -f bloom.so $EXPORTTO
printf "done.\n"

printf "Compiling cuckoo ... "
gcc $OPTIONS -o cuckoo.so cuckoo.c
strip cuckoo.so
mv -f cuckoo.so $EXPORTTO
printf "done.\n"

printf "Compiling clock ... "
gcc $OPTIONS -o clock.so clock.c
strip clock.so
mv -f clock.so $EXPORTTO
printf "done.\n"

printf "Compiling fastmath ... "
gcc $OPTIONS -Wno-strict-aliasing -o fastmath.so fastmath.c
strip fastmath.so
mv -f fastmath.so $EXPORTTO
printf "done.\n"

printf "Compiling iconv ... "
gcc $OPTIONS -o iconv.so iconv.c -liconv
strip iconv.so
mv -f iconv.so $EXPORTTO
printf "done.\n"

printf "Compiling gmp ... "
gcc $OPTIONS -o gmp.so gmp.c -lgmp
strip gmp.so
mv -f gmp.so $EXPORTTO
printf "done.\n"

printf "Compiling mpfr ... "
gcc $OPTIONS -o mpfr.so mpfr.c -lmpfr -lgmp
strip mpfr.so
mv -f mpfr.so $EXPORTTO
printf "done.\n"

printf "Compiling com ... "
gcc -O2 $MYFLAGS -DPLUS -fpic -c -o charbuf.o charbuf.c
gcc -O2 $MYFLAGS -fpic -c -o luasys.o luasys.c
gcc -O2 $MYFLAGS -fpic -c -o com.o com.c
gcc $OPTIONS -fpic -o com.so com.o charbuf.o luasys.o
strip com.so
mv -f com.so $EXPORTTO
printf "done.\n"

printf "Compiling heaps ... "
gcc $OPTIONS -o heaps.so heaps.c
strip heaps.so
mv -f heaps.so $EXPORTTO
printf "done.\n"

printf "Compiling rbtree ... "
gcc $OPTIONS -Wno-maybe-uninitialized -o rbtree.so rbtree.c
strip rbtree.so
mv -f rbtree.so $EXPORTTO
printf "done.\n"

printf "Compiling bimaps ... "
gcc $OPTIONS -Wno-maybe-uninitialized -o bimaps.so bimaps.c
strip bimaps.so
mv -f bimaps.so $EXPORTTO
printf "done.\n"

printf "Compiling testlib ... "
gcc $OPTIONS -o testlib.so testlib.c
strip testlib.so
mv -f testlib.so $EXPORTTO
printf "done.\n"

printf "Compiling double ... "
gcc $OPTIONS -o double.so double.c
strip double.so
mv -f double.so $EXPORTTO
printf "done.\n"

echo Installing all libraries into /lib folder ...
echo All done.

