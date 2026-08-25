#!/bin/sh
# compile and install skript for the plus package for Intel Debian
# execute this batch file in the .../agena/src folder by typing:
# sh makeplusstretch.sh

# Configure gmp and mpfr on 32-bit Linux as follows to prevent Valgrind from crashing:
# GMP:
# export CFLAGS="-O2 -march=i686 -mno-avx -mno-fma"
# ./configure ABI=32 CFLAGS="-O2 -march=i686 -mno-avx -mno-fma" --prefix=/usr/local --disable-assembly
# MPFR:
# ./configure --prefix=/usr/local --disable-assembly

export OPTIONS="-DLUA_USE_LINUX -DDEBIAN -Wall -O2 -g -shared -fgnu89-inline -fPIC -I../src -L../src"
export MYFLAGS="-fgnu89-inline -fPIC"

export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in abci.o ads.o astro.o charbuf.o com.o cordic.o curses.o \
    dcastro.o double.o fastmath.o fractals.o gmp.o iconv.o interp.o \
    luasys.o miniz.o minizip.o moon.o mpfr.o net.o phq.o skycrane.o \
    sqlite.o sqlite3.o sunriset.o testlib.o zx.o ddhash.o strhash.o \
    strmap.o intmap.o
do
  if [ -f "$i" ]; then
    rm "$i"
  fi
done

printf "Compiling abci ... "
gcc $OPTIONS -o abci.so abci.c
strip abci.so
mv -f abci.so $EXPORTTO
printf "done.\n"

printf "Compiling ADS ... "
gcc -O2 $MYFLAGS -fpic -c -o vecoff64.o vecoff64.c
gcc -O2 $MYFLAGS -fpic -c -o ads.o ads.c
gcc $OPTIONS -fpic -o ads.so ads.o vecoff64.o
strip ads.so
mv -f ads.so $EXPORTTO
printf "done.\n"

printf "Compiling astro ... "
gcc -O2 $MYFLAGS -fpic -c -o astro.o astro.c
gcc -O2 $MYFLAGS -fpic -c -o sunriset.o sunriset.c
gcc -O2 $MYFLAGS -fpic -c -o moon.o moon.c
gcc -O2 $MYFLAGS -fpic -c -o dcastro.o dcastro.c
gcc $OPTIONS -fpic -o astro.so astro.o sunriset.o moon.o sofa.o dcastro.o
strip astro.so
mv -f astro.so $EXPORTTO
printf "done.\n"

printf "Compiling com ... "
gcc -O2 -DPLUS $MYFLAGS -fpic -c -o charbuf.o charbuf.c
gcc -O2 $MYFLAGS -fpic -c -o luasys.o luasys.c
gcc -O2 $MYFLAGS -fpic -c -o com.o com.c
gcc $OPTIONS -fpic -o com.so com.o charbuf.o luasys.o
strip com.so
mv -f com.so $EXPORTTO
printf "done.\n"

printf "Compiling cordic ... "
gcc $OPTIONS -o cordic.so cordic.c
strip cordic.so
mv -f cordic.so $EXPORTTO
printf "done.\n"

printf "Compiling curses ... "
gcc $OPTIONS -o curses.so curses.c -lncurses
strip curses.so
mv -f curses.so $EXPORTTO
printf "done.\n"

printf "Compiling ddhash ... "
gcc $OPTIONS -o ddhash.so ddhash.c
strip ddhash.so
mv -f ddhash.so $EXPORTTO
printf "done.\n"

printf "Compiling double ... "
gcc $OPTIONS -o double.so double.c
strip double.so
mv -f double.so $EXPORTTO
printf "done.\n"

printf "Compiling fastmath ... "
gcc $OPTIONS -Wno-strict-aliasing -o fastmath.so fastmath.c
strip fastmath.so
mv -f fastmath.so $EXPORTTO
printf "done.\n"

printf "Compiling fractals ... "
gcc $OPTIONS -o fractals.so fractals.c
strip fractals.so
mv -f fractals.so $EXPORTTO
printf "done.\n"

printf "Compiling gmp ... "
gcc $OPTIONS -o gmp.so gmp.c -lgmp
strip gmp.so
mv -f gmp.so $EXPORTTO
printf "done.\n"

printf "Compiling gzip ... "
gcc $OPTIONS -o gzip.so gzip.c -lz
strip gzip.so
mv -f gzip.so $EXPORTTO
printf "done.\n"

printf "Compiling iconv ... "
gcc $OPTIONS -o iconv.so iconv.c -liconv
strip iconv.so
mv -f iconv.so $EXPORTTO
printf "done.\n"

printf "Compiling intmap ... "
gcc $OPTIONS -o intmap.so intmap.c
strip intmap.so
mv -f intmap.so $EXPORTTO
printf "done.\n"

printf "Compiling minizip ... "
gcc $OPTIONS -o minizip.so minizip.c miniz.c -lagena
strip minizip.so
mv -f minizip.so $EXPORTTO
printf "done.\n"

printf "Compiling mpfr ... "
gcc $OPTIONS \
	-I/usr/local/include \
	-L/usr/local/lib \
	-mno-avx -mno-fma \
	-o mpfr.so mpfr.c \
	-Wl,-rpath,/usr/local/lib \
	-lmpfr -lgmp
# strip mpfr.so
mv -f mpfr.so $EXPORTTO
printf "done.\n"

printf "Compiling net ... "
gcc $OPTIONS -shared -fPIC -o net.so net.c
strip net.so
mv -f net.so $EXPORTTO
printf "done.\n"

if [ -f ../phq/phq.c ]; then
   printf "Compiling phonetiQs ... "
   gcc $OPTIONS -o phq.so ../phq/phq.c
   strip phq.so
   mv -f phq.so ../phq
   printf "done.\n"
fi

printf "Compiling skycrane ... "
gcc $OPTIONS -o skycrane.so skycrane.c
strip skycrane.so
mv -f skycrane.so $EXPORTTO
printf "done.\n"

printf "Compiling sqlite ... "
gcc $OPTIONS -o sqlite.so sqlite.c sqlite3.c sqlite3.h -lagena
strip sqlite.so
mv -f sqlite.so $EXPORTTO
printf "done.\n"

printf "Compiling strhash ... "
gcc $OPTIONS -o strhash.so strhash.c
strip strhash.so
mv -f strhash.so $EXPORTTO
printf "done.\n"

printf "Compiling strmap ... "
gcc $OPTIONS -o strmap.so strmap.c
strip strmap.so
mv -f strmap.so $EXPORTTO
printf "done.\n"

printf "Compiling testlib ... "
gcc $OPTIONS -o testlib.so testlib.c
strip testlib.so
mv -f testlib.so $EXPORTTO
printf "done.\n"

printf "Compiling zx ... "
gcc $OPTIONS -o zx.so zx.c
strip zx.so
mv -f zx.so $EXPORTTO
printf "done.\n"

echo Installing all libraries into /lib folder ...
echo All done.

