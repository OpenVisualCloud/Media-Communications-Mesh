# !/bin/bash -e


VERSION="2.14.1"
wget https://github.com/akheron/jansson/releases/download/v${VERSION}/jansson-${VERSION}.tar.gz
tar -xvzf jansson-${VERSION}.tar.gz
cd jansson-${VERSION} 
mkdir build
cd build
cmake -DBUILD_IN_SOURCE=0 \
      -DJANSSON_BUILD_DOCS=0 \
      -DJANSSON_BUILD_EXAMPLES=0 \
      -DJANSSON_BUILD_SHARED_LIBS=1 ..
make
make check
sudo make install

cd ..

sudo rm -rf jansson-${VERSION}
rm -rf jansson-${VERSION}.tar.gz


