#!/bin/bash

# Install all dependencies
sudo dnf update -y && \
sudo dnf install -y autoconf automake bison f2c flex git gpg g++ \ 
blas-devel boost-devel lapack-devel lpsolve-devel libtool lpsolve \
ninja-build pkg-config wget cmake \ 
gmp-devel mpfr-devel mpfi-devel libxml2-devel libfplll-devel # dependencies for Sollya


# Build Sollya
git clone https://gitlab.inria.fr/sollya/sollya.git && \
cd sollya && \
./autogen.sh && \
./configure && \
sudo make install

cd ..

# Build flopoco
git clone https://gitlab.com/flopoco/flopoco
cd flopoco
make


