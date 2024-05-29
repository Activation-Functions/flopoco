
define file_exists # -----------------------
$(shell [ -f $(1) ] && echo 1 || echo 0)
endef # ------------------------------------

define dir_exists # ------------------------
$(shell [ -d $(1) ] && echo 1 || echo 0)
endef # ------------------------------------

B := $(shell tput bold)
N := $(shell tput sgr0)
O := $(shell tput setaf 11)
R := $(shell tput setaf 1)
G := $(shell tput setaf 2)

PRINT_OK    := [$(G)  OK  $(N)]
PRINT_INFO  := [$(O) INFO $(N)]
PRINT_ERROR := [$(R) ERR! $(N)]

define print_ok
$(PRINT_OK)$(1)
endef

define print_info
$(PRINT_INFO)$(1)
endef

define print_error
$(PRINT_ERROR)$(1)
endef

define static_ok
    $(info $(call print_ok,$(1)))
endef

define static_info # ----------------------------
    $(info $(call print_info,$(1)))
endef #------------------------------------------

define static_error #----------------------------
    $(info $(call print_error,$(1)))
endef #------------------------------------------

define shell_ok # -------------------------------
    @echo '$(call print_ok,$(1))'
endef # -----------------------------------------

define shell_info #------------------------------
    @echo '$(call print_info,$(1))'
endef # -----------------------------------------

define shell_error # ----------------------------
    @echo '$(call print_error,$(1))'
endef # -----------------------------------------

RELEASE_FILE_LINUX := /etc/os-release
RELEASE_FILE_MACOS := /usr/bin/sw_vers
QUOTE := "

define get_field
$(subst $(QUOTE),,$(shell cat $(RELEASE_FILE_LINUX) | grep -oP '(?<=^$(1)=).*'))
endef

# ---------------------------------------------------
ifeq ($(call file_exists, $(RELEASE_FILE_LINUX)), 1)
    OS_PRETTY_NAME := $(call get_field,PRETTY_NAME)
    OS_NAME := $(call get_field,NAME)
    OS_VERSION_ID := $(call get_field,VERSION_ID)
    OS_VERSION := $(call get_field,VERSION)
    OS_ID := $(call get_field,ID)
# ---------------------------------------------------
else ifeq ($(call file_exists, /usr/bin/sw_vers), 1)
# ---------------------------------------------------
    OS_ID           := macos
    OS_NAME         := macOS
    OS_VERSION_ID   := $(word 4, $(shell /usr/bin/sw_vers))
    OS_VERSION      := $(OS_VERSION_ID)
    OS_PRETTY_NAME  := $(OS_NAME) $(OS_VERSION)
endif

#$(call static_info, OS_PRETTY_NAME = $(OS_PRETTY_NAME))
#$(call static_info, OS_NAME = $(OS_NAME))
#$(call static_info, OS_VERSION_ID = $(OS_VERSION_ID))
#$(call static_info, OS_VERSION = $(OS_VERSION))
#$(call static_info, OS_ID = $(OS_ID))
