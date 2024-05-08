#!/usr/bin/env bash

export APP_DIR=$PWD/build-linux/app
export BASE_DIR=$PWD

cd $APPDIR

export QT_BASE_DIR=$HOME/Qt/6.2.3/gcc_64
export QTDIR=$QT_BASE_DIR
export QMAKE=$QT_BASE_DIR/bin/qmake
export PATH=$QT_BASE_DIR/bin:$QT_BASE_DIR/libexec:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib:$APPDIR/usr/lib:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

mkdir -p $APP_DIR/usr/plugins

cp -P -r $QTDIR/plugins/* $APP_DIR/usr/plugins/
cp -P -r $QTDIR/libexec $APP_DIR/usr
cp -P -r $QTDIR/resources $APP_DIR/usr
cp -P -r $QTDIR/translations/ $APP_DIR/usr

mv $APP_DIR/usr/lib/x86_64-linux-gnu/* $APP_DIR/usr/lib/

mkdir -p $APP_DIR/usr/qml

rm -rf $APP_DIR/usr/lib/x86_64-linux-gnu/
rm -rf $APP_DIR/usr/include/

cp $BASE_DIR/sync-exclude-linux.lst $APP_DIR/usr/bin/sync-exclude.lst
cp $APP_DIR/usr/share/icons/hicolor/512x512/apps/kdrive-win.png $APP_DIR

cp -P /usr/local/lib64/libssl.so* $APP_DIR/usr/lib/
cp -P /usr/local/lib64/libcrypto.so* $APP_DIR/usr/lib/
cp -P -r /usr/lib/x86_64-linux-gnu/nss/ $APP_DIR/usr/lib/
cp ~/Qt/Tools/QtCreator/lib/Qt/lib/libQt6SerialPort.so.6 $APP_DIR/usr/lib/

$HOME/desktop-setup/linuxdeploy-x86_64.AppImage --appdir $APP_DIR -e $APP_DIR/usr/bin/kDrive -i $APP_DIR/kdrive-win.png -d $APP_DIR/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0
