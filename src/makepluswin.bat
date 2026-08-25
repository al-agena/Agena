@echo off

rem Check
rem objdump -d dual.dll | C:\Windows\System32\findstr.exe "addsd"
rem to verify the code is really using SSE2 instructions
rem and
rem objdump -d ..\lib\dual.dll | C:\Windows\System32\findstr.exe /i "fadd fsub fmul fdiv fld fstp"
rem for x87 instructions.
rem Minimum processor requirement: Pentium 4/Athlon 64 or newer, SSE2 instruction set. AVX is not required.

rem Default math is x87
set SSE_FLAGS=-mfpmath=387
set MATH_MSG=x87 math (default)

rem Check if argument 1 is -sse or /sse
if "%~1"=="" goto START_BUILD
if /I "%~1"=="-sse" goto SET_SSE
if /I "%~1"=="/sse" goto SET_SSE
goto START_BUILD

:SET_SSE
set SSE_FLAGS=-mfpmath=sse -msse2 -march=nehalem
set MATH_MSG=SSE2 math

:START_BUILD
echo [INFO] Compiling with %MATH_MSG%.

set gccopts=-Wall -Wno-attributes -pipe -O3 -g -shared -I. -L. -lagena -fgnu89-inline %SSE_FLAGS%
set gccshortopts=-Wall -O3 -g -fgnu89-inline -shared -I. -L. %SSE_FLAGS%
set gccfileopts=-pipe -O3 -g -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes %SSE_FLAGS%
set gccfinalopts=-pipe -O3 -g -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes %SSE_FLAGS%
set gmpfropts=-Wall -g -O3 -fgnu89-inline -shared %SSE_FLAGS%
set offs=-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

set OLDPATH=%PATH%
set PATH=c:\MinGW\bin;%PATH%

echo Building abci.
gcc %gccopts% -o abci.dll abci.c

echo Building ADS.
gcc %gccfileopts% -c -o vecoff64.o vecoff64.c
gcc %offs% %gccfileopts% -c -o ads.o ads.c
gcc -O3 %SSE_FLAGS% -shared -static-libgcc -fgnu89-inline -fpic -o ads.dll ads.o vecoff64.o -L. -lagena

echo Building astro.
gcc %gccfileopts% -c -o sunriset.o sunriset.c
gcc %gccfileopts% -c -o moon.o moon.c
gcc %gccfileopts% -c -o astro.o astro.c
gcc %gccfinalopts% -o astro.dll astro.o agnt64.o sofa.o sdncal.o moon.o sunriset.o -L. -lagena

echo Building com.
gcc %gccopts% -DPLUS -o com.dll com.c luasys.c charbuf.c

echo Building cordic.
gcc %gccopts% -o cordic.dll cordic.c

echo Building curses.
set gcccursesopts=-O3 %SSE_FLAGS% -DLUA_BUILD_AS_DLL -DHAVE_NCURSES_H -fgnu89-inline -Wno-attributes
gcc %gcccursesopts% -c -o curses.o curses.c
gcc %gccfinalopts% -o curses.dll curses.o -L. -lagena -lncurses

echo Building double.
gcc %gccopts% -o double.dll double.c

echo Building fastmath.
gcc %gccopts% -Wno-strict-aliasing -o fastmath.dll fastmath.c

echo Building fractals.
gcc %gccopts% -o fractals.dll fractals.c

echo Building gzip.
set zlibs=-lagena -lz
gcc %gccopts% -o gzip.dll gzip.c %zlibs%

echo Building iconv.
set iconvlibs=-lagena -liconv
gcc %gccopts% -o iconv.1.dll iconv.c %iconvlibs%
rename iconv.1.dll iconv.dll

echo Building ival.
gcc %gccfileopts% -c -o ival.o ival.c
gcc %gccfinalopts% -o ival.dll ival.o -L. -lagena ../ports/ival/src/fi_lib.a

echo Building mapm.
set mapmlibs=-lagena -lmapm
gcc %gccopts% -o mapm.1.dll mapm.c %mapmlibs%
rename mapm.1.dll mapm.dll

echo Building minizip.
gcc %gccfileopts% -c -o miniz.o miniz.c
gcc %gccfinalopts% -o minizip.dll minizip.c miniz.o -L. -lagena

