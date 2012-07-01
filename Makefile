#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
# Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
#
# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Look at Makefile.config for configuration options.
#
# Tested on unix platforms (building for GTK and cross-compiling for RO) and
# on RO (building for RO).
#
# To clean, invoke as above, with the 'clean' target
#
# To build developer Doxygen generated documentation, invoke as above,
# with the 'docs' target:
#   make docs
#

all: all-program

# Determine host type
# NOTE: HOST determination on RISC OS could fail because of missing bug fixes
#	in UnixLib which only got addressed in UnixLib 5 / GCCSDK 4.
#	When you don't have 'uname' available, you will see:
#	  File 'uname' not found
#	When you do and using a 'uname' compiled with a buggy UnixLib, you
#	will see the following printed on screen:
#	  RISC OS
#	In both cases HOST make variable is empty and we recover from that by
#	assuming we're building on RISC OS.
#	In case you don't see anything printed (including the warning), you
#	have an up-to-date RISC OS build system. ;-)
HOST := $(shell uname -s)

# Sanitise host
# TODO: Ideally, we want the equivalent of s/[^A-Za-z0-9]/_/g here
HOST := $(subst .,_,$(subst -,_,$(subst /,_,$(HOST))))

ifeq ($(HOST),)
  HOST := riscos
  $(warning Build platform determination failed but that's a known problem for RISC OS so we're assuming a native RISC OS build.)
else
  ifeq ($(HOST),RISC OS)
    # Fixup uname -s returning "RISC OS"
    HOST := riscos
  endif
endif

ifeq ($(HOST),riscos)
  # Build happening on RO platform, default target is RO backend
  ifeq ($(TARGET),)
    TARGET := riscos
  endif
else
  ifeq ($(HOST),BeOS)
    HOST := beos
  endif
  ifeq ($(HOST),Haiku)
    # Haiku implements the BeOS API
    HOST := beos
  endif
  ifeq ($(HOST),beos)
    # Build happening on BeOS platform, default target is BeOS backend
    ifeq ($(TARGET),)
      TARGET := beos
    endif
    # BeOS still uses gcc2
    GCCVER := 2
  else
    ifeq ($(HOST),AmigaOS)
      HOST := amiga
      ifeq ($(TARGET),)
        TARGET := amiga
      endif
    else
      ifeq ($(HOST),Darwin)
        HOST := macosx
        ifeq ($(TARGET),)
          TARGET := cocoa
        endif
      endif  
      ifeq ($(HOST),FreeMiNT)
        HOST := mint
      endif
      ifeq ($(HOST),mint)
        ifeq ($(TARGET),)
          TARGET := atari
        endif
      endif
      ifeq ($(findstring MINGW,$(HOST)),MINGW)
        # MSYS' uname reports the likes of "MINGW32_NT-6.0"
        HOST := windows
      endif
      ifeq ($(HOST),windows)
        ifeq ($(TARGET),)
          TARGET := windows
        endif
      endif

      # Default target is GTK backend
      ifeq ($(TARGET),)
        TARGET := gtk
      endif
    endif
  endif
endif
SUBTARGET =
RESOURCES =

ifneq ($(TARGET),riscos)
  ifneq ($(TARGET),gtk)
    ifneq ($(TARGET),beos)
      ifneq ($(findstring amiga,$(TARGET)),amiga)
        ifneq ($(TARGET),framebuffer)
          ifneq ($(TARGET),windows)
            ifneq ($(TARGET),atari)
              ifneq ($(TARGET),cocoa)
                ifneq ($(TARGET),monkey)
                  $(error Unknown TARGET "$(TARGET)", should either be "riscos", "gtk", "beos", "amiga", "framebuffer", "windows", "atari" or "cocoa" or "monkey")
                endif
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch
STRIP=strip

# Override this only if the host compiler is called something different
HOST_CC := gcc

