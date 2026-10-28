#!/usr/bin/sh

# you may have to change file permissions before compilation:
# chmod -R 755 .
# chown -R <username> .

# Always run with sudo:
# sudo sh makeallraspi.sh

cd src
sh makeraspi.sh
cd ..
cd ports
cd ival
make clean
make linux
cp ival.so ../../lib
cd ..
cd mapm_4.9.5
make -f makefile.unx clean
make -f makefile.unx
cd ..
cd mapmagena
make -f makefile.linux clean
make -f makefile.linux
make -f makefile.linux install
cd ..
cd g2agena
make clean
chmod a+x ./configure
./configure
make
cd gdi
make -f makefile.stretch clean
make -f makefile.stretch
make -f makefile.stretch install
cd ../../../../fltk-1.4.4/editor
make clean
make
cd ../..
