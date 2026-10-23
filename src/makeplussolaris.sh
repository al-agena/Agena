#! /bin/sh
# Compile and install skript for the plus package for Solaris 10 (Sparc and x86)
#
# Execute this batch file in the .../agena/src folder by typing:
# sh makeplussolaris.sh
#
# Note that libgcc_s.so and libgcc_s.so.1 must exist in /usr/lib and
# /usr/local/lib so that the modules can be used.

# delete *.o files not deleted by make clean
for i in abci.o ads.o aspell.o astro.o charbuf.o com.o cordic.o curses.o \
    dcastro.o double.o fastmath.o fractals.o gmp.o iconv.o \
    interp.o luasys.o miniz.o minizip.o moon.o mpfr.o net.o phq.o \
    skycrane.o sqlite.o sqlite3.o sunriset.o testlib.o zx.o \
    strhash.o strmap.o
do
  if [ -f "$i" ]; then
    rm "$i"
  fi
done

OPTIONS="-O3 -fpic -Wno-attributes -fgnu89-inline"
OPTFINAL="-O3 -shared -fpic"

printf "Compiling abci ... "
gcc $OPTIONS -c -o abci.o abci.c
gcc $OPTFINAL -o abci.so abci.o
rm -f abci.o
printf "done.\n"

printf "Compiling ADS ... "
gcc $OPTIONS -c -o ads.o ads.c
# we need to compile and link against vector in Sun Solaris
gcc $OPTIONS -c -o vecoff64.o vecoff64.c
gcc $OPTFINAL -o ads.so ads.o
rm -f vecoff64.o ads.o
printf "done.\n"

printf "Compiling aspell ... "
ASPELL_INCLUDES="-I/usr/local/include -I/export/home/proglang/agena/src"
ASPELL_LIBS="-Wl,-Bstatic -L/usr/local/lib -laspell -Wl,-Bdynamic -lstdc++ -lm"
gcc $OPTIONS $ASPELL_INCLUDES -c -o aspell.o aspell.c
gcc $OPTFINAL -o aspell.so aspell.o $ASPELL_LIBS
rm -f aspell.o
printf "done.\n"

printf "Compiling astro ... "
gcc $OPTIONS -c -o astro.o astro.c
gcc $OPTIONS -c -o moon.o moon.c
gcc $OPTIONS -c -o sunriset.o sunriset.c
gcc $OPTIONS -c -o dcastro.o dcastro.c
gcc $OPTFINAL -o astro.so astro.o sunriset.o moon.o dcastro.o
rm -f astro.o sunriset.o moon.o dcastro.o
printf "done.\n"

printf "Compiling com ... "
gcc $OPTIONS -DPLUS -c -o charbuf.o charbuf.c
gcc $OPTIONS -c -o luasys.o luasys.c
gcc $OPTIONS -c -o com.o com.c
gcc $OPTFINAL -o com.so com.o charbuf.o luasys.o
rm -f com.o charbuf.o luasys.o
printf "done.\n"

printf "Compiling cordic ... "
gcc $OPTIONS -c -o cordic.o cordic.c
gcc $OPTFINAL -o cordic.so cordic.o
rm -f cordic.o
printf "done.\n"

# ncurses 5.0, 5.9 and 6.1 do not compile in Solaris, so use curses.
printf "Compiling curses ... "
gcc $OPTIONS -Wno-discarded-qualifiers -std=c99 -lcurses -c -o curses.o curses.c
gcc $OPTFINAL -std=c99 -lcurses -o curses.so curses.o
rm -f curses.o
printf "done.\n"

printf "Compiling double ... "
gcc $OPTIONS -c -o double.o double.c
gcc $OPTFINAL -o double.so double.o
rm -f double.o
printf "done.\n"

printf "Compiling fastmath ... "
gcc $OPTIONS -Wno-strict-aliasing -c -o fastmath.o fastmath.c
gcc $OPTFINAL -Wno-strict-aliasing -o fastmath.so fastmath.o
rm -f fastmath.o
printf "done.\n"

printf "Compiling fractals ... "
gcc $OPTIONS -c -o fractals.o fractals.c
gcc $OPTFINAL -o fractals.so fractals.o
rm -f fractals.o
printf "done.\n"

