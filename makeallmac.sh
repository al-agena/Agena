cd src
sh makemacosx.sh
sudo cp libagena.a /usr/local/lib
cd ..
cd ports
cd zlib-1.2.11
# if you update the zlib library, change files in ports/gzip, as well.
make clean
chmod a+x ./configure
./configure && make
cd ..
cd gzip
make -f makefile.macosx clean
make -f makefile.macosx
make -f makefile.macosx install
cd ..
cd ival
make clean
make macosx
cp ival.so ../../lib
cd ..
cd mapm_4.9.5
make -f makefile.osx clean
make -f makefile.osx
cd ..
cd mapmagena
make -f makefile.macosx clean
make -f makefile.macosx
make -f makefile.macosx install
cd ..
cd xml
make -f makefile.macosx clean
make -f makefile.macosx
make -f makefile.macosx install
cd ..
cd g2agena
make clean
chmod a+x ./configure
#./configure
./configure --x-libraries=/usr/X11/lib --x-includes=/usr/X11/include --disable-dependency-tracking
make
cd gdi
make -f makefile.macosx clean
make -f makefile.macosx
make -f makefile.macosx install
cd ../../..
cd ../fltk-1.4.4/editor
make clean
make
cd ../../agena
