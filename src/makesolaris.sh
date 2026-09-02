#!/usr/bin/sh
# This script builds Agena and the plus packages on Solaris
# Execute this script in the Agena src folder
make clean
make config
make solaris -j2
sh makeplussolaris.sh
make install

