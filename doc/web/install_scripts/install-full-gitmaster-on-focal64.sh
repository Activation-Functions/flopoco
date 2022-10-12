#!/bin/bash

# install the dependencies that are available in a Debian distribution
yes | sudo apt update && sudo DEBIAN_FRONTEND=noninteractive apt install -y git cmake ninja sollya wget g++ libsollya-dev flex bison libboost-all-dev autotools-dev autoconf automake f2c libblas-dev liblapack-dev libtool liblpsolve55-dev lp-solve


git clone https://gitlab.com/flopoco/flopoco

cd flopoco
mkdir build
cd build

cmake -GNinja ..
ninja

# Old-style make also works
#  cmake ..
#  make

# executables are in build/code/FloPoCoBin/

cd ..
ln -s ./build/code/FloPoCoBin/flopoco .

# build the html documentation in doc/web. 
./flopoco BuildHTMLDoc

# Pure luxury: bash autocompletion. 
./flopoco BuildAutocomplete
mkdir ~/.bash_completion.d/
mv flopoco_autocomplete ~/.bash_completion.d/flopoco
# The following line should be called only once
echo ". ~/.bash_completion.d/flopoco" >> ~/.bashrc

# Now show the list of operators
./flopoco  

echo
echo "If you saw the command-line help of flopoco, welcome to FloPoCo !"
