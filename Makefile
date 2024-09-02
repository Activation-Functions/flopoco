
GNU_MAKE_VERSION_MINIMUM := 4.3

ifeq ($(shell expr $(MAKE_VERSION) \< $(GNU_MAKE_VERSION_MINIMUM)), 1)
    $(error MAKE_VERSION should be at least >= $(GNU_MAKE_VERSION_MINIMUM) ($(MAKE_VERSION)))
endif

# -----------------------------------------------------------------------
# Note:
# -----------------------------------------------------------------------
# - VERSION_MAJOR should be incremented
#   whenever a big/revolutionary change to FloPoCo has been made.
# - VERSION_MINOR should be incremented whenever a new operator is added.
# - VERSION_PATCH when important bugfixes are made.
# -----------------------------------------------------------------------

FLOPOCO_VERSION_MAJOR   := 5
FLOPOCO_VERSION_MINOR   := 0
FLOPOCO_VERSION_PATCH   := 0
FLOPOCO_VERSION         := $(FLOPOCO_VERSION_MAJOR).$(FLOPOCO_VERSION_MINOR)
FLOPOCO_VERSION_FULL    := $(FLOPOCO_VERSION).$(FLOPOCO_VERSION_PATCH)
FLOPOCO_BRANCH          := $(shell git symbolic-ref --short HEAD)
FLOPOCO_COMMIT_HASH     := $(shell git rev-parse HEAD)

# Actual (absolute) path to the repository's root Makefile
MKROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# Local dependencies build directories:

BUILD_DIR := $(MKROOT)/build
BUILD_DEPENDENCIES_DIR := $(BUILD_DIR)/dependencies
BUILD_DEPENDENCIES_SOURCE_DIR := $(BUILD_DEPENDENCIES_DIR)/src
BUILD_DEPENDENCIES_BINARY_DIR := $(BUILD_DEPENDENCIES_DIR)/bin

# Utilities for parsing OS and distribution version
# as well as pretty-printing stuff.
include $(MKROOT)/tools/utilities.mk

# Print FloPoCo version, branch and commit hash:
$(call static_info, Running $(B)FloPoCo$(N) build script\
    ($(B)v$(FLOPOCO_VERSION_FULL)$(N)) on $(B)$(OS_PRETTY_NAME)$(N))

