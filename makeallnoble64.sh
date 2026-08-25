#!/usr/bin/sh

# you may have to change file permissions before compilation:
# chmod -R 755 .
# chown -R <username> .

cd src
sh makestretch.sh
sudo cp libagena.* /usr/local/lib
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
make -f makefile.stretch64 clean
make -f makefile.stretch64
make -f makefile.stretch64 install
cd ../../../../fltk-1.4.4/editor
make clean
make
cd ../..
