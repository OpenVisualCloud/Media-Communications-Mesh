# !/bin/bash -e


VERSION="2.14.1"
wget https://github.com/akheron/jansson/releases/download/v${VERSION}/jansson-${VERSION}.tar.gz
tar -xvzf jansson-${VERSION}.tar.gz
cd jansson-${VERSION} 
./configure
make
sudo make install

cd ../..