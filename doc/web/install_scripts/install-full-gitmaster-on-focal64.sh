#!/bin/bash

# install the dependencies that are available in a Debian distribution
 
yes | sudo apt update && \
      sudo DEBIAN_FRONTEND=noninteractive apt install -y autoconf automake autotools-dev bison f2c flex git gpg g++ libblas-dev libboost-all-dev liblapack-dev liblpsolve55-dev libsollya-dev libtool lp-solve ninja-build pkg-config sollya wget && \
      wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null && \
      echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null && \
      sudo apt update && \
      sudo DEBIAN_FRONTEND=noninteractive apt install -y kitware-archive-keyring cmake


git clone https://gitlab.com/flopoco/flopoco
cd flopoco
# Remove this before merge:
git checkout merge/master-cmake
make
