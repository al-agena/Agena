#!/bin/sh
# This script builds Agena and the plus packages on Intel/AMD Debian Stretch in the src folder.
# Execute this script in the Agena src folder WITH PRECEEDING sudo.
make clean
rm -f *.o *.so *.a
make config
make stretch -j$(nproc)
sh makeplusstretch.sh
# make install;  cannot use this as my Makefile is for various platforms
sudo mkdir -p /usr/local/lib
sudo mkdir -p /usr/local/include
sudo cp agena /usr/bin/
sudo cp libagena.so libagena.a /usr/local/lib/

DEB_TRIPLET=$(dpkg-architecture -q DEB_HOST_MULTIARCH 2>/dev/null)
# If dpkg-architecture is missing, look at the system compiler target directly
if [ -z "$DEB_TRIPLET" ]; then
  DEB_TRIPLET=$(gcc -dumpmachine 2>/dev/null)
fi
if [ -n "$DEB_TRIPLET" ] && [ -d "/usr/lib/$DEB_TRIPLET" ]; then
  # Create the symlink exactly where this specific Debian/Raspbian variant expects system libraries
  sudo ln -sf /usr/local/lib/libagena.so /usr/lib/"$DEB_TRIPLET"/libagena.so
else
  # Fallback for old systems or non-Debian environments
  sudo ln -sf /usr/local/lib/libagena.so /usr/lib/libagena.so
fi

sudo cp agena.h agnxlib.h agncmpt.h agnconf.h agnhlps.h agenalib.h lstate.h /usr/local/include
sudo chmod 755 /usr/bin/agena
sudo chmod 644 /usr/local/lib/libagena.so /usr/local/lib/libagena.a
sudo chmod 644 /usr/local/include/agena.h /usr/local/include/agn*.h /usr/local/include/agenalib.h
sudo ldconfig

