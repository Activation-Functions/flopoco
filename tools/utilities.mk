
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

$(call static_info, Including $(B)utilities.mk$(N) file)

# ---------------------------------------------------
ifeq ($(call file_exists, /etc/arch-release), 1)
# ---------------------------------------------------
    OS              := Arch
# ---------------------------------------------------
else ifeq ($(call file_exists, /etc/lsb-release), 1)
# ---------------------------------------------------
    OS              := Ubuntu
    OS_VERSION_FULL := $(shell lsb_release -d)
    OS_VERSION      := $(word 3, $(OS_VERSION_FULL))
    OS_LTS          := $(word 4, $(OS_VERSION_FULL))
# ---------------------------------------------------
else ifeq ($(call file_exists, /etc/debian_version), 1)
# ---------------------------------------------------
    OS              := Debian
    OS_VERSION_FULL := $(shell cat /etc/debian_version)
    OS_VERSION      := $(OS_VERSION_FULL)
    OS_LTS          := $(OS_VERSION)
    # ---------------------------------------------------
else ifeq ($(call file_exists, /usr/bin/sw_vers), 1)
# ---------------------------------------------------
    OS              := Darwin
    OS_VERSION_FULL := $(word 4, $(shell /usr/bin/sw_vers))
    OS_VERSION      := $(OS_VERSION_FULL)
    OS_LTS          := NA
# ---------------------------------------------------
else
# ---------------------------------------------------
    OS              := NA
    OS_VERSION_FULL := NA
    OS_VERSION      := NA
    OS_LTS          := NA
endif
