#!/usr/bin/sh

# 1. Configuration
set -e
export AGENAVER="7.9.2"
export BUILD_USER="pi"
export AGENAHOME="/home/$BUILD_USER/agena"

# 2. Paths and Variables
export MAINTARGETDIR="$AGENAHOME/installers/debian"
export AGENATARGET_ROOT="$MAINTARGETDIR/agena"
export AGENA_INSTALL_DIR="$AGENATARGET_ROOT/usr/agena"
export AGENATARGET_BIN="$AGENA_INSTALL_DIR"
export AGENATARGET_LIB="$AGENA_INSTALL_DIR/lib"
export AGENATARGET_DATA="$AGENA_INSTALL_DIR/data"
export AGENATARGET_DOC="$AGENA_INSTALL_DIR/doc"
export AGENATARGET_SHARE="$AGENA_INSTALL_DIR/share"

# 3. Cleanup
echo "Cleaning up old payload..."
sudo rm -rf "$AGENATARGET_ROOT/usr"

# Restore the full folder structure
mkdir -p "$AGENATARGET_BIN"
mkdir -p "$AGENATARGET_LIB"
mkdir -p "$AGENATARGET_DATA"
mkdir -p "$AGENATARGET_DOC"
mkdir -p "$AGENATARGET_SHARE/fractint"
mkdir -p "$AGENATARGET_SHARE/icons"
mkdir -p "$AGENATARGET_SHARE/schemes"
mkdir -p "$AGENATARGET_SHARE/scripting"
mkdir -p "$AGENATARGET_ROOT/usr/bin"
mkdir -p "$AGENATARGET_ROOT/usr/lib"

sudo chown -R $BUILD_USER:$BUILD_USER "$AGENATARGET_ROOT/usr"

# 4. Payload / Binaries
echo "Packing binaries..."
cp -p "$AGENAHOME/src/agena" "$AGENATARGET_BIN/"
cp -p "$AGENAHOME/../fltk-1.4.4/editor/agenaedit" "$AGENATARGET_BIN/"
cp -p "/usr/local/bin/srglue"  "$AGENATARGET_BIN/"
cp -p "/usr/local/bin/sragena" "$AGENATARGET_BIN/"

sudo chown $BUILD_USER:$BUILD_USER "$AGENATARGET_BIN/sr"*

# 4. Payload / Libraries
echo "Packing specific libraries..."
for libfile in abci.so abci.agn iconv.agn iconv.so ads.agn ads.so agenaini.spl \
  ansi.agn astro.agn astro.so com.so cordic.so curses.agn curses.so divs.agn \
  double.agn double.so fastmath.so fractals.agn fractals.so gdi.agn gdi.so \
  gzip.so ival.so ival.agn kiss.agn library.agn maple.agn mapm.agn mapm.so \
  minizip.so gmp.agn gmp.so mpfr.agn mpfr.so net.agn net.so skycrane.agn \
  skycrane.so sqlite.so tar.agn telex.agn testlib.so unimath.agn zx.so \
  strhash.so strmap.so;
do
  if [ -f "$AGENAHOME/lib/$libfile" ]; then
    cp -p "$AGENAHOME/lib/$libfile" "$AGENATARGET_LIB/"
  fi
done

# 4. Payload / Documentation
echo "Packing official documentation..."
for docfile in agena-crashcourse.pdf agena-primer.pdf agena-reference.pdf \
  agena-quickref.xls ascii.xls regex.txt SQLite.htm SQLite.agn;
do
  if [ -f "$AGENAHOME/doc/$docfile" ]; then
    cp -p "$AGENAHOME/doc/$docfile" "$AGENATARGET_DOC/"
  fi
done

cp -p "$AGENAHOME/change.log" "$AGENA_INSTALL_DIR/"
cp -p "$AGENAHOME/licence" "$AGENA_INSTALL_DIR/"
cp -p "$AGENAHOME/ports/sragena-105/README.srglue" "$AGENA_INSTALL_DIR/"

# 4. Payload / Assets
echo "Packing assets..."
rsync -a --exclude='cities' "$AGENAHOME/data/" "$AGENATARGET_DATA/"

# 4. Payload / Share content
echo "Packing shared assets (icons, schemes, scripting)..."
[ -d "$AGENAHOME/share/fractint" ] && cp -rp "$AGENAHOME/share/fractint/"* "$AGENATARGET_SHARE/fractint/"
[ -d "$AGENAHOME/share/icons" ] && cp -rp "$AGENAHOME/share/icons/"* "$AGENATARGET_SHARE/icons/"
[ -d "$AGENAHOME/share/schemes" ] && cp -rp "$AGENAHOME/share/schemes/"* "$AGENATARGET_SHARE/schemes/"
[ -d "$AGENAHOME/share/scripting" ] && cp -rp "$AGENAHOME/share/scripting/"* "$AGENATARGET_SHARE/scripting/"

# 4. Payload / System Libraries
echo "Packing system libraries..."
if [ -f "$AGENAHOME/src/libagena.so" ]; then
  cp -p "$AGENAHOME/src/libagena.so" "$AGENATARGET_ROOT/usr/lib/"
fi
if [ -f "$AGENAHOME/src/libagena.a" ]; then
  cp -p "$AGENAHOME/src/libagena.a" "$AGENATARGET_ROOT/usr/lib/"
fi

# 5. The Symbolic Links & Wrapper Logic
echo "Creating seamless permission wrapper for /usr/bin..."

# Clean up any existing system links first
rm -f "$AGENATARGET_ROOT/usr/bin/agena"
rm -f "$AGENATARGET_ROOT/usr/bin/agenaedit"
rm -f "$AGENATARGET_ROOT/usr/bin/srglue"
rm -f "$AGENATARGET_ROOT/usr/bin/sragena"

# Create a clever wrapper script for agena to handle permissions instantly
cat << 'EOF' > "$AGENATARGET_ROOT/usr/bin/agena"
#!/bin/sh
# Check if the user has write permissions to the index folder
if [ ! -w "/usr/agena/data" ] || [ ! -w "/usr/agena/data/cities.kdx" ]; then
    # Silently correct permissions using fallback user state if root access isn't active
    sudo chmod 777 /usr/agena/data 2>/dev/null || true
    sudo chmod 666 /usr/agena/data/* 2>/dev/null || true
fi
# Fire up the actual application with all arguments preserved
exec /usr/agena/agena "$@"
EOF

# Create standard symbolic links for the remaining helper apps
ln -s "/usr/agena/agenaedit" "$AGENATARGET_ROOT/usr/bin/agenaedit"
ln -s "/usr/agena/srglue"    "$AGENATARGET_ROOT/usr/bin/srglue"
ln -s "/usr/agena/sragena"   "$AGENATARGET_ROOT/usr/bin/sragena"

# 6. Final Packaging Security Setup
echo "Finalizing package permissions..."
chmod -R 755 "$AGENATARGET_ROOT/usr"

echo "Building Debian Package..."
cd "$MAINTARGETDIR"
dpkg-deb --root-owner-group --build agena "agena-$AGENAVER-raspi.stretch.armv6-32.deb"

echo "Build Complete!"
