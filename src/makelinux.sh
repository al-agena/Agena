#!/bin/sh
# This script builds Agena and the plus packages on Linux
# Execute this script in the Agena src folder
make clean
make config
make linux
sh makepluslinux.sh
make install

