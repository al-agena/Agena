#!/usr/bin/sh
cd src
sh makesolaris.sh
cp libagena.* /usr/local/lib
cd ..
cd ports
# if you update the zlib library, change files in ports/gzip, as well.
cd zlib-1.2.11
make clean
chmod a+x ./configure
./configure && make
cd ..
cd ival
make clean
make solaris
cp ival.so ../../lib
cd ..
cd gzip
make -f makefile.solaris clean
make -f makefile.solaris
make -f makefile.solaris install
cd ..
cd mapm_4.9.5
make -f makefile.solaris clean
make -f makefile.solaris
cd ..
cd mapmagena
make -f makefile.solaris clean
make -f makefile.solaris
make -f makefile.solaris install
cd ..
cd g2agena
make clean
chmod a+x ./configure
./configure
make
cd gdi
make -f makefile.solaris clean
make -f makefile.solaris
make -f makefile.solaris install
cd ../../..
cd ../fltk-1.3.8/editor
make clean
make
cd ../../agena

