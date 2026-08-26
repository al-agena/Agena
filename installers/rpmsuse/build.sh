#/bin/sh
# shell script for building an Agena RPM package on OpenSUSE 10.3 and 11.x
# execute this script in the /installers/rpm folder
# Comment all `make install` statements in the Agena makefile before executing this script !

# !!! DO NOT ADD A SUFFIX BEHIND THE NUMERIC VERSION NUMBER !!!
export AGENANAME="agena-7.9.2"

# change the following path according to your needs
export AGENADIR="/home/proglang/agena"
export REL="-1"
cd $AGENADIR/src
rmdir $AGENANAME
mkdir $AGENANAME
cp *.c *.h *.def makefile readme.txt change.log $AGENANAME
tar cfv /usr/src/packages/SOURCES/$AGENANAME-src.tar.gz $AGENANAME
rpmbuild -ba --target=i386 $AGENADIR/installers/rpmsuse/agena.spec
#rpmbuild --nobuild --target=i386 $AGENADIR/installers/rpmsuse/agena.spec
cd $AGENADIR
cp /usr/src/packages/RPMS/i386/$AGENANAME$REL.i386.rpm .
mv -f $AGENANAME$REL.i386.rpm $AGENANAME-linux.i386.rpm
alien --scripts -d --keep-version $AGENANAME-linux.i386.rpm