echo Building gmp.
set gmplibs=-lgmp -lagena
gcc %gccopts% -o gmp.1.dll gmp.c %gmplibs%
rename gmp.1.dll gmp.dll

echo Building mpfr.
set mpfrlibs=-I. -L. -lmpfr -I. -L. -lgmp -I. -L. -lagena
gcc %mpfropts% -o mpfr.1.dll mpfr.c %mpfrlibs%
rename mpfr.1.dll mpfr.dll

echo Building net.
gcc %gccfileopts% -c -o net.o net.c
gcc %gccfinalopts% -o net.dll net.o -L. -lwsock32 -lwininet -lmswsock -lWs2_32 -lagena

echo Building skycrane.
gcc %gccopts% -o skycrane.dll skycrane.c

echo Building testlib.
gcc %gccopts% -o testlib.dll testlib.c

echo Building zx.
gcc %gccopts% -o zx.dll zx.c

rem strip --strip-unneeded *.dll  MinGW/GCC 10.x's strip does not accept wildcards any longer
echo.
echo Slimming and moving packages.

strip --strip-unneeded abci.dll
copy abci.dll ..\lib >> NUL
strip --strip-unneeded ads.dll
copy ads.dll ..\lib >> NUL
strip --strip-unneeded astro.dll
copy astro.dll ..\lib >> NUL
strip --strip-unneeded com.dll
copy com.dll ..\lib >> NUL
strip --strip-unneeded cordic.dll
copy cordic.dll ..\lib >> NUL
strip --strip-unneeded curses.dll
copy curses.dll ..\lib >> NUL
strip --strip-unneeded double.dll
copy double.dll ..\lib >> NUL
strip --strip-unneeded fractals.dll
copy fractals.dll ..\lib >> NUL
strip --strip-unneeded iconv.dll
copy iconv.dll ..\lib >> NUL
strip --strip-unneeded ival.dll
copy ival.dll ..\lib >> NUL
strip --strip-unneeded fastmath.dll
copy fastmath.dll ..\lib >> NUL
strip --strip-unneeded gzip.dll
copy gzip.dll ..\lib >> NUL
strip --strip-unneeded mapm.dll
copy mapm.dll ..\lib >> NUL
strip --strip-unneeded minizip.dll
copy minizip.dll ..\lib >> NUL
strip --strip-unneeded gmp.dll
copy gmp.dll ..\lib >> NUL
strip --strip-unneeded mpf.dll
copy mpf.dll ..\lib >> NUL
strip --strip-unneeded net.dll
copy net.dll ..\lib >> NUL
strip --strip-unneeded skycrane.dll
copy skycrane.dll ..\lib >> NUL
strip --strip-unneeded testlib.dll
copy testlib.dll ..\lib >> NUL
strip --strip-unneeded zx.dll
copy zx.dll ..\lib >> NUL

echo Installed all libraries into Agena /lib folder.

del abci.dll
del ads.dll
del astro.dll
del com.dll
del cordic.dll
del curses.dll
del double.dll
del fastmath.dll
del fractals.dll
del gzip.dll
del iconv.dll
del ival.dll
del mapm.dll
del minizip.dll
del mp.dll
del mpf.dll
del net.dll
del skycrane.dll
del testlib.dll
del zx.dll

del ads.o astro.o curses.o ival.o moon.o net.o sunriset.o miniz.o vecoff64.o

set iconvlibs=
set curseslibs=
set expatlibs=
set gccfileopts=
set gcccursesopts=
set gccfinalopts=
set gccopts=
set gccshortopts=
set gmplibs=
set gmpopts=
set mapmlibs=
set mpfrlibs=
set mpfropts=
set offs=
set PATH=%OLDPATH%
set OLDPATH=
set zlibs=

rem ******************************************************************
rem old deprecated packages
rem ******************************************************************

rem echo Building compress.
rem gcc %gccopts% -o compress.dll compress.c lcompress.c agnhlps.c

rem echo Building words.
rem gcc %gccopts% -o words.dll words.c

rem echo Building phonetiQs.
rem gcc %gccopts% -o phq.dll plus/phq.c
rem copy phq.dll ..\phq >> NUL
rem del phq.dll