
FLOPOCO_VERSION_MAJOR   := 5
FLOPOCO_VERSION_MINOR   := 0
FLOPOCO_VERSION_PATCH   := 0
FLOPOCO_VERSION         := $(FLOPOCO_VERSION_MAJOR).$(FLOPOCO_VERSION_MINOR)
FLOPOCO_VERSION_FULL    := $(FLOPOCO_VERSION).$(FLOPOCO_VERSION_PATCH)
FLOPOCO_BRANCH          := $(shell git symbolic-ref --short HEAD)
FLOPOCO_COMMIT_HASH     := $(shell git rev-parse HEAD)

MKROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DEPENDENCIES_DIR := $(MKROOT)/build/dependencies
BUILD_DEPENDENCIES_SOURCE_DIR := $(BUILD_DEPENDENCIES_DIR)/src
BUILD_DEPENDENCIES_BINARY_DIR := $(BUILD_DEPENDENCIES_DIR)/bin

CMAKE_GENERATOR ?= Ninja
PREFIX ?= /usr/local
SCALP_BACKEND ?= SCIP

ifneq ($(CONFIG), docker)
    SUDO := sudo
endif

include $(MKROOT)/tools/utilities.mk

$(call static_ok, Running $(B)FloPoCo$(N) build script\
    ($(B)v$(FLOPOCO_VERSION_FULL)$(N)) on $(B)$(OS)$(N)  \
    ($(OS_VERSION) $(OS_LTS)))

