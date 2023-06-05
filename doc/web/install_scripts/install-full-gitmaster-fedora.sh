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
mkdir build
cd build

cmake -GNinja .. && \
ninja

# executables are in build/code/FloPoCoBin/
ln -s ./code/FloPoCoBin/flopoco .

# build the html documentation in doc/web. 
./flopoco BuildHTMLDoc


# Pure luxury: bash autocompletion. 
./flopoco BuildAutocomplete

mkdir ~/.bash_completion.d/
# Ensure that update on the file will be updated on the computer ...
mv flopoco_autocomplete ~/.bash_completion.d/flopoco

# ... But won't add another rudundant line in the .bashrc
if [[ ! -d "~/.bash_completion.d/flopoco" ]] then	
	# The following line should be called only once
	echo ". ~/.bash_completion.d/flopoco" >> ~/.bashrc
fi

# Now show the list of operators
./flopoco  

echo
echo "If you saw the command-line help of flopoco, welcome to FloPoCo !"


