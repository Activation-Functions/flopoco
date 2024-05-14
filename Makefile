

MKROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DEPENDENCIES_DIR := $(MKROOT)/build/dependencies
BUILD_DEPENDENCIES_SOURCE_DIR := $(BUILD_DEPENDENCIES_DIR)/src
BUILD_DEPENDENCIES_BINARY_DIR := $(BUILD_DEPENDENCIES_DIR)/bin

CMAKE_GENERATOR ?= Ninja
SCALP_BACKEND ?= LPSOLVE

include $(MKROOT)/tools/utilities.mk

$(call static_info, OS: $(B)$(OS)$(N))
$(call static_info, Using ScaLP backend: $(B)$(SCALP_BACKEND)$(N))

# -----------------------------------------------------------------------------

all: flopoco

# -----------------------------------------------------------------------------
.PHONY: clean
# -----------------------------------------------------------------------------
clean:
	@rm -rf build

# -----------------------------------------------------------------------------
.PHONY: sysdeps
# -----------------------------------------------------------------------------

# ------------------------------------------
ifeq ($(OS), $(filter $(OS), UBUNTU DEBIAN))
# ------------------------------------------
    SYSDEPS += git
    SYSDEPS += g++
    SYSDEPS += cmake
    SYSDEPS += ninja-build
    SYSDEPS += libgmp-dev
    SYSDEPS += libmpfi-dev
    SYSDEPS += libmpfr-dev
    SYSDEPS += libsollya-dev
    SYSDEPS += liblpsolve55-dev
    SYSDEPS += dh-autoreconf
    SYSDEPS += libf2c2-dev
    SYSDEPS += flex
    SYSDEPS += libboost-all-dev
    SYSDEPS += pkg-config
sysdeps:
	$(call shell_info, Updating $(OS) system $(B)dependencies$(N): $(SYSDEPS))
	@sudo apt update
	@sudo apt install $(SYSDEPS)
# -------------------------------------
else ifeq ($(OS), ARCHLINUX)
# -------------------------------------
# Using AUR with docker::
# docker pull greyltc/archlinux-aur:yay
    SYSDEPS += git
    SYSDEPS += gcc
    SYSDEPS += cmake
    SYSDEPS += ninja
    SYSDEPS += gmp
    SYSDEPS += mpfr
    SYSDEPS += mpfi
    SYSDEPS += flex
    SYSDEPS += lpsolve
    SYSDEPS += boost
    SYSDEPS += pkgconf
    SYSDEPS += sollya-git
    SYSDEPS += f2c
sysdeps:
	$(call shell_info, Updating $(OS) system $(B)dependencies$(N): $(SYSDEPS))
	@yay -Syu
	@yay -S $(SYSDEPS)

# -------------------------------------
else ifeq ($(OS), ALPINE)
# -------------------------------------
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
endif

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
.PHONY: scalp
# -----------------------------------------------------------------------------
SCALP_GIT := https://digidev.digi.e-technik.uni-kassel.de/git/scalp.git
SCALP_SOURCE_DIR := $(BUILD_DEPENDENCIES_SOURCE_DIR)/scalp
SCALP_BINARY_DIR := $(BUILD_DEPENDENCIES_BINARY_DIR)/scalp
SCALP_CMAKE_PATCH := $(MKROOT)/tools/scalp_fpc.patch
SCALP_DEPENDENCIES += $(SCALP_CMAKE_PATCH)
SCALP := $(SCALP_BINARY_DIR)/lib/libScaLP.so

# ------------------------------------------------------------------
ifeq ($(SCALP_BACKEND), SCIP)
# ------------------------------------------------------------------
    SCALP_DEPENDENCIES += $(SCIP)
    SCALP_CMAKE_OPTIONS += -DUSE_LPSOLVE=OFF
    SCALP_CMAKE_OPTIONS += -DSCIP_DIR=$(SCIP_BINARY_DIR)
    SCALP_CMAKE_OPTIONS += -DSOPLEX_DIR=$(SOPLEX_BINARY_DIR)
else
    LPSOLVE_LIBRARIES := /usr/lib/lp_solve/liblpsolve55.so
    SCALP_DEPENDENCIES += sysdeps
    SCALP_CMAKE_OPTIONS += -DUSE_LPSOLVE=ON
    SCALP_CMAKE_OPTIONS += -DLPSOLVE_ROOT_DIR=/usr/include/lpsolve
    SCALP_CMAKE_OPTIONS += -DLPSOLVE_LIBRARIES=$(LPSOLVE_LIBRARIES)
    SCALP_CMAKE_OPTIONS += -DLPSOLVE_INCLUDE_DIRS=$(LPSOLVE_INCLUDE_DIRS)
    SCALP_CMAKE_OPTIONS += -DBUILD_SHARED_LIBRARIES=OFF
endif

# TODO:
#SCALP_DEPENDENCIES += GUROBI
#SCALP_DEPENDENCIES += CPLEX

scalp: $(SCALP)

.ONESHELL:
$(SCALP): $(SCALP_DEPENDENCIES)
	$(call shell_info, Fetching and building $(B)ScaLP$(N) library)
	@mkdir -p $(SCALP_BINARY_DIR)
	@git clone $(SCALP_GIT) $(SCALP_SOURCE_DIR)
	@cd $(SCALP_SOURCE_DIR)
	@patch -p0 CMakeLists.txt $(SCALP_CMAKE_PATCH)
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

wcpg: $(WCPG)

.ONESHELL:
$(WCPG):
	$(call shell_info, Fetching and building $(B)WCPG$(N) library)
	@mkdir -p $(WCPG_BINARY_DIR)
	@git clone $(WCPG_GIT) $(WCPG_SOURCE_DIR)
	@cd $(WCPG_SOURCE_DIR)
	@git checkout $(WCPG_COMMIT_HASH)
	@bash autogen.sh
	@./configure --prefix=$(WCPG_BINARY_DIR)
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
	@patch -p0 CMakeLists.txt $(PAGSUITE_CMAKE_PATCH)
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
	       -DPAG_LOCAL=$(PAGSUITE_BINARY_DIR)
	@cmake --build build

# -----------------------------------------------------------------------------
.PHONY: install
# -----------------------------------------------------------------------------
install: $(FLOPOCO)
	@cmake --build build --target install

