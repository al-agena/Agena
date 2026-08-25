#! /bin/sh
# compile-and-install skript for the plus package on Mac OS X 10.4 or later
# execute this batch file in the .../agena/src folder by typing:
# sh makeplusmacosx.sh
# linking and stripping do not work here

export OPTIONS1="-O2 -g -DNOINLINE -fno-common -c -force_cpusubtype_ALL -mmacosx-version-min=10.4"
export OPTIONS2="-DNOINLINE -bundle -undefined dynamic_lookup -force_cpusubtype_ALL -mmacosx-version-min=10.4"
export DDMATHFLAGS="-fno-builtin -ffloat-store -fno-unsafe-math-optimizations"

#export OPTIONS1="-O2 -g -fno-common -c -force_cpusubtype_ALL -mmacosx-version-min=10.4 -arch i386 -arch ppc -arch ppc64"
#export OPTIONS2="-bundle -undefined dynamic_lookup -force_cpusubtype_ALL -mmacosx-version-min=10.4 -arch i386 -arch ppc -arch ppc64"

export EXPORTTO="../lib"

# delete *.o files not deleted by make clean
for i in abci.o ads.o astro.o charbuf.o com.o cordic.o curses.o \
    dcastro.o double.o fastmath.o fractals.o gmp.o iconv.o interp.o \
    luasys.o miniz.o minizip.o moon.o mpfr.o net.o phq.o \
    skycrane.o sqlite.o sqlite3.o sunriset.o testlib.o zx.o \
    strhash.o strmap.o intmap.o
do
  if [ -f "$i" ]; then
    rm "$i"
  fi
done

printf "Compiling abci ... "
gcc $OPTIONS1 -o abci.o abci.c
gcc $OPTIONS2 -o abci.so abci.o
mv -f abci.so $EXPORTTO
printf "done.\n"

printf "Compiling ADS ... "
gcc $OPTIONS1 -o vecoff64.o vecoff64.c
gcc $OPTIONS1 -o ads.o ads.c
gcc $OPTIONS2 -o ads.so ads.o vecoff64.o
mv -f ads.so $EXPORTTO
printf "done.\n"

printf "Compiling astro ... "
gcc $OPTIONS1 -o astro.o astro.c
gcc $OPTIONS1 -o moon.o moon.c
gcc $OPTIONS1 -o sunriset.o sunriset.c
gcc $OPTIONS1 -o dcastro.o dcastro.c
gcc $OPTIONS2 -o astro.so astro.o moon.o sunriset.o dcastro.o
mv -f astro.so $EXPORTTO
printf "done.\n"

printf "Compiling com ... "
gcc $OPTIONS1 -DPLUS -o charbuf.o charbuf.c
gcc $OPTIONS1 -o luasys.o luasys.c
gcc $OPTIONS1 -o com.o com.c
gcc $OPTIONS2 -o com.so com.o luasys.o charbuf.o
mv -f com.so $EXPORTTO
printf "done.\n"

printf "Compiling cordic ... "
gcc $OPTIONS1 -o cordic.o cordic.c
gcc $OPTIONS2 -o cordic.so cordic.o
mv -f cordic.so $EXPORTTO
printf "done.\n"

printf "Compiling curses ... "
gcc $OPTIONS1 -o curses.o curses.c
gcc $OPTIONS2 -o curses.so curses.o -lncurses
mv -f curses.so $EXPORTTO
printf "done.\n"

printf "Compiling double ... "
gcc $OPTIONS1 -o double.o double.c
gcc $OPTIONS2 -o double.so double.o
mv -f double.so $EXPORTTO
printf "done.\n"

printf "Compiling fastmath ... "
gcc $OPTIONS1 -Wno-strict-aliasing -o fastmath.o fastmath.c
gcc $OPTIONS2 -Wno-strict-aliasing -o fastmath.so fastmath.o
mv -f fastmath.so $EXPORTTO
printf "done.\n"

