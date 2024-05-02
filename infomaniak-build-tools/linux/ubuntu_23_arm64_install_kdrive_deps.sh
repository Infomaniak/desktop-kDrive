#!/bin/bash

# Install desktop-kDrive dependencies for development purposes
# on Ubuntu 23.10.1 Mantic Minautor for arch. arm64 

set -e

sudo apt-get install xclip 1> /dev/null

exec 3>&1 > /tmp/kdrive_env_installer.log 2>&1

function install_kdrive_dependencies(){
	echo "Installing common kdrive dependencies" >&3
	
	echo "- most common packages" >&3
	sudo apt update --fix-missing
	sudo apt install --yes \
		wget \
		curl \
		pkg-config \
		ninja-build \
		software-properties-common \
		libkf5kio-dev \
		git

	echo "- zlib" >&3
	sudo apt update --fix-missing
	sudo apt install --yes \
		zlib1g \
		zlib1g-dev

	echo "- sqlite3 dev files" >&3
	sudo apt update && sudo apt install --yes \
		libsqlite3-dev

	echo "- mesa-dev-common" >&3
	sudo apt update && sudo apt install --yes \
		mesa-common-dev
		
	sudo apt-get install -y libxkbcommon-dev \
		glib-2.0 \
		libsecret-1-dev
		
	echo "done." >&3
}

function install_cmake_packages () {
	echo "Installing cmake" >&3

	sudo apt update --fix-missing
	sudo apt install --yes cmake extra-cmake-modules
}

function install_qt_packages() {
	echo "Installing Qt packages" >&3
	
	sudo apt update
	sudo apt install --yes \
		qt6-webengine-dev \
		qt6-positioning-dev \
		qt6-webview-dev \
		qt6-tools-dev \
		qt6-webengine-dev-tools \
		libqt6core5compat6-dev \
		libqt6svg6-dev \
		linguist-qt6 \
		qt6-l10n-tools \
		qt6-tools-dev-tools \
		qtcreator
}

function install_cppunit() {
	echo "Installing cppunit" >&3

	echo "- cppunit dependencies ..." >&3
	sudo apt-get install -y autotools-dev
	sudo apt-get install -y automake
	sudo apt-get install -y libtool m4 automake

	cd $HOME/Projects
	
	echo "- cloning ..." >&3
	git clone git://anongit.freedesktop.org/git/libreoffice/cppunit/ && \
	cd cppunit && \
	
	echo "- configuring ..." >&3
	./autogen.sh && \
	./configure && \
	
	echo "- building ..." >&3
	make
	
	echo "- installing ..." >&3
	sudo make install
	
	echo "done." >&3
}

function install_logc4plus(){
	echo "Installing log4cplus" >&3
	
	cd /tmp

	echo " - cloning ..." >&3
	git clone --recurse-submodules https://github.com/log4cplus/log4cplus.git
	cd log4cplus
	git checkout 2.1.x

	echo " - building ..." >&3
	mkdir build
	cd build

	echo " - installing ..." >&3
	cmake .. -DUNICODE=1
	sudo cmake --build . --target install
	
	rm -rf /tmp/log4cplus
	
	echo "- done." >&3
}

function install_openssl(){
	echo "Installing openssl" >&3

	cd /tmp

	echo "- cloning ..." >&3
	git clone git://git.openssl.org/openssl.git
	cd openssl
	git checkout openssl-3.1

	echo "- configuring ..." >&3
	./Configure shared

	echo "- building ..." >&3
	make

	echo "- installing ..." >&3
	sudo make install
	
	rm -rf /tmp/openssl
	
    echo "- done." >&3
}

function install_poco(){
	echo "Installing poco" >&3

	cd /tmp 

	echo "- cloning ..." >&3
	git clone  https://github.com/pocoproject/poco.git
	cd poco
	git checkout poco-1.12.5

	echo "- building ..." >&3
	mkdir cmake-build
	cd cmake-build
	cmake .. -DOPENSSL_ROOT_DIR=/usr/local \
		-DOPENSSL_INCLUDE_DIR=/usr/local/include \
		-DOPENSSL_LIBRARIES=/usr/local/lib \
		-DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib/libcrypto.so \
		-DOPENSSL_SSL_LIBRARY=/usr/local/lib/libssl.so

	echo "- installing ..." >&3
	sudo cmake --build . --target install
	
	rm -rf /tmp/poco
	
	echo "- done." >&3
}

function install_sentry(){
	echo "Installing sentry" >&3

	echo "- downloading ..." >&3
	sudo apt-get install curl
	sudo apt-get install libcurl4-openssl-dev

	cd /tmp

	git clone https://github.com/getsentry/sentry-native.git
	cd sentry-native
	git checkout tags/0.5.4
	git submodule init
	git submodule update --recursive

	echo "- building ..." >&3
	cmake -B build \
		-DSENTRY_INTEGRATION_QT=YES \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo

	cmake --build build --parallel

	echo "- installing ..." >&3
	sudo cmake --install build
	
	rm -rf /tmp/sentry-native
	
	echo "- done." >&3
}

function install_xxhash(){
	echo "Installing xxhash" >&3

	cd /tmp

	echo "- cloning ..." >&3
	git clone  https://github.com/Cyan4973/xxHash.git
	cd xxHash/cmake_unofficial

	echo "- building ..." >&3
	mkdir build
	cd build 
	cmake ..
	sudo cmake --build . --target install
  
	rm -rf /tmp/xxHash
	
	echo "- done." >&3
}

install_kdrive_dependencies
install_cmake_packages
install_qt_packages
install_logc4plus
install_openssl
install_poco
install_sentry
install_xxhash

echo "Development environment successully installed." >&3