ifeq ($(TARGET),riscos)
  ifeq ($(HOST),riscos)
    # Build for RO on RO
    GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
    CCRES := ccres
    TPLEXT :=
    MAKERUN := makerun
    RUNEXT :=
    CC := gcc
    EXEEXT :=
    PKG_CONFIG :=
  else
    # Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
    # either using GCCSDK 4 - ELF)
    ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/env),)
        GCCSDK_INSTALL_ENV := /opt/netsurf/arm-unknown-riscos/env
      else
        GCCSDK_INSTALL_ENV := /home/riscos/env
      endif
    endif

    ifeq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/cross/bin),)
        GCCSDK_INSTALL_CROSSBIN := /opt/netsurf/arm-unknown-riscos/cross/bin
      else
        GCCSDK_INSTALL_CROSSBIN := /home/riscos/cross/bin
      endif
    endif

    CCRES := $(GCCSDK_INSTALL_CROSSBIN)/ccres
    TPLEXT := ,fec
    MAKERUN := $(GCCSDK_INSTALL_CROSSBIN)/makerun
    RUNEXT := ,feb
    CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
    ifneq (,$(findstring arm-unknown-riscos-gcc,$(CC)))
      SUBTARGET := -elf
      EXEEXT := ,e1f
      ELF2AIF := $(GCCSDK_INSTALL_CROSSBIN)/elf2aif
    else
     SUBTARGET := -aof
     EXEEXT := ,ff8
    endif
    PKG_CONFIG := $(GCCSDK_INSTALL_ENV)/ro-pkg-config
    CCACHE := $(shell which ccache)
    ifneq ($(CCACHE),)
      CC := $(CCACHE) $(CC)
    endif
  endif
else
  ifeq ($(TARGET),beos)
    # Building for BeOS/Haiku
    #ifeq ($(HOST),beos)
      # Build for BeOS on BeOS
      GCCSDK_INSTALL_ENV := /boot/develop
      CC := gcc
      CXX := g++
      EXEEXT :=
      PKG_CONFIG :=
    #endif
  else
    ifeq ($(TARGET),windows)
      ifneq ($(HOST),windows)
        # Set Mingw defaults
	MINGW_PREFIX ?= i586-mingw32msvc-
	MINGW_INSTALL_ENV ?= /usr/i586-mingw32msvc/

        # mingw cross-compile
        CC := $(MINGW_PREFIX)gcc
        PKG_CONFIG := $(MINGW_INSTALL_ENV)/bin/pkg-config
      else
        # Building on Windows
        CC := gcc
        PKG_CONFIG :=
      endif
    else
      ifeq ($(findstring amiga,$(TARGET)),amiga)
        ifneq ($(findstring amiga,$(HOST)),amiga)
          ifeq ($(TARGET),amigaos3)
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-unknown-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-unknown-amigaos/cross/bin

            SUBTARGET = os3
          else
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/ppc-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/ppc-amigaos/cross/bin
          endif

          override TARGET := amiga

          CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)

          PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
        endif
      else
        ifeq ($(TARGET),cocoa)
          PKG_CONFIG := PKG_CONFIG_PATH="$(PKG_CONFIG_PATH):/usr/local/lib/pkgconfig" pkg-config
        else
          # Building for GTK, Framebuffer, Atari
          PKG_CONFIG := pkg-config
        endif
      endif
    endif
  endif
endif

# Target paths

OBJROOT = build-$(HOST)-$(TARGET)$(SUBTARGET)
DEPROOT := $(OBJROOT)/deps
TOOLROOT := $(OBJROOT)/tools


# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Parameters to add to CFLAGS
# 3: Parameters to add to LDFLAGS
# 4: Human-readable name for the feature
define feature_enabled
  ifeq ($$(NETSURF_USE_$(1)),YES)
    CFLAGS += $(2)
    LDFLAGS += $(3)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	enabled       (NETSURF_USE_$(1) := YES))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(4)	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES or NO)
  endif
endef

