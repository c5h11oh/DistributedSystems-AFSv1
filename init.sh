#!/bin/bash
export INSTALL_PATH="/p2/installs"

# install
sudo apt-get update
sudo apt-get install -y build-essential wget gcc g++ gdb git vim htop curl autoconf libtool pkg-config fuse libfuse-dev libssl-dev
## get a newer cmake
sudo apt-get remove cmake
wget https://github.com/Kitware/CMake/releases/download/v3.22.2/cmake-3.22.2-linux-x86_64.sh
/bin/sh cmake-3.22.2-linux-x86_64.sh -- --skip-license --prefix=$INSTALL_PATH
# cp -r cmake-3.22.2-linux-x86_64/* $INSTALL_PATH/
echo "PATH=$INSTALL_PATH/bin:\$PATH" >> ~/.bashrc
source ~/.bashrc
echo "set tabstop=4" > ~/.vimrc

# git
git config --global user.name "Sven Hwang"
git config --global user.email "sven.chwen@gmail.com"

echo "[adding repo deploy key]"
echo "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIWkwKvGF4Qmg0ZNFgQTZwcv5pdREjezPQ66RXJsYMLS sven.chwen@gmail.com"> ~/.ssh/id_cs739_afsv1_deploy_key.pub
echo -e "-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW
QyNTUxOQAAACCFpMCrxheEJoNGTRYEE2cHL+aXURI3sz0OukVybGDC0gAAAJiyCmN7sgpj
ewAAAAtzc2gtZWQyNTUxOQAAACCFpMCrxheEJoNGTRYEE2cHL+aXURI3sz0OukVybGDC0g
AAAEBxwXYCyXgmFF7cVb/4MfG9Up9YMqU0p3nHCQSc5lrLGYWkwKvGF4Qmg0ZNFgQTZwcv
5pdREjezPQ66RXJsYMLSAAAAFHN2ZW4uY2h3ZW5AZ21haWwuY29tAQ==
-----END OPENSSH PRIVATE KEY-----" > ~/.ssh/id_cs739_afsv1_deploy_key
chmod 0600 ~/.ssh/id_cs739_afsv1_deploy_key
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_cs739_afsv1_deploy_key

# mkdir
sudo mkdir -p /p2
sudo chown c5h11oh /p2
mkdir -p $INSTALL_PATH /p2/repo /p2/grpc

## get a newer cmake
sudo apt-get remove cmake
wget https://github.com/Kitware/CMake/releases/download/v3.22.2/cmake-3.22.2-linux-x86_64.sh
/bin/sh cmake-3.22.2-linux-x86_64.sh -- --skip-license --prefix=$INSTALL_PATH
# cp -r cmake-3.22.2-linux-x86_64/* $INSTALL_PATH/
echo "PATH=$INSTALL_PATH/bin:\$PATH" >> ~/.bashrc
source ~/.bashrc
git clone git@github.com:c5h11oh/cs739-afsv1.git /p2/repo

# install grpc
# https://grpc.io/docs/languages/cpp/quickstart/
git clone --recurse-submodules -b v1.43.0 https://github.com/grpc/grpc /p2/grpc
cd /p2/grpc
mkdir -p cmake/build
pushd cmake/build
$INSTALL_PATH/bin/cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH \
      ../..
make -j
make install
popd

# linker
cd /etc/ld.so.conf.d
echo $INSTALL_PATH | sudo tee p2.conf
sudo /sbin/ldconfig

