# RPM building file for OpenSUSE 10.3
# ! build Agena in /home/proglang/agena !
Summary: Agena Programming Language
Name: agena
Version: 7.9.2
Release: 1
# change Release number in build.sh file, as well.
#Source0: %{name}-%{version}-src.tar.gz
License: GPL v2
Group: Development/Languages
Source: http://downloads.sourceforge.net/agena/%{name}-%{version}-src.tar.gz
Summary: Agena is a procedural programming language.
#BuildRequires: ncurses, readline, freetype, fontconfig, libpng, zlib, expat, jpeg, gd, iconv, intl, xpm, libiconv, xpm, libintl, libgd
#BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}
BuildRoot: %{_tmppath}/%{name}-%{version}
URL: http://agena.sourceforge.net
Packager: Alexander Walz
Vendor: Alexander Walz
Provides: agena libagena.so libagena.a
%description
Agena is an easy-to-learn procedural programming language designed for science,
scripting, and many other applications.

Its syntax resembles very simplified Algol 68 with elements taken from Lua
and SQL.

Agena provides fast real and complex arithmetics, efficient text processing,
flexible data structures, intelligent procedures and package management.
%prep
# unpack the source and cd into the SOURCE directory
%setup -q
%build
make config
make opensuse
%install
make install
# for sanity protection, make sure the Buildroot is empty
rm -rf $RPM_BUILD_ROOT
%makeinstall
mkdir -p $RPM_BUILD_ROOT/usr/agena
mkdir -p $RPM_BUILD_ROOT/usr/agena/lib
mkdir -p $RPM_BUILD_ROOT/usr/agena/data
mkdir -p $RPM_BUILD_ROOT/usr/agena/doc
mkdir -p $RPM_BUILD_ROOT/usr/agena/share
mkdir -p $RPM_BUILD_ROOT/usr/agena/share/fractint
mkdir -p $RPM_BUILD_ROOT/usr/agena/share/icons
mkdir -p $RPM_BUILD_ROOT/usr/agena/share/schemes
mkdir -p $RPM_BUILD_ROOT/usr/agena/share/scripting
mkdir -p $RPM_BUILD_ROOT/usr/local
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
mkdir -p $RPM_BUILD_ROOT/usr/local/lib
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/lib/pkgconfig
# Documentation etc.
cp /home/proglang/agena/doc/agena-crashcourse.pdf $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/agena-primer.pdf $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/agena-reference.pdf $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/agena-quickref.xls $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/ascii.xls $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/regex.txt $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/SQLite.htm $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/doc/SQLite.agn $RPM_BUILD_ROOT/usr/agena/doc
cp /home/proglang/agena/installers/rpmsuse/agena.pc $RPM_BUILD_ROOT/usr/lib/pkgconfig
# Data files
cp /home/proglang/agena/data/langreg.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/viking2.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/viking2.txt $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/airlines.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/airlines.txt $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/gothic.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/sunspots.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/sunspots.txt $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/cities.kdx $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/cities.dbf $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/cities.txt $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/country.csv $RPM_BUILD_ROOT/usr/agena/data
cp /home/proglang/agena/data/timezone.csv $RPM_BUILD_ROOT/usr/agena/data

# Libraries
cp /home/proglang/agena/lib/abci.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/abci.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/ads.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/ads.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/agenaini.spl $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/ansi.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/astro.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/astro.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/com.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/cordic.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/curses.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/curses.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/divs.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/double.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/double.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/fastmath.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/fractals.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/fractals.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/gdi.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/gdi.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/gzip.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/iconv.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/iconv.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/ival.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/ival.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/kiss.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/library.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/mapm.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/mapm.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/minizip.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/gmp.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/gmp.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/mpfr.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/mpfr.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/net.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/net.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/skycrane.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/skycrane.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/strhash.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/strmap.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/sqlite.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/tar.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/telex.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/testlib.so $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/unimath.agn $RPM_BUILD_ROOT/usr/agena/lib
cp /home/proglang/agena/lib/zx.so $RPM_BUILD_ROOT/usr/agena/lib

