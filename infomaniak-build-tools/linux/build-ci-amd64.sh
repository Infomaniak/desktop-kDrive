#! /bin/bash

set -xe

export QT_BASE_DIR="~/Qt/6.2.3"
export QTDIR="$QT_BASE_DIR/gcc_64"
export BASEPATH=$PWD
export CONTENTDIR="$BASEPATH/build-linux"
export INSTALLDIR="$CONTENTDIR/install"
export BUILDDIR="$CONTENTDIR/build"
export APPDIR="$CONTENTDIR/app"


mkdir -p $APPDIR
mkdir -p $BUILDDIR
mkdir -p $INSTALLDIR

export QMAKE=$QTDIR/bin/qmake
export PATH=$QTDIR/bin:$QTDIR/libexec:$PATH
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
export SUFFIX=""

# Build client
cd $BUILDDIR
mkdir -p $BUILDDIR/client

CMAKE_PARAMS=()

export KDRIVE_DEBUG=0

cmake -B$BUILDDIR -H$BASEPATH \
    -DOPENSSL_ROOT_DIR=/usr/local \
    -DOPENSSL_INCLUDE_DIR=/usr/local/include \
    -DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib64/libcrypto.so \
    -DOPENSSL_SSL_LIBRARY=/usr/local/lib64/libssl.so \
    -DQT_FEATURE_neon=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$BASEPATH \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBIN_INSTALL_DIR=$BUILDDIR/client \
    -DKDRIVE_VERSION_SUFFIX=$SUFFIX \
    -DKDRIVE_THEME_DIR="$BASEPATH/infomaniak" \
    -DWITH_CRASHREPORTER=0 \
    -DKDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
    -DBUILD_UNIT_TESTS=1 \
    "${CMAKE_PARAMS[@]}" \

make -j4
make DESTDIR=$APPDIR install

cp $BASEPATH/sync-exclude-linux.lst $BUILDDIR/bin/sync-exclude.lst
