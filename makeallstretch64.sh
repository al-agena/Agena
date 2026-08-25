#!/usr/bin/sh

# you may have to change file permissions before compilation:
# chmod -R 755 .
# chown -R <username> .

cd src
sh makestretch.sh
sudo cp libagena.* /usr/local/lib
cd ..
cd ports
#cd libusb-1.0.18
#make clean
#chmod a+x ./configure
#./configure && make
#cd ..
#cd lualibusb1-1.0.1.agena
#make -f makefile.linux clean
#make -f makefile.linux
#make -f makefile.linux install
#cd ..
#cd zlib-1.2.11
# if you update the zlib library, change files in ports/gzip, as well.
#make clean
#chmod a+x ./configure
#./configure && make
#cd ..
#cd gzip
#make -f makefile.unix clean
#make -f makefile.unix
#make -f makefile.unix install
#cd ..
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
#cd xml
#make -f makefile.linux clean
#make -f makefile.linux
#make -f makefile.linux install
#cd ..
cd g2agena
#make clean
#chmod a+x ./configure
#./configure
#make
cd gdi
make -f makefile.stretch64 clean
make -f makefile.stretch64
make -f makefile.stretch64 install
cd ../../../../fltk-1.4.4/editor
make clean
make
cd ../..