cp /home/proglang/agena/lib/maple.agn $RPM_BUILD_ROOT/usr/agena/lib
# Schema files
cp /home/proglang/agena/share/schemes/agena.lang $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/schemes/agena.sch $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/schemes/agena.xml $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/schemes/agena.dat $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/schemes/nedit.rc $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/schemes/readme.txt $RPM_BUILD_ROOT/usr/agena/share/schemes
cp /home/proglang/agena/share/icons/agena.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena256.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/aedit256.gif $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/aedit256.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena.png $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena128x128.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena128x128.png $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena64x64.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena64x64.png $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena64x64.ppf $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena8b.gif $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agena8b.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agenasmall.ico $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/icons/agenasmall.png $RPM_BUILD_ROOT/usr/agena/share/icons
cp /home/proglang/agena/share/scripting/ln.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/getopt.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/cpuinfo.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/cpuinfo.bat $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/cpuinfo.cmd $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/drive.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/drive.bat $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/drive.cmd $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/memory.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/memory.bat $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/memory.cmd $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/whereis.agn $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/whereis.bat $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/scripting/whereis.cmd $RPM_BUILD_ROOT/usr/agena/share/scripting
cp /home/proglang/agena/share/fractint/alamo.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/bagheram.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/bordeaux.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/brgtsaph.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/dancooper.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/donald.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/drowning.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/hafez.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/hafez2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/m-maybe.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/maga.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/mondrian.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/oasis.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/office.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/office2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/orgteal.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/orozil.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/orozil2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/rauschen.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/sapphire.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/teahouse.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/usa.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/velvet.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/y2ktechn.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/chloro.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/chloro2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/cinemat.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/fa18raaf.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/fa18usaf.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o3.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o4.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o5.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/h2o6.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/iaf.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/lime.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/painter.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/paintr2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/pluto.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/prism.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/prism2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/prism3.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/prism4.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/prism5.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/spectra2.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/spectra3.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/spectra4.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/spectral.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/tiranga.map $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/share/fractint/alexmaps.txt $RPM_BUILD_ROOT/usr/agena/share/fractint
cp /home/proglang/agena/src/agena $RPM_BUILD_ROOT/usr/local/bin
#cp /home/proglang/fltk-1.4.4/editor/agenaedit $RPM_BUILD_ROOT/usr/local/bin
cp /home/proglang/agena/change.log $RPM_BUILD_ROOT/usr/agena
cp /home/proglang/agena/src/libagena.a $RPM_BUILD_ROOT/usr/local/lib
cp /home/proglang/agena/src/libagena.so $RPM_BUILD_ROOT/usr/local/lib
cp /home/proglang/agena/src/licence $RPM_BUILD_ROOT/usr/agena
# Srglue/Sragena
cp /home/proglang/agena/ports/sragena-104/src/srglue $RPM_BUILD_ROOT/usr/local/bin
cp /home/proglang/agena/ports/sragena-104/src/sragena $RPM_BUILD_ROOT/usr/local/bin
cp /home/proglang/agena/ports/sragena-104/README.srglue $RPM_BUILD_ROOT/usr/agena
chmod -R 755 $RPM_BUILD_ROOT/usr/agena
chmod 755 $RPM_BUILD_ROOT/usr/local/bin/agena
chmod 755 $RPM_BUILD_ROOT/usr/local/bin/srglue
chmod 755 $RPM_BUILD_ROOT/usr/local/bin/sragena
chmod 755 $RPM_BUILD_ROOT/usr/local/lib/libagena.a
chmod 755 $RPM_BUILD_ROOT/usr/local/lib/libagena.so
chmod 755 $RPM_BUILD_ROOT/usr/lib/pkgconfig/agena.pc
%clean
rm -rf $RPM_BUILD_ROOT
%files
%defattr(-,root,root)
/usr/agena/doc
/usr/agena/share
# no need to specify share subdirs
# do not remove agena.ini file, so do not delete /usr/agena/lib subdirectory completely
/usr/agena/change.log
/usr/agena/licence
/usr/agena/README.srglue
/usr/agena/data/langreg.csv
/usr/agena/data/viking2.csv
/usr/agena/data/viking2.txt
/usr/agena/data/airlines.csv
/usr/agena/data/airlines.txt
/usr/agena/data/gothic.csv
/usr/agena/data/sunspots.csv
/usr/agena/data/sunspots.txt
/usr/agena/data/cities.kdx
/usr/agena/data/cities.dbf
/usr/agena/data/cities.txt
/usr/agena/data/country.csv
/usr/agena/data/timezone.csv

# Libraries
/usr/agena/lib/abci.agn
/usr/agena/lib/abci.so
/usr/agena/lib/ads.agn
/usr/agena/lib/ads.so
/usr/agena/lib/agenaini.spl
/usr/agena/lib/ansi.agn
/usr/agena/lib/astro.agn
/usr/agena/lib/astro.so
/usr/agena/lib/com.so
/usr/agena/lib/cordic.so
/usr/agena/lib/curses.agn
/usr/agena/lib/curses.so
/usr/agena/lib/divs.agn
/usr/agena/lib/double.agn
/usr/agena/lib/double.so
/usr/agena/lib/fastmath.so
/usr/agena/lib/fractals.agn
/usr/agena/lib/fractals.so
/usr/agena/lib/gdi.agn
/usr/agena/lib/gdi.so
/usr/agena/lib/gzip.so
/usr/agena/lib/iconv.agn
/usr/agena/lib/iconv.so
/usr/agena/lib/ival.so
/usr/agena/lib/ival.agn
/usr/agena/lib/kiss.agn
/usr/agena/lib/library.agn
/usr/agena/lib/maple.agn
/usr/agena/lib/mapm.agn
/usr/agena/lib/mapm.so
/usr/agena/lib/minizip.so
/usr/agena/lib/gmp.agn
/usr/agena/lib/gmp.so
/usr/agena/lib/mpfr.agn
/usr/agena/lib/mpfr.so
/usr/agena/lib/net.agn
/usr/agena/lib/net.so
/usr/agena/lib/strhash.so
/usr/agena/lib/strmap.so
/usr/agena/lib/skycrane.agn
/usr/agena/lib/skycrane.so
/usr/agena/lib/sqlite.so
/usr/agena/lib/tar.agn
/usr/agena/lib/telex.agn
/usr/agena/lib/testlib.so
/usr/agena/lib/unimath.agn
/usr/agena/lib/zx.so
/usr/local/lib/libagena.a
/usr/local/lib/libagena.so
/usr/lib/pkgconfig/agena.pc
/usr/local/bin/agena
/usr/local/bin/srglue
/usr/local/bin/sragena
#/usr/local/bin/agenaedit
#%doc /usr/local/info/agena.info
#%doc %attr(0444,root,root) /usr/local/man/man1/indent.1
#%doc COPYING AUTHORS README NEWS
%post
#cd /usr/lib
#ln -sf ./libreadline.so.5.2 ./libreadline.so.5
#ln -sf ./libhistory.so.5.2 ./libhistory.so.5
#ln -sf ./libncurses.so.5.6 ./libncurses.so.5
echo ""
echo "The Agena binary has been put in the /usr/local/bin folder."
echo ""
echo "The Agena library files libagena.* have been put in the /usr/local/lib folder."
echo ""
echo "All other Agena files including the manual may be found in /usr/agena."
echo ""
echo "The main Agena library folder is /usr/agena/lib."
echo ""