printf "Compiling fractals ... "
gcc $OPTIONS1 -o fractals.o fractals.c
gcc $OPTIONS2 -o fractals.so fractals.o
mv -f fractals.so $EXPORTTO
printf "done.\n"

printf "Compiling gmp ... "
gcc $OPTIONS1 gmp.c -o gmp.o
gcc $OPTIONS2 -o gmp.so gmp.o  -lgmp
mv -f gmp.so $EXPORTTO
printf "done.\n"

# build libiconv-1.19 on Mac OS X statically with the following statement in the `ports` folder:
# ./configure --prefix=/usr/local --disable-nls --enable-static --disable-shared --host=i386-apple-darwin10
printf "Compiling iconv ... "
# then refer to the *.a file there
ICONV_STATIC="../ports/libiconv-1.19/lib/.libs/libiconv.a"
ICONV_INC="-I../ports/libiconv-1.19/lib/include"
gcc $OPTIONS1  $ICONV_INC iconv.c -o iconv.o
gcc $OPTIONS2 -o iconv.so iconv.o $ICONV_STATIC
mv -f iconv.so $EXPORTTO
printf "done.\n"

printf "Compiling intmap ... "
gcc $OPTIONS1 intmap.c -o intmap.o
gcc $OPTIONS2 -o intmap.so intmap.o
mv -f intmap.so $EXPORTTO
printf "done.\n"

printf "Compiling minizip ... "
gcc $OPTIONS1 -c -o miniz.o miniz.c
gcc $OPTIONS1 -c -o minizip.o minizip.c
gcc $OPTIONS2 -o minizip.so minizip.o miniz.o
mv -f minizip.so $EXPORTTO
printf "done.\n"

printf "Compiling mpfr ... "
gcc $OPTIONS1 mpfr.c -o mpfr.o
gcc $OPTIONS2 -o mpfr.so mpfr.o -lmpfr -lgmp
mv -f mpfr.so $EXPORTTO
printf "done.\n"

printf "Compiling net ... "
gcc $OPTIONS1 -o net.o net.c
gcc $OPTIONS2 -o net.so net.o
mv -f net.so $EXPORTTO
printf "done.\n"

if [ -f ../phq/phq.c ]; then
   printf "Compiling phonetiQs ... "
   gcc $OPTIONS1 -o phq.o ../phq/phq.c
   gcc $OPTIONS2 -o phq.so phq.o
   mv -f phq.so ../phq
   printf "done.\n"
fi

printf "Compiling skycrane ... "
gcc $OPTIONS1 -o skycrane.o skycrane.c
gcc $OPTIONS2 -o skycrane.so skycrane.o
mv -f skycrane.so $EXPORTTO
printf "done.\n"

printf "Compiling sqlite ... "
# Don't include the sqlite3.h header file, or you'll get "file is not of the required architecture" otherwise.
gcc $OPTIONS1 -c -o sqlite3.o sqlite3.c
gcc $OPTIONS1 -c -o sqlite.o sqlite.c
gcc $OPTIONS2 -o sqlite.so sqlite.o sqlite3.o
mv -f sqlite.so $EXPORTTO
printf "done.\n"


printf "Compiling strhash ... "
gcc $OPTIONS1 -o strhash.o strhash.c
gcc $OPTIONS2 -o strhash.so strhash.o
mv -f strhash.so $EXPORTTO
printf "done.\n"

printf "Compiling strmap ... "
gcc $OPTIONS1 -o strmap.o strmap.c
gcc $OPTIONS2 -o strmap.so strmap.o
mv -f strmap.so $EXPORTTO
printf "done.\n"

printf "Compiling testlib ... "
gcc $OPTIONS1 -o testlib.o testlib.c
gcc $OPTIONS2 -o testlib.so testlib.o
mv -f testlib.so $EXPORTTO
printf "done.\n"

printf "Compiling zx ... "
gcc $OPTIONS1 -o zx.o zx.c
gcc $OPTIONS2 -o zx.so zx.o
mv -f zx.so $EXPORTTO
printf "done.\n"

echo Installing all libraries into /lib folder ...
echo All done.