$(call static_ok, Branch $(B)$(FLOPOCO_BRANCH)$(N))
$(call static_ok, Commit $(B)#$(FLOPOCO_COMMIT_HASH)$(N))
$(call static_ok, Running $(B)from$(N): $(PWD))
$(call static_ok, $(B)Make targets$(N): $(MAKECMDGOALS))
$(call static_ok, $(B)ScaLP backend$(N): $(SCALP_BACKEND))

# -----------------------------------------------------------------------------

all: flopoco

# -----------------------------------------------------------------------------
.PHONY: help
# -----------------------------------------------------------------------------
help:
	$(call shell_info,)

# -----------------------------------------------------------------------------
.PHONY: clean
# -----------------------------------------------------------------------------
clean:
	@rm -rf build

# -----------------------------------------------------------------------------
.PHONY: docker
# -----------------------------------------------------------------------------

DOCKER_IMAGE ?= debian
DOCKER_IMAGE_SUPPORTED := ubuntu debian archlinux alpine
DOCKERFILE := $(MKROOT)/Dockerfile
FLOPOCO_DOCKER_TAG := flopoco:$(FLOPOCO_VERSION_FULL)-$(SCALP_BACKEND)-$(DOCKER_IMAGE)

# -------------------------------------------------------------------------------
ifeq ($(DOCKER_IMAGE), debian)
# -------------------------------------------------------------------------------
define docker_script
    FROM debian:latest
    RUN apt update
    RUN yes | apt install git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco && git checkout dev/cmake && make && make install
endef
# -------------------------------------------------------------------------------
else ifeq ($(DOCKER_IMAGE), ubuntu)
# -------------------------------------------------------------------------------
define docker_script
    FROM ubuntu:latest
    ARG DEBIAN_FRONTEND=noninteractive
    RUN apt update
    RUN yes | apt install git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco && git checkout dev/cmake && make && make install
endef
# -------------------------------------------------------------------------------
else ifeq ($(DOCKER_IMAGE), archlinux)
# -------------------------------------------------------------------------------
define docker_script
    FROM greyltc/archlinux-aur:yay
    RUN yay --noconfirm
    RUN yay --noconfirm -S git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco && git checkout dev/cmake && make && make install
endef
endif

$(DOCKERFILE):
	$(call shell_info, Generating $(B)Dockerfile$(N))
	@echo '$(call docker_script)' > $(DOCKERFILE)

docker: $(DOCKERFILE)
	$(call shell_info, Building docker image $(B)$(FLOPOCO_DOCKER_TAG)$(N))
	@docker build --no-cache					    \
		      --build-arg SCALP_BACKEND=$(SCALP_BACKEND)	    \
		       -t flopoco:$(FLOPOCO_VERSION_FULL)-$(SCALP_BACKEND)  \
		      $(MKROOT)

# -----------------------------------------------------------------------------
.PHONY: sysdeps
# -----------------------------------------------------------------------------

# -----------------------------------------------------------
ifeq ($(OS), $(filter $(OS), Ubuntu Debian))
# -----------------------------------------------------------
    SYSDEPS += git
    SYSDEPS += g++
    SYSDEPS += cmake
    SYSDEPS += ninja-build
    SYSDEPS += libgmp-dev
    SYSDEPS += libmpfi-dev
    SYSDEPS += libmpfr-dev
    SYSDEPS += libsollya-dev
    SYSDEPS += dh-autoreconf
    SYSDEPS += libf2c2-dev
    SYSDEPS += flex
    SYSDEPS += libboost-all-dev
    SYSDEPS += pkg-config

define sysdeps_cmd
    $(SUDO) apt update
    yes | $(SUDO) apt install $(SYSDEPS)
endef

# -----------------------------------------------------------
else ifeq ($(OS), Arch)
# Using AUR with docker::
# docker pull greyltc/archlinux-aur:yay
# -----------------------------------------------------------
    SYSDEPS += git
    SYSDEPS += gcc
    SYSDEPS += cmake
    SYSDEPS += ninja
    SYSDEPS += gmp
    SYSDEPS += mpfr
    SYSDEPS += mpfi
    SYSDEPS += flex
    SYSDEPS += boost
    SYSDEPS += pkgconf
    SYSDEPS += sollya-git
    SYSDEPS += f2c

define sysdeps_cmd
    yay -Syu
    yay -S $(SYSDEPS)
endef

# -----------------------------------------------------------
else ifeq ($(OS), Alpine)
# -----------------------------------------------------------
    SYSDEPS += git
    SYSDEPS += gcc
    SYSDEPS += cmake
    SYSDEPS += automake
    SYSDEPS += libtool
    SYSDEPS += ninja
    SYSDEPS += gmp-dev
    SYSDEPS += mpfr-dev
    SYSDEPS += boost-dev
    SYSDEPS += flex
    SYSDEPS += bison
    SYSDEPS += gnuplot
    SYSDEPS += pkgconf
    SYSDEPS += libxml2-dev
    SYSDEPS += libmpfi libmpfi-static # edge/testing
# -----------------------------------------------------------
else ifeq ($(OS), Darwin)
# macOS: homebrew (Anastasia) or macports (Martin)
MACOS_PKG_MANAGER ?= brew
# -----------------------------------------------------------
    SYSDEPS += git          # brew: ok | macports: ok
    SYSDEPS += wget         # brew: ok | macports: ok
    SYSDEPS += make         # brew: ok | macports: gmake
    SYSDEPS += cmake        # brew: ok | macports: cmake
    SYSDEPS += gmp          # brew: ok | macports: ok
    SYSDEPS += mpfr         # brew: ok | macports: ok
    SYSDEPS += mpfi         # brew: ok | macports: ok
    SYSDEPS += sollya       # brew: ok | macports: nope
    SYSDEPS += pkg-config   # brew: ok | macports: pkgconfig
    SYSDEPS += boost        # brew: ok | macports: ok
    SYSDEPS += autoconf     # brew: ok | macports: ok
    SYSDEPS += automake     # brew: ok | macports: ok
    SYSDEPS += libtool      # brew: ok | macports: ok
    SYSDEPS += f2c          # brew: nope | macports: ok
    SYSDEPS += lapack       # brew: ok
    SYSDEPS += openblas     # brew: ok
# -----------------------------------------------------------
else
# -----------------------------------------------------------
    SYSDEPS_CMD += $(call shell_info, Linux distribution could not be identified, skipping...)
endif

sysdeps:
	$(call shell_info, Updating $(OS) system $(B)dependencies$(N): $(SYSDEPS))
	$(call sysdeps_cmd)

# build fplll manually ?
# for building sollya: autoreconf -fi ./configure and make

# -----------------------------------------------------------------------------
.PHONY: soplex
# -----------------------------------------------------------------------------
SOPLEX_GIT := https://github.com/scipopt/soplex.git
SOPLEX_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/soplex
SOPLEX_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/soplex
SOPLEX := $(SOPLEX_BINARY_DIR)/lib/libsoplex.a

soplex: $(SOPLEX)

.ONESHELL:
$(SOPLEX):
	$(call shell_info, Fetching and building $(B)SOPLEX$(N) library)
	@mkdir -p $(SOPLEX_BINARY_DIR)
	@git clone $(SOPLEX_GIT) $(SOPLEX_SOURCE_DIR)
	@cd $(SOPLEX_SOURCE_DIR)
	@cmake -B build -G$(CMAKE_GENERATOR) \
	       -DCMAKE_INSTALL_PREFIX=$(SOPLEX_BINARY_DIR)
	@cmake --build build --target install

# -----------------------------------------------------------------------------
.PHONY: scip
# -----------------------------------------------------------------------------
SCIP_GIT := https://github.com/scipopt/scip.git
SCIP_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/scip
SCIP_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/scip
SCIP := $(SCIP_BINARY_DIR)/lib/libscip.so

.ONESHELL:
$(SCIP): $(SOPLEX)
	$(call shell_info, Fetching and building $(B)SCIP$(N) library)
	@mkdir -p $(SCIP_BINARY_DIR)
	@git clone $(SCIP_GIT) $(SCIP_SOURCE_DIR)
	@cd $(SCIP_SOURCE_DIR)
	@cmake -B build -G$(CMAKE_GENERATOR)		    \
	       -DSOPLEX_DIR=$(SOPLEX_BINARY_DIR)	    \
	       -DAUTOBUILD=ON				    \
	       -DCMAKE_INSTALL_PREFIX=$(SCIP_BINARY_DIR)
	@cmake --build build --target install

# -----------------------------------------------------------------------------
.PHONY: gurobi
# -----------------------------------------------------------------------------
# Gurobi (TODO):
# - Linux(x86): https://packages.gurobi.com/11.0/gurobi11.0.2_linux64.tar.gz
# - Linux(arm): https://packages.gurobi.com/11.0/gurobi11.0.2_armlinux64.tar.gz
# -- macOS: https://packages.gurobi.com/11.0/gurobi11.0.2_macos_universal2.pkg

# -----------------------------------------------------------------------------
.PHONY: scalp
# -----------------------------------------------------------------------------
SCALP_GIT := https://digidev.digi.e-technik.uni-kassel.de/git/scalp.git
SCALP_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/scalp
SCALP_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/scalp
SCALP_CMAKE_PATCH := $(MKROOT)/tools/scalp_fpc.patch
SCALP := $(SCALP_BINARY_DIR)/lib/libScaLP.so

SCALP_DEPENDENCIES += $(SCALP_CMAKE_PATCH)
SCALP_CMAKE_OPTIONS += -DUSE_LPSOLVE=OFF

ifdef WITH_GUROBI # ---------------
    SCALP_DEPENDENCIES += $(GUROBI)
else # ----------------------------
    SCALP_DEPENDENCIES += $(SCIP)
    SCALP_CMAKE_OPTIONS += -DSCIP_DIR=$(SCIP_BINARY_DIR)
    SCALP_CMAKE_OPTIONS += -DSOPLEX_DIR=$(SOPLEX_BINARY_DIR)
endif # ---------------------------

scalp: $(SCALP)

.ONESHELL:
$(SCALP): $(SCALP_DEPENDENCIES)
	$(call shell_info, Fetching and building $(B)ScaLP$(N) library)
	@mkdir -p $(SCALP_BINARY_DIR)
	@git clone $(SCALP_GIT) $(SCALP_SOURCE_DIR)
	@cd $(SCALP_SOURCE_DIR)
	@patch -p0 -f CMakeLists.txt $(SCALP_CMAKE_PATCH)
	@cmake -B build -G$(CMAKE_GENERATOR)		    \
	       -DCMAKE_INSTALL_PREFIX=$(SCALP_BINARY_DIR)   \
	       $(SCALP_CMAKE_OPTIONS)
	@cmake --build build --target install

# -----------------------------------------------------------------------------
.PHONY: wcpg
# -----------------------------------------------------------------------------
WCPG_GIT := https://github.com/fixif/WCPG
WCPG_COMMIT_HASH := b90253a4a6a650300454f5656a7e8410e0493175
WCPG_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/wcpg
WCPG_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/wcpg
WCPG := $(WCPG_BINARY_DIR)/lib/libwcpg.a
WCPG_CONFIGURE_FLAGS += CFLAGS=-I/opt/local/include

# ----------------------------------------------------------------
.PHONY: f2c
# ----------------------------------------------------------------
# on a few distributions (macOS) f2c is not available as a package

wcpg: $(WCPG)

.ONESHELL:
$(WCPG):
	$(call shell_info, Fetching and building $(B)WCPG$(N) library)
	@mkdir -p $(WCPG_BINARY_DIR)
	@git clone $(WCPG_GIT) $(WCPG_SOURCE_DIR)
	@cd $(WCPG_SOURCE_DIR)
	@git checkout $(WCPG_COMMIT_HASH)
	@bash autogen.sh
	@./configure --prefix=$(WCPG_BINARY_DIR) $(WCPG_CONFIGURE_FLAGS)
	@make -j8 install

# -----------------------------------------------------------------------------
.PHONY: pagsuite
# -----------------------------------------------------------------------------
PAGSUITE_GIT := https://gitlab.com/kumm/pagsuite.git
PAGSUITE_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/pagsuite
PAGSUITE_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/pagsuite
PAGSUITE := $(PAGSUITE_BINARY_DIR)/lib/librpag.so
PAGSUITE_CMAKE_PATCH := $(MKROOT)/tools/pagsuite_fpc.patch

pagsuite: $(PAGSUITE)

.ONESHELL:
$(PAGSUITE): $(SCALP)
	$(call shell_info, Fetching and building $(B)PAGSuite$(N) library)
	@mkdir -p $(PAGSUITE_BINARY_DIR)
	@git clone $(PAGSUITE_GIT) $(PAGSUITE_SOURCE_DIR)
	@cd $(PAGSUITE_SOURCE_DIR)
	@patch -p0 -f CMakeLists.txt $(PAGSUITE_CMAKE_PATCH)
	@cmake -B build -G$(CMAKE_GENERATOR)			\
	       -DSCALP_PREFIX_PATH=$(SCALP_BINARY_DIR)		\
	       -DCMAKE_INSTALL_PREFIX=$(PAGSUITE_BINARY_DIR)
	@cmake --build build --target install

# -----------------------------------------------------------------------------
.PHONY: dependencies
# -----------------------------------------------------------------------------
FLOPOCO_DEPENDENCIES += sysdeps
FLOPOCO_DEPENDENCIES += $(SCALP)
FLOPOCO_DEPENDENCIES += $(WCPG)
FLOPOCO_DEPENDENCIES += $(PAGSUITE)

dependencies: $(FLOPOCO_DEPENDENCIES)

# -----------------------------------------------------------------------------
.PHONY: flopoco
# -----------------------------------------------------------------------------

FLOPOCO := $(MKROOT)/build/code/FloPoCoBin/flopoco
flopoco: $(FLOPOCO)

$(FLOPOCO): $(FLOPOCO_DEPENDENCIES)
	$(call shell_info, Now building $(B)FloPoCo$(N))
	@cmake -B build -G$(CMAKE_GENERATOR)	    \
	       -DWCPG_LOCAL=$(WCPG_BINARY_DIR)	    \
	       -DSCALP_LOCAL=$(SCALP_BINARY_DIR)    \
	       -DPAG_LOCAL=$(PAGSUITE_BINARY_DIR)   \
	       -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	@cmake --build build

# -----------------------------------------------------------------------------
.ONESHELL:
.PHONY: install
# -----------------------------------------------------------------------------
install: $(FLOPOCO)
	@cd $(MKROOT)
	@cmake --build build --target install