# 1: Feature name (ie, NETSURF_USE_RSVG -> RSVG)
# 2: pkg-config required modules for feature
# 3: Human-readable name for the feature
define pkg_config_find_and_add
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  NETSURF_FEATURE_$(1)_AVAILABLE := $$(shell $$(PKG_CONFIG) --exists $(2) && echo yes)

  ifeq ($$(NETSURF_USE_$(1)),YES)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	enabled       (NETSURF_USE_$(1) := YES))
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	failed        (NETSURF_USE_$(1) := YES))
        $$(error Unable to find library for: $(3) ($(2)))
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),AUTO)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-enabled  (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := YES
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-disabled (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := NO
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
      $$(error NETSURF_USE_$(1) must be YES, NO, or AUTO)
    endif
  endif
endef

# ----------------------------------------------------------------------------
# General flag setup
# ----------------------------------------------------------------------------

# Set up the WARNFLAGS here so that they can be overridden in the Makefile.config
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs
ifneq ($(GCCVER),2)
  WARNFLAGS += -Wno-unused-parameter 
endif

# Pull in the configuration
include Makefile.defaults

$(eval $(call feature_enabled,JPEG,-DWITH_JPEG,-ljpeg,JPEG (libjpeg)))
$(eval $(call feature_enabled,MNG,-DWITH_MNG,-lmng,JNG/MNG/PNG (libmng)))

$(eval $(call feature_enabled,HARU_PDF,-DWITH_PDF_EXPORT,-lhpdf -lpng,PDF export (haru)))
$(eval $(call feature_enabled,LIBICONV_PLUG,-DLIBICONV_PLUG,,glibc internal iconv))

# common libraries without pkg-config support
LDFLAGS += -lz

CFLAGS += -DNETSURF_UA_FORMAT_STRING=\"$(NETSURF_UA_FORMAT_STRING)\"
CFLAGS += -DNETSURF_HOMEPAGE=\"$(NETSURF_HOMEPAGE)\"


# ----------------------------------------------------------------------------
# General make rules
# ----------------------------------------------------------------------------

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

$(TOOLROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(TOOLROOT)"
	$(Q)$(MKDIR) $(TOOLROOT)
	$(Q)$(TOUCH) $(TOOLROOT)/created

CLEANS := clean-target clean-testament

POSTEXES :=

# ----------------------------------------------------------------------------
# Target specific setup
# ----------------------------------------------------------------------------

include $(TARGET)/Makefile.target

# ----------------------------------------------------------------------------
# General source file setup
# ----------------------------------------------------------------------------

include Makefile.sources

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# Force exapnsion of source file list
SOURCES := $(SOURCES)

ifeq ($(SOURCES),)
$(error Unable to build NetSurf, could not determine set of sources to build)
endif

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.m,%.o,$(patsubst %.s,%.o,$(SOURCES))))))))

$(EXETARGET): $(OBJECTS) $(RESOURCES)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CC) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif
ifeq ($(TARGET),windows)
	$(Q)$(TOUCH) windows/res/preferences
endif
ifeq ($(TARGET),gtk)
	$(Q)$(TOUCH) gtk/res/toolbarIndices
endif
ifeq ($(NETSURF_STRIP_BINARY),YES)
	$(VQ)echo "   STRIP: $(EXETARGET)"
	$(Q)$(STRIP) $(EXETARGET)
endif
ifeq ($(TARGET),beos)
	$(VQ)echo "    XRES: $(EXETARGET)"
	$(Q)$(BEOS_XRES) -o $(EXETARGET) $(RSRC_BEOS)
	$(VQ)echo "  SETVER: $(EXETARGET)"
	$(Q)$(BEOS_SETVER) $(EXETARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "NetSurf $(VERSION_FULL)" \
                -long "NetSurf $(VERSION_FULL) © 2003 - 2008 The NetSurf Developers"
	$(VQ)echo " MIMESET: $(EXETARGET)"
	$(Q)$(BEOS_MIMESET) $(EXETARGET)
endif

ifeq ($(TARGET),beos)
$(RDEF_IMP_BEOS): $(RDEP_BEOS)
	$(VQ)echo "     GEN: $@"
	$(Q)n=5000; for f in $^; do echo "resource($$n,\"$${f#beos/res/}\") #'data' import \"$${f#beos/}\";"; n=$$(($$n+1)); done > $@

$(RSRC_BEOS): $(RDEF_BEOS) $(RDEF_IMP_BEOS)
	$(VQ)echo "      RC: $<"
	$(Q)$(BEOS_RC) -I beos -o $@ $^
endif

ifeq ($(TARGET),riscos)
  # Native RO build is different as 1) it can't do piping and 2) ccres on
  # RO does not understand Unix filespec
  ifeq ($(HOST),riscos)
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) -o processed_template $(2)
	$$(Q)$$(CCRES) processed_template $$(subst /,.,$$@)
	$$(Q)$(RM) processed_template
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  else
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) $(2) | $$(CCRES) - $$@
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  endif

