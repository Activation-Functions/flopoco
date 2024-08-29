#!/bin/bash

yes | sudo apt-get update && sudo apt-get install g++ libgmp3-dev libmpfr-dev libfplll-dev libxml2-dev bison libmpfi-dev flex cmake libboost-all-dev libgsl0-dev

wget https://www.sollya.org/releases/sollya-4.1/sollya-4.1.tar.gz && tar xzf sollya-4.1.tar.gz && cd sollya-4.1/   && ./configure && make  && sudo make install   && cd ..

git clone https://gitlab.com/flopoco/flopoco

cd flopoco

# this should not be necessary, change this once master is working again!
git checkout dev/master

mkdir build
cd build

cmake -GNinja ..
ninja

# Old-style make also works
#  cmake ..
#  make

# executables are in bin/flopoco
ln -s ./bin/flopoco

# build the html documentation in doc/web. 
./flopoco BuildHTMLDoc

# Pure luxury: bash autocompletion. This should be called only once
./flopoco BuildAutocomplete
mkdir ~/.bash_completion.d/
mv flopoco_autocomplete ~/.bash_completion.d/flopoco
echo ". ~/.bash_completion.d/flopoco" >> ~/.bashrc

# Now show the list of operators
./flopoco  