$(call static_info, Branch $(B)$(FLOPOCO_BRANCH)$(N))
$(call static_info, Commit $(B)#$(FLOPOCO_COMMIT_HASH)$(N))
$(call static_info, Running $(B)from$(N): $(PWD))

ifndef MAKECMDGOALS # ------------------------
    # This is just for info-printing purposes:
    MAKECMDGOALS := all
endif # --------------------------------------

$(call static_info, $(B)Make targets$(N): $(MAKECMDGOALS))

ifneq ($(CONFIG), docker) # -----------
    # don't sudo in docker environments
    SUDO := sudo
endif # -------------------------------

# Detect if ninja is available, otherwise, use Makefiles:
ifeq ($(shell which ninja),) # --------
    CMAKE_GENERATOR ?= "Unix Makefiles"
else # --------------------------------
    CMAKE_GENERATOR ?= Ninja
endif # -------------------------------

# GUROBI_HOME can be set as an environment variable in the user's shell
# resource file (as recommended in the Gurobi installation manual)
# But it can also be added on-the-fly as a Makefile argument, e.g.:
# $ make GUROBI_HOME=/opt/gurobi1102/linux64
GUROBI_ROOT_DIR := $(GUROBI_HOME)

# By default, install a symlink of 'flopoco' binary in $(PREFIX)
# when '$ make install' is used.
# Otherwise, install all libraries and binaries in $(PREFIX)
INSTALL_TYPE ?= symlink

# PREFIX used for system installation of the FloPoCo binaries/symlink:
PREFIX ?= /usr/local

# If ON (default), the script will take care of fetching and building SCIP
# locally, and integrate it with ScaLP.
WITH_SCIP ?= ON

# Adds NVC and its dependencies to the build process (OFF by default)
WITH_NVC ?= OFF

# Builds in RELEASE mode by default, otherwise, user can set it to 'DEBUG'
CONFIG ?= RELEASE

ifeq ($(CONFIG), DEBUG) # -------------------------
    CMAKE_BUILD_TYPE := -DCMAKE_BUILD_TYPE=Debug
else
    CMAKE_BUILD_TYPE := -DCMAKE_BUILD_TYPE=Release
endif # -------------------------------------------

$(call static_info, $(B)CONFIG$(N): $(CONFIG))
$(call static_info, $(B)CMAKE_GENERATOR$(N): $(CMAKE_GENERATOR))
$(call static_info, $(B)INSTALL_TYPE$(N): $(INSTALL_TYPE))
$(call static_info, $(B)PREFIX$(N): $(PREFIX))
$(call static_info, $(B)NVC building$(N): $(WITH_NVC))

# -----------------------------------------------------------------------------
ifeq ($(call file_exists, $(GUROBI_ROOT_DIR)/bin/gurobi.sh), 1)
# Try to find Gurobi from either $GUROBI_HOME environment variable or
# a user-defined path. If Gurobi can't be found, only SCIP will be used
# as a backend.
# -----------------------------------------------------------------------------
    SCALP_BACKEND += GUROBI
    $(call static_ok, Found $(B)Gurobi$(N) in $(GUROBI_ROOT_DIR))
    ifeq ($(WITH_SCIP), ON)
        # Add SCIP anyway, unless explicitly set OFF by the user
        SCALP_BACKEND += SCIP
    endif
else
    # Otherwise, just use SCIP
    SCALP_BACKEND := SCIP
endif

$(call static_info, $(B)ScaLP backend(s)$(N): $(SCALP_BACKEND))

# -----------------------------------------------------------------------------xxxxxx

all: flopoco

# -----------------------------------------------------------------------------
.PHONY: help
# -----------------------------------------------------------------------------
help:
	$(call shell_info, TODO!)

# -----------------------------------------------------------------------------
.PHONY: clean
# -----------------------------------------------------------------------------
clean:
	@rm -rf build

# -----------------------------------------------------------------------------
.PHONY: docker
# Build docker images with the minimum required to run flopoco
# flopoco binary will be installed system-wide.
# -----------------------------------------------------------------------------

DOCKER_IMAGE ?= debian
DOCKER_IMAGE_SUPPORTED := ubuntu debian archlinux alpine

# Check if the selected image is implemented in this script:
ifeq (docker, $(filter docker, $(MAKECMDGOALS)))
    ifneq ($(DOCKER_IMAGE), $(filter $(DOCKER_IMAGE), $(DOCKER_IMAGE_SUPPORTED)))
        $(call static_error, Unsupported $(B)DOCKER_IMAGE$(N) $(DOCKER_IMAGE))
        $(call static_error, Supported images: $(DOCKER_IMAGE_SUPPORTED))
        $(error Aborting...)
    else
        $(call static_ok, $(B)DOCKER_IMAGE$(N): $(DOCKER_IMAGE))
    endif
endif

DOCKERFILE     := $(MKROOT)/Dockerfile
DOCKER_BRANCH  ?= dev/master
DOCKER_ARGS    += --no-cache
FLOPOCO_DOCKER_TAG := flopoco:$(FLOPOCO_VERSION_FULL)-$(DOCKER_IMAGE)

ifdef DOCKER_PROGRESS_PLAIN # --------------------------
    # Display the full log of the image building process
    DOCKER_ARGS += --progress=plain
endif # ------------------------------------------------

# -------------------------------------------------------------------------------
ifeq ($(DOCKER_IMAGE), debian)
# -------------------------------------------------------------------------------
define docker_script
    FROM debian:latest
    RUN apt update
    RUN yes | apt install git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco \
     && git checkout $(DOCKER_BRANCH) \
     && make sysdeps flopoco \
     && make install
endef
# -------------------------------------------------------------------------------
else ifeq ($(DOCKER_IMAGE), ubuntu)
# -------------------------------------------------------------------------------
define docker_script
    FROM ubuntu:latest
    ENV DEBIAN_FRONTEND=noninteractive
    RUN apt update
    RUN yes | apt install git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco \
     && git checkout $(DOCKER_BRANCH) \
     && make sysdeps flopoco DEBIAN_FRONTEND=noninteractive \
     && make install
endef
# -------------------------------------------------------------------------------
else ifeq ($(DOCKER_IMAGE), archlinux)
# -------------------------------------------------------------------------------
define docker_script
    FROM archlinux:latest
    RUN pacman -Syu --noconfirm
    RUN pacman --noconfirm -S git make sudo
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco \
     && git checkout $(DOCKER_BRANCH) \
     && make sysdeps flopoco \
     && make install
endef
# -------------------------------------------------------------------------------
else ifeq ($(DOCKER_IMAGE), alpine)
# -------------------------------------------------------------------------------
define docker_script
    FROM alpine:latest
    RUN apk update
    RUN apk add git make
    RUN echo '' >> /etc/apk/repositories
    RUN git clone https://gitlab.com/flopoco/flopoco
    RUN cd flopoco \
     && git checkout $(DOCKER_BRANCH) \
     && make sysdeps flopoco \
     && make install
endef
endif

# First, generate the dockerfile, by calling the distro-dependent
# 'docker_script' macro
$(DOCKERFILE):
	$(call shell_info, Generating $(B)Dockerfile$(N))
	@echo '$(call docker_script)' > $(DOCKERFILE)

# Build from the generated Dockerfile,
# remove Dockerfile afterwards
docker: $(DOCKERFILE)
	$(call shell_info, Building docker image $(B)$(FLOPOCO_DOCKER_TAG)$(N))
	@docker build $(DOCKER_ARGS) -t $(FLOPOCO_DOCKER_TAG) $(MKROOT)
	@rm -rf $(DOCKERFILE)

# -----------------------------------------------------------
.PHONY: sysdeps
# OS-dependent 'package-manager' dependency lists
# This is used for building docker images, and for the CI
# but it can be also directly called by the user, if the
# OS is supported: $ make sysdeps flopoco
# -----------------------------------------------------------
ifeq ($(OS_ID), $(filter $(OS_ID), ubuntu debian))
# -----------------------------------------------------------
    SYSDEPS += g++
    SYSDEPS += cmake
    SYSDEPS += ninja-build
    SYSDEPS += libgmp-dev
    SYSDEPS += libmpfi-dev
    SYSDEPS += libmpfr-dev
    SYSDEPS += liblapack-dev liblapacke-dev
    SYSDEPS += libsollya-dev
    SYSDEPS += dh-autoreconf
    SYSDEPS += flex
    SYSDEPS += libboost-all-dev
    SYSDEPS += pkg-config
    SYSDEPS += libtbb-dev

    define sysdeps_cmd
        $(SUDO) apt update
        $(SUDO) apt install -y $(SYSDEPS)
    endef

    NVC_DEPENDENCIES += build-essential
    NVC_DEPENDENCIES += automake
    NVC_DEPENDENCIES += autoconf
    NVC_DEPENDENCIES += flex
    NVC_DEPENDENCIES += llvm-dev
    NVC_DEPENDENCIES += pkg-config
    NVC_DEPENDENCIES += zlib1g-dev
    NVC_DEPENDENCIES += libdw-dev
    NVC_DEPENDENCIES += libffi-dev
    NVC_DEPENDENCIES += libzstd-dev

    define nvcdeps_cmd
        $(SUDO) apt update
        yes | $(SUDO) apt install $(NVC_DEPENDENCIES)
    endef

# -----------------------------------------------------------
else ifeq ($(OS_ID), arch)
# Using AUR with docker:
# docker pull greyltc/archlinux-aur:yay
# -----------------------------------------------------------
    SYSDEPS += gcc      # pacman
    SYSDEPS += cmake    # pacman
    SYSDEPS += ninja    # pacman
    SYSDEPS += gmp      # pacman
    SYSDEPS += mpfr     # pacman
    SYSDEPS += mpfi     # pacman
    SYSDEPS += flex     # pacman
    SYSDEPS += boost    # pacman
    SYSDEPS += pkgconf  # pacman
    SYSDEPS += autoconf # pacman
    SYSDEPS += libtool  # pacman
    SYSDEPS += lapack   # pacman
    SYSDEPS += lapacke  # pacman
    SYSDEPS += fplll    # pacman
    SYSDEPS += libxml2  # pacman
#    SYSDEPS += sollya-git # AUR, broken (?)
    define sysdeps_cmd
        yay -Syu
        yay -S $(SYSDEPS)
    endef
# -----------------------------------------------------------
else ifeq ($(OS_ID), alpine)
# -----------------------------------------------------------
    SYSDEPS += gcc
    SYSDEPS += g++
    SYSDEPS += grep
    SYSDEPS += patch
    SYSDEPS += libtbb-dev # for papilo/scip
    SYSDEPS += cmake
    SYSDEPS += automake
    SYSDEPS += autoconf
    SYSDEPS += bash
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
    SYSDEPS += libmpfi
    SYSDEPS += libmpfi-dev # edge/testing
    SYSDEPS += fplll-dev # not recognized correctly (TODO)
# -----------------------------------------------------------
else ifeq ($(OS_ID), macos)
    # macOS: homebrew (default) or macports
    MACOS_PKG_MANAGER ?= brew
# -----------------------------------------------------------
    # Common to both package managers (brew & port):
    SYSDEPS += wget         # brew: ok | macports: ok
    SYSDEPS += cmake        # brew: ok | macports: ok
    SYSDEPS += gmp          # brew: ok | macports: ok
    SYSDEPS += mpfr         # brew: ok | macports: ok
    SYSDEPS += mpfi         # brew: ok | macports: ok
    SYSDEPS += boost        # brew: ok | macports: ok
    SYSDEPS += autoconf     # brew: ok | macports: ok
    SYSDEPS += automake     # brew: ok | macports: ok
    SYSDEPS += libtool      # brew: ok | macports: ok
    SYSDEPS += lapack       # brew: ok | macports: ok
    # ---------------------------------------------------------
    ifeq ($(MACOS_PKG_MANAGER), brew)
    # ---------------------------------------------------------
        SYSDEPS += make         # brew: ok | macports: gmake
        SYSDEPS += pkg-config   # brew: ok | macports: pkgconfig
        SYSDEPS += sollya       # brew: ok | macports: nope /!/
        SYSDEPS += ninja        # brew: ok | macports: nope /!/

        define sysdeps_cmd
            brew update && brew upgrade
            brew install $(SYSDEPS)
        endef
    # ---------------------------------------------------------
    else ifeq ($(MACOS_PKG_MANAGER), port)
    # ---------------------------------------------------------
        SYSDEPS += gmake
        SYSDEPS += pkgconfig

        define sysdeps_cmd
            sudo port selfupdate
            sudo port install $(SYSDEPS)
        endef
    endif # MACOS_PKG_MANAGER
    # -----------------------------------------------------------
    else # Unknown distro, do nothing
    # -----------------------------------------------------------
    define sysdeps_cmd
        $(call shell_info, Linux distribution could not be identified, skipping...)
    endef
endif # OS_ID

sysdeps:
	$(call shell_info, Updating $(OS_ID) system $(B)dependencies$(N): $(SYSDEPS))
	$(call sysdeps_cmd)

# Note: for building sollya: autoreconf -fi ./configure and make

# -----------------------------------------------------------------------------
.PHONY: scip
# Solver for mixed integer programming (MIP) and mixed integer
# nonlinear programming (MINLP)
# -----------------------------------------------------------------------------
SCIP_GIT := https://github.com/scipopt/scip.git
SCIP_VERSION := v910
SCIP_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/scip
SCIP_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/scip
SCIP_LIBRARIES := $(SCIP_BINARY_DIR)/lib/libscip.$(dylib)

scip: $(SCIP_LIBRARIES)

.ONESHELL:
$(SCIP_LIBRARIES):
	$(call shell_info, Fetching and building $(B)SCIP$(N) library)
	@mkdir -p $(SCIP_BINARY_DIR)
	@git clone $(SCIP_GIT) $(SCIP_SOURCE_DIR)	
	@cd $(SCIP_SOURCE_DIR)
	@git checkout $(SCIP_VERSION)
	@cmake -B build -G$(CMAKE_GENERATOR)		    \
	       -DAUTOBUILD=ON				    \
	       -DCMAKE_INSTALL_PREFIX=$(SCIP_BINARY_DIR)    \
	       -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE	    \
	       $(CMAKE_BUILD_TYPE)
	@cmake --build build --target install -j 8

install-scip: $(SCIP_LIBRARIES)
	$(call shell_info, Installing $(B)SCIP$(N) libraries ($(SCIP_LIBRARIES)) in $(PREFIX))
	@cp $(SCIP_LIBRARIES) $(PREFIX)/lib

# -----------------------------------------------------------------------------
.PHONY: gurobi
# -----------------------------------------------------------------------------
# Gurobi (TODO):
# - Linux(x86): https://packages.gurobi.com/11.0/gurobi11.0.2_linux64.tar.gz
# - Linux(arm): https://packages.gurobi.com/11.0/gurobi11.0.2_armlinux64.tar.gz
# - macOS: https://packages.gurobi.com/11.0/gurobi11.0.2_macos_universal2.pkg
# - The binaries can be downloaded without a license, but it is needed when
# the libraries and/or the executable are called

# -----------------------------------------------------------------------------
.PHONY: scalp
# Wrapper for different ILP-solvers, such as Gurobi, CPLEX, SCIP and LPSOLVE
# -----------------------------------------------------------------------------
SCALP_GIT := https://digidev.digi.e-technik.uni-kassel.de/git/scalp.git
SCALP_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/scalp
SCALP_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/scalp

SCALP_LIBRARIES += $(SCALP_BINARY_DIR)/lib/libScaLP.$(dylib)
scalp: $(SCALP_LIBRARIES)

SCALP_CMAKE_OPTIONS += -DUSE_LPSOLVE=OFF
SCALP_CMAKE_OPTIONS += $(CMAKE_BUILD_TYPE)
SCALP_CMAKE_OPTIONS += -DSOPLEX_DYN_LINK=OFF

# -----------------------------------------------
ifeq (GUROBI, $(filter GUROBI, $(SCALP_BACKEND)))
# -----------------------------------------------
    SCALP_DEPENDENCIES += $(GUROBI)
    SCALP_CMAKE_OPTIONS += -DGUROBI_ROOT_DIR=$(GUROBI_ROOT_DIR)
    SCALP_LIBRARIES += $(SCALP_BINARY_DIR)/lib/libScaLP-Gurobi.$(dylib)
endif
# -------------------------------------------
ifeq (SCIP, $(filter SCIP, $(SCALP_BACKEND)))
# -------------------------------------------
    SCALP_DEPENDENCIES += $(SCIP_LIBRARIES)
    SCALP_CMAKE_OPTIONS += -DSCIP_ROOT_DIR=$(SCIP_BINARY_DIR)
    SCALP_LIBRARIES += $(SCALP_BINARY_DIR)/lib/libScaLP-SCIP.$(dylib)
endif

ifeq ($(OS_ID), alpine)
    SCALP_CMAKE_OPTIONS += -DCMAKE_CXX_FLAGS=-U_FORTIFY_SOURCE
endif

.ONESHELL:
$(SCALP_LIBRARIES) &: $(SCALP_DEPENDENCIES)
	$(call shell_info, Fetching and building $(B)ScaLP$(N) library)
	@mkdir -p $(SCALP_BINARY_DIR)
	@git clone $(SCALP_GIT) $(SCALP_SOURCE_DIR)
	@cd $(SCALP_SOURCE_DIR)
	@cmake -B build -G$(CMAKE_GENERATOR)		    \
	       -DCMAKE_INSTALL_PREFIX=$(SCALP_BINARY_DIR)   \
	       $(SCALP_CMAKE_OPTIONS)
	@cmake --build build --target install

install-scalp: $(SCALP_LIBRARIES)
	$(call shell_info, Installing $(B)SCALP$(N) libraries ($(SCALP_LIBRARIES)) in $(PREFIX))
	@cp $(SCALP_LIBRARIES) $(PREFIX)/lib

# -----------------------------------------------------------------------------
.PHONY: wcpg
# Functions for reliable evaluation of the Worst-Case Peak Gain matrix
# of a discrete-time LTI filter.
# -----------------------------------------------------------------------------
WCPG_GIT := https://github.com/fixif/WCPG
WCPG_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/wcpg
WCPG_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/wcpg

WCPG_LIBRARIES += $(WCPG_BINARY_DIR)/lib/libwcpg.$(dylib).0.0.9
WCPG_LIBRARIES += $(WCPG_BINARY_DIR)/lib/libwcpg.$(dylib).0
WCPG_LIBRARIES += $(WCPG_BINARY_DIR)/lib/libwcpg.$(dylib)

ifeq ($(OS_ID), macos) # ------------------------------------
    # On macOS, for some reason, lapack homebrew installation
    # can't be found, we have to explicitely set these
    # additional flags:
    WCPG_MACOS_FLAGS += -I/usr/local/opt/lapack/include
    WCPG_MACOS_FLAGS += -L/usr/local/opt/lapack/lib
    WCPG_CONFIGURE_FLAGS += --with-lapack=/usr/local/opt/lapack
    WCPG_CONFIGURE_FLAGS += CFLAGS="$(WCPG_MACOS_FLAGS)"
endif # -----------------------------------------------------

wcpg: $(WCPG_LIBRARIES)

.ONESHELL:
$(WCPG_LIBRARIES) &:
	$(call shell_info, Fetching and building $(B)WCPG$(N) library)
	@mkdir -p $(WCPG_BINARY_DIR)
	@git clone $(WCPG_GIT) $(WCPG_SOURCE_DIR)
	@cd $(WCPG_SOURCE_DIR)
	@bash autogen.sh
	@./configure --prefix=$(WCPG_BINARY_DIR) $(WCPG_CONFIGURE_FLAGS)
	@make -j8 install

install-wcpg: $(WCPG_LIBRARIES)
	$(call shell_info, Installing $(B)WCPG$(N) libraries ($(WCPG_LIBRARIES)) in $(PREFIX)/lib)
	@cp $(WCPG_LIBRARIES) $(PREFIX)/lib

# -----------------------------------------------------------------------------
.PHONY: pagsuite
# Optimization tools for the (pipelined) multiple constant multiplication
# ((P)MCM) problem, i.e., the multiplication of a single variable with
# multiple constants using bit-shifts, registered adders/subtractors
# and registers.
# -----------------------------------------------------------------------------
PAGSUITE_GIT := https://gitlab.com/kumm/pagsuite.git
PAGSUITE_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/pagsuite
PAGSUITE_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/pagsuite
PAGSUITE_LIBRARIES += $(PAGSUITE_BINARY_DIR)/lib/libpag.$(dylib)
PAGSUITE_LIBRARIES += $(PAGSUITE_BINARY_DIR)/lib/libpag.$(dylib).0
PAGSUITE_LIBRARIES += $(PAGSUITE_BINARY_DIR)/lib/libpag.$(dylib).2.1.0
PAGSUITE_LIBRARIES += $(PAGSUITE_BINARY_DIR)/lib/librpag.$(dylib)
PAGSUITE_LIBRARIES += $(PAGSUITE_BINARY_DIR)/lib/liboscm.$(dylib)

pagsuite: $(PAGSUITE_LIBRARIES)

.ONESHELL:
$(PAGSUITE_LIBRARIES) &: $(SCALP_LIBRARIES)
	$(call shell_info, Fetching and building $(B)PAGSuite$(N) library)
	@mkdir -p $(PAGSUITE_BINARY_DIR)
	@git clone $(PAGSUITE_GIT) $(PAGSUITE_SOURCE_DIR)
	@cd $(PAGSUITE_SOURCE_DIR)
	@cmake -B build -G$(CMAKE_GENERATOR)			\
	       -DSCALP_PREFIX_PATH=$(SCALP_BINARY_DIR)		\
	       -DCMAKE_INSTALL_PREFIX=$(PAGSUITE_BINARY_DIR)	\
	       $(CMAKE_BUILD_TYPE)
	@cmake --build build --target install

install-pagsuite: $(PAGSUITE_LIBRARIES)
	$(call shell_info, Installing $(B)PAGSuite$(N) libraries ($(PAGSUITE_LIBRARIES)) in $(PREFIX))
	@cp $(PAGSUITE_LIBRARIES) $(PREFIX)/lib

# -----------------------------------------------------------------------------
.PHONY: nvc
# VHDL compiler and simulator.
# -----------------------------------------------------------------------------
NVC := /usr/local/bin/nvc
NVC_GIT := https://github.com/nickg/nvc.git
NVC_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/nvc

nvc: $(NVC)

.ONESHELL:
$(NVC):
	$(call shell_info, Fetching and building $(B)NVC$(N) dependency)
	$(call nvcdeps_cmd)
	@git clone $(NVC_GIT) $(NVC_SOURCE_DIR)
	@cd $(NVC_SOURCE_DIR)
	@./autogen.sh
	@mkdir build && cd build
	@../configure
	@make
	@sudo make install

# -----------------------------------------------------------------------------
.PHONY: dependencies
# -----------------------------------------------------------------------------

# PAGSuite needs scalp anyway.
#FLOPOCO_DEPENDENCIES += scalp
FLOPOCO_DEPENDENCIES += $(WCPG_LIBRARIES)
FLOPOCO_DEPENDENCIES += $(PAGSUITE_LIBRARIES)
FLOPOCO_DEPENDENCIES += $(shell find code/ -type f -name '*')

ifeq ($(WITH_NVC), ON)
    FLOPOCO_DEPENDENCIES += $(NVC)
endif

dependencies: $(FLOPOCO_DEPENDENCIES)

# -----------------------------------------------------------------------------
.PHONY: flopoco
# -----------------------------------------------------------------------------

FLOPOCO := $(MKROOT)/build/bin/flopoco
flopoco: $(FLOPOCO)

$(FLOPOCO): $(FLOPOCO_DEPENDENCIES)
	$(call shell_info, Now building $(B)FloPoCo$(N))
	@cmake -B build -G$(CMAKE_GENERATOR)	    \
	       -DWCPG_LOCAL=$(WCPG_BINARY_DIR)	    \
	       -DSCALP_LOCAL=$(SCALP_BINARY_DIR)    \
	       -DPAG_LOCAL=$(PAGSUITE_BINARY_DIR)   \
	       -DCMAKE_INSTALL_PREFIX=$(PREFIX)	    \
	       $(CMAKE_BUILD_TYPE)
	@cmake --build build -j 8
	$(call shell_info, Adding 'flopoco' $(B)symlink$(N)' in repository's root directory)
	@ln -fs $(FLOPOCO) $(MKROOT)/build
	$(call shell_info, Building the $(B)HTML documentation$(N) in doc/web)
	$(FLOPOCO) BuildHTMLDoc
	$(call shell_info, Now running $(B)FloPoCo$(N))
	$(FLOPOCO)
	$(call shell_info, Generating and installing $(B)bash autocompletion$(N) file)
	$(FLOPOCO) BuildAutocomplete
	$(call shell_ok, If you saw the command-line help of FloPoCo - Welcome!)

# -----------------------------------------------------------------------------
.ONESHELL:
.PHONY: install
# -----------------------------------------------------------------------------
ifeq ($(INSTALL_TYPE), symlink) # ---------------------------------------------
install: $(FLOPOCO)
	$(call shell_info, Installing $(B)flopoco symlink$(N) in $(PREFIX))
	@ln -fs $(FLOPOCO) $(PREFIX)/bin
else # ------------------------------------------------------------------------
install: $(FLOPOCO) install-pagsuite	\
		    install-wcpg	\
		    install-scalp	\
		    install-scip
	@cd $(MKROOT)
	@cmake --build build --target install
endif

# -----------------------------------------------------------------------------
.ONESHELL:
.PHONY: autotests
# -----------------------------------------------------------------------------
autotests: $(FLOPOCO)
	@$(FLOPOCO) autotest operator=all testlevel=0 output=autotests
