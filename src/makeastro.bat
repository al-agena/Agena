@echo off
rem set gccfileopts=-pipe -O3 -DLUA_BUILD_AS_DLL -fgnu89-inline -fno-fast-math -fexcess-precision=standard -fno-associative-math -Wno-attributes
rem set gccfinalopts=-pipe -O3 -shared -static-libgcc -fpic -fgnu89-inline -fno-fast-math -fexcess-precision=standard -fno-associative-math -Wno-attributes

rem There is no difference in Windows between the setup above and the following one:

set gccfileopts=-Wall -pipe -O2 -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
set gccfinalopts=-Wall -pipe -O2 -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes

del sunriset.o moon.o dcastro.o astro.o

gcc %gccfileopts% -c -o sunriset.o sunriset.c
gcc %gccfileopts% -c -o moon.o moon.c
gcc %gccfileopts% -c -o dcastro.o dcastro.c
gcc %gccfileopts% -c -o astro.o astro.c
gcc %gccfinalopts% -o astro.dll astro.o agnt64.o sofa.o sdncal.o moon.o sunriset.o dcastro.o -L. -lagena

strip --strip-unneeded astro.dll

copy astro.dll ..\lib >> NUL

echo Installed `astro` library into Agena /lib folder.

del astro.dll

set gccfileopts=
set gccfinalopts=