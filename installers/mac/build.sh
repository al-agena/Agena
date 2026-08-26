#!/usr/bin/sh
# shell script for building an Agena Mac OS X PKG package on Mac OS X 10.5 Intel
# execute this script in the /installers/mac folder

# environment variables, change the following paths according to your needs

#export AGENAHOME="/Users/alexanderwalz/agena"
export AGENAHOME="../.."
export MAINTARGETDIR="$AGENAHOME/installers/mac/macinstalldir"
export AGENATARGET_USR="$MAINTARGETDIR/agena/usr"
export AGENATARGET_DOCS="$MAINTARGETDIR/agena/Library/Documentation/Agena"

#export AGENAHOME="c:/agena"
#export MAINTARGETDIR="c:/agena/installers/mac"
#export AGENATARGET_USR="$MAINTARGETDIR/agena/usr"
#export AGENATARGET_DOCS="$MAINTARGETDIR/agena/Library/Documentation/Agena"

# create directories if necessary

mkdir -p $MAINTARGETDIR
mkdir -p $MAINTARGETDIR/agena
mkdir -p $MAINTARGETDIR/agena/usr
mkdir -p $MAINTARGETDIR/agena/usr/agena
mkdir -p $MAINTARGETDIR/agena/usr/agena/lib
mkdir -p $MAINTARGETDIR/agena/usr/agena/data
mkdir -p $MAINTARGETDIR/agena/usr/agena/share
mkdir -p $MAINTARGETDIR/agena/usr/agena/share/fractint
mkdir -p $MAINTARGETDIR/agena/usr/agena/share/icons
mkdir -p $MAINTARGETDIR/agena/usr/agena/share/schemes
mkdir -p $MAINTARGETDIR/agena/usr/agena/share/scripting
mkdir -p $MAINTARGETDIR/agena/usr/local
mkdir -p $MAINTARGETDIR/agena/usr/local/bin
mkdir -p $MAINTARGETDIR/agena/usr/local/lib
mkdir -p $MAINTARGETDIR/agena/Library
mkdir -p $MAINTARGETDIR/agena/Library/Documentation
mkdir -p $MAINTARGETDIR/agena/Library/Documentation/Agena

# Copy files to be distributed to target folders

# Documentation files
cp $AGENAHOME/doc/agena-crashcourse.pdf $AGENATARGET_DOCS
cp $AGENAHOME/doc/agena-primer.pdf $AGENATARGET_DOCS
cp $AGENAHOME/doc/agena-reference.pdf $AGENATARGET_DOCS
cp $AGENAHOME/doc/agena-quickref.xls $AGENATARGET_DOCS
cp $AGENAHOME/doc/ascii.xls $AGENATARGET_DOCS
cp $AGENAHOME/doc/regex.txt $AGENATARGET_DOCS
cp $AGENAHOME/doc/SQLite.htm $AGENATARGET_DOCS
cp $AGENAHOME/doc/SQLite.agn $AGENATARGET_DOCS

# Data files
cp $AGENAHOME/data/langreg.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/viking2.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/viking2.txt $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/airlines.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/airlines.txt $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/gothic.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/sunspots.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/sunspots.txt $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/cities.kdx $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/cities.dbf $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/cities.txt $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/country.csv $AGENATARGET_USR/agena/data
cp $AGENAHOME/data/timezone.csv $AGENATARGET_USR/agena/data

# Library files
cp $AGENAHOME/lib/abci.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/abci.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/ads.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/ads.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/agenaini.spl $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/ansi.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/astro.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/astro.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/com.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/cordic.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/curses.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/curses.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/divs.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/double.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/double.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/fastmath.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/fractals.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/fractals.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/gdi.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/gdi.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/gzip.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/iconv.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/iconv.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/ival.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/ival.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/kiss.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/library.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/maple.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/mapm.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/mapm.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/minizip.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/gmp.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/gmp.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/mpfr.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/mpfr.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/net.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/net.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/sqlite.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/strhash.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/strmap.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/skycrane.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/skycrane.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/tar.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/telex.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/testlib.so $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/unimath.agn $AGENATARGET_USR/agena/lib
cp $AGENAHOME/lib/zx.so $AGENATARGET_USR/agena/lib


# Scheme files
cp $AGENAHOME/share/schemes/agena.lang $AGENATARGET_USR/agena/share/schemes
cp $AGENAHOME/share/schemes/agena.xml $AGENATARGET_USR/agena/share/schemes
cp $AGENAHOME/share/schemes/agena.dat $AGENATARGET_USR/agena/share/schemes
cp $AGENAHOME/share/schemes/agena.sch $AGENATARGET_USR/agena/share/schemes
cp $AGENAHOME/share/schemes/nedit.rc $AGENATARGET_USR/agena/share/schemes
cp $AGENAHOME/share/schemes/readme.txt $AGENATARGET_USR/agena/share/schemes

# Icon files
cp $AGENAHOME/share/icons/agena.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena256.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/aedit256.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena.png $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena128x128.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena128x128.png $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena64x64.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena64x64.png $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena8b.gif $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agena8b.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agenasmall.ico $AGENATARGET_USR/agena/share/icons
cp $AGENAHOME/share/icons/agenasmall.png $AGENATARGET_USR/agena/share/icons

# Scripting samples
cp $AGENAHOME/share/scripting/ln.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/getopt.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/cpuinfo.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/cpuinfo.bat $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/cpuinfo.cmd $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/drive.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/drive.bat $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/drive.cmd $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/memory.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/memory.bat $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/memory.cmd $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/whereis.agn $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/whereis.bat $AGENATARGET_USR/agena/share/scripting
cp $AGENAHOME/share/scripting/whereis.cmd $AGENATARGET_USR/agena/share/scripting

# FRACTINT colour maps
cp $AGENAHOME/share/fractint/alamo.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/bagheram.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/bordeaux.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/brgtsaph.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/dancooper.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/donald.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/drowning.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/hafez.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/hafez2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/m-maybe.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/maga.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/mondrian.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/oasis.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/office.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/office2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/orgteal.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/orozil.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/orozil2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/rauschen.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/sapphire.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/teahouse.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/usa.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/velvet.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/y2ktechn.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/chloro.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/chloro2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/cinemat.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/fa18raaf.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/fa18usaf.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o3.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o4.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o5.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/h2o6.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/iaf.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/lime.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/painter.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/paintr2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/pluto.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/prism.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/prism2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/prism3.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/prism4.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/prism5.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/spectra2.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/spectra3.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/spectra4.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/spectral.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/tiranga.map $AGENATARGET_USR/agena/share/fractint
cp $AGENAHOME/share/fractint/alexmaps.txt $AGENATARGET_USR/agena/share/fractint

# Change Log et cetera
cp $AGENAHOME/change.log $AGENATARGET_USR/agena
cp $AGENAHOME/ports/sragena-105/README.srglue $AGENATARGET_USR/agena

# Binary and shared C libraries
cp $AGENAHOME/src/libagena.a $AGENATARGET_USR/local/lib
cp $AGENAHOME/src/licence $AGENATARGET_USR/local/lib
cp $AGENAHOME/src/agena $AGENATARGET_USR/local/bin
cp $AGENAHOME/ports/sragena-105/src/srglue $AGENATARGET_USR/local/bin
cp $AGENAHOME/ports/sragena-105/src/sragena $AGENATARGET_USR/local/bin

cp $AGENAHOME/../fltk-1.4.4/editor/agenaedit $AGENATARGET_USR/local/bin
cp -r $AGENAHOME/../fltk-1.4.4/editor/agenaedit.app $AGENATARGET_USR/local/bin
chmod a+x $AGENATARGET_USR/local/bin/agenaedit

# position: owner & group & other
# 7 = read (1) + write (2) + execute (4) = 1 + 2 + 4 = 7
# 5 = read + execute
chmod -R 755 $AGENATARGET_USR/agena
chmod 755 $AGENATARGET_USR/local/bin/agena
chmod 755 $AGENATARGET_USR/local/bin/srglue
chmod 755 $AGENATARGET_USR/local/bin/sragena
chmod 755 $AGENATARGET_USR/local/lib/libagena.a