clean-templates:
	$(VQ)echo "   CLEAN: $(CLEAN_TEMPLATES)"
	$(Q)$(RM) $(CLEAN_TEMPLATES)
CLEANS += clean-templates

$(eval $(foreach TPL,$(TPL_RISCOS), \
	$(call compile_template,$(notdir $(TPL)),$(TPL))))
endif

clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)

clean-testament:
	$(VQ)echo "   CLEAN: utils/testament.h"
	$(Q)$(RM) utils/testament.h

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir

all-program: $(EXETARGET) post-exe

.PHONY: testament
testament utils/testament.h:
	$(Q)if test -d .svn; then \
		$(PERL) utils/svn-testament.pl $(CURDIR) utils/testament.h; \
	else \
		$(PERL) utils/git-testament.pl $(CURDIR) utils/testament.h; \
	fi

post-exe: $(POSTEXES)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) Makefile.config

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
ifeq ($(GCCVER),2)
# simpler deps tracking for gcc2...
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef
else
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef
endif

define compile_target_cpp
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CXX) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "ASSEMBLE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(ASFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.d)),$(subst /,_,$(SOURCE:.cpp=.o)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.d)),$(subst /,_,$(SOURCE:.m=.o)))))

# Cannot currently generate dep files for S files because they're objasm
# when we move to gas format, we will be able to.

#$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
#	$(call dependency_generate_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.d)),$(subst /,_,$(SOURCE:.s=.o)))))

ifneq ($(MAKECMDGOALS),clean)
-include $(sort $(addprefix $(DEPROOT)/,$(DEPFILES)))
endif

# And rules to build the objects themselves...

$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call compile_target_cpp,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.o)),$(subst /,_,$(SOURCE:.cpp=.d)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.o)),$(subst /,_,$(SOURCE:.m=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

.PHONY: all clean docs install install-gtk

clean: $(CLEANS)

install-gtk: nsgtk
	mkdir -p $(DESTDIR)$(NETSURF_GTK_RESOURCES)throbber
	mkdir -p $(DESTDIR)$(NETSURF_GTK_RESOURCES)icons
	mkdir -p $(DESTDIR)$(NETSURF_GTK_BIN)
	@cp nsgtk $(DESTDIR)$(NETSURF_GTK_BIN)netsurf
	@cp -RL gtk/res/adblock.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/arrow_down_8x32.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/ca-bundle.txt $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/default.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/default.ico $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/favicon.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/gtkdefault.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/icons/*.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/internal.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/languages $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/license $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/netsurf.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/netsurf.xpm $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/netsurf-16x16.xpm $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/quirks.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/themelist $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/throbber/*.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)throbber
	@cp -RL gtk/res/toolbarIndices $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -RL gtk/res/SearchEngines $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@tar cf - --exclude .svn -C gtk/res themes | tar xf - -C $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@# Install translations
	@tar cf - --exclude .svn -C gtk/res C de en fr it nl | tar xf - -C $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@# Install glade templates
	@cp -v gtk/res/*.gtk*.ui $(DESTDIR)$(NETSURF_GTK_RESOURCES)

install-beos: NetSurf
#       TODO:HAIKU -- not sure if throbber is needed.  being left out for now.
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_BIN)
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
#	mkdir -p $(DESTDIR)$(NETSURF_BEOS_RESOURCES)throbber
	@copyattr -d NetSurf $(DESTDIR)$(NETSURF_BEOS_BIN)NetSurf
	@cp -vRL beos/res/adblock.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/ca-bundle.txt $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/default.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/beosdefault.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL gtk/res/license $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
#	@cp -vRL beos/res/throbber/*.png $(DESTDIR)$(NETSURF_BEOS_RESOURCES)throbber
	gzip -9v < beos/res/messages > $(DESTDIR)$(NETSURF_BEOS_RESOURCES)messages 


install-framebuffer: $(EXETARGET)
	mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_BIN)
	mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_RESOURCES)
	@cp -v $(EXETARGET) $(DESTDIR)/$(NETSURF_FRAMEBUFFER_BIN)netsurf$(SUBTARGET)
	@for F in default.css messages; do cp -vL framebuffer/res/$$F $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES); done

install: all-program install-$(TARGET)

docs:
	doxygen Docs/Doxyfile