printf "Compiling gmp ... "
gcc $OPTIONS -std=c99 -lgmp -c -o gmp.o gmp.c
gcc $OPTFINAL -std=c99 -lgmp -o gmp.so gmp.o
rm -f gmp.o
printf "done.\n"

printf "Compiling gzip ... "
gcc $OPTIONS -c -o gzip.o gzip.c
gcc $OPTFINAL -o gzip.so gzip.o -lz
rm -f gzip.o
printf "done.\n"

printf "Compiling iconv ... "
gcc $OPTIONS -std=c99 -liconv -c -o iconv.o iconv.c
gcc $OPTFINAL -std=c99 -liconv -o iconv.so iconv.o
rm -f iconv.o
printf "done.\n"

printf "Compiling minizip ... "
gcc $OPTIONS -DPCRE2_CODE_UNIT_WIDTH=8 -c -o miniz.o miniz.c
gcc $OPTIONS -DPCRE2_CODE_UNIT_WIDTH=8 -c -o minizip.o minizip.c
gcc $OPTFINAL -o minizip.so minizip.o miniz.o
rm -f miniz.o minizip.o
printf "done.\n"

printf "Compiling mpfr ... "
gcc $OPTIONS -std=c99 -lmpfr -lgmp -c -o mpfr.o mpfr.c
gcc $OPTFINAL -std=c99 -lmpfr -lgmp -o mpfr.so mpfr.o
rm -f mpfr.o
printf "done.\n"

printf "Compiling net ... "
#gcc $OPTIONS -c -o network.o network.c
gcc $OPTIONS -c -o net.o net.c
#gcc $OPTFINAL -o net.so net.o network.o -lsocket -lnsl
gcc $OPTFINAL -o net.so net.o -lsocket -lnsl
rm -f net.o
printf "done.\n"

if [ -f ../phq/phq.c ]; then
   printf "Compiling phonetiQs ... "
   gcc $OPTIONS -c -o phq.o ../phq/phq.c
   gcc $OPTFINAL -o phq.so phq.o
   rm -f phq.o
   printf "done.\n"
   strip phq.so
   mv phq.so ../phq
fi

printf "Compiling skycrane ... "
gcc $OPTIONS -c -o skycrane.o skycrane.c
gcc $OPTFINAL -o skycrane.so skycrane.o
rm -f skycrane.o
printf "done.\n"

printf "Compiling sqlite ... "
gcc $OPTIONS -DPCRE2_CODE_UNIT_WIDTH=8 -c -o sqlite3.o sqlite3.c
gcc $OPTIONS -DPCRE2_CODE_UNIT_WIDTH=8 -c -o sqlite.o sqlite.c
gcc $OPTFINAL -o sqlite.so sqlite.o sqlite3.o
rm -f sqlite.o sqlite3.o
printf "done.\n"

printf "Compiling strhash ... "
gcc $OPTIONS -c -o strhash.o strhash.c
gcc $OPTFINAL -o strhash.so strhash.o
rm -f strhash.o
printf "done.\n"

printf "Compiling strmap ... "
gcc $OPTIONS -c -o strmap.o strmap.c
gcc $OPTFINAL -o strmap.so strmap.o
rm -f strmap.o
printf "done.\n"

printf "Compiling testlib ... "
gcc $OPTIONS -c -o testlib.o testlib.c
gcc $OPTFINAL -o testlib.so testlib.o
rm -f testlib.o
printf "done.\n"

printf "Compiling zx ... "
gcc $OPTIONS -c -o zx.o zx.c
gcc $OPTFINAL -o zx.so zx.o
rm -f zx.o
printf "done.\n"


printf "Slimming and moving packages ... "
strip *.so
mv abci.so ../lib
mv ads.so ../lib
mv aspell.so ../lib
mv astro.so ../lib
mv com.so ../lib
mv cordic.so ../lib
mv curses.so ../lib
mv double.so ../lib
mv fastmath.so ../lib
mv fractals.so ../lib
mv gmp.so ../lib
mv gzip.so ../lib
mv iconv.so ../lib
mv intmap.so ../lib
mv minizip.so ../lib
mv mpfr.so ../lib
mv net.so ../lib
mv skycrane.so ../lib
mv strhash.so ../lib
mv strmap.so ../lib
mv sqlite.so ../lib
mv testlib.so ../lib
mv zx.so ../lib
printf "done.\n"

echo Installed all libraries into /lib folder.
echo All done.

