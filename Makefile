# vi: noexpandtab tabstop=4 softtabstop=4 list

# OS DETECTION
# ------------
ifdef ComSpec
	os = windows
else
	uname_s = $(shell uname -s)
	ifeq ($(uname_s),Darwin)
		os = macos
	endif
	ifeq ($(uname_s),Linux)
		os = linux
	endif
	ifeq ($(uname_s),FreeBSD)
		os = freebsd
	endif
endif

# PRIMARY TARGETS
# ---------------
all: library
.PHONY: library clean clean-library minimod all test

# CONFIG
# ------
OUTPUT_DIR := build

# macos only:
MIN_MACOS_VERSION = 10.11

# windows only:
# path to the LLVM installation
# ! do NOT add trailing \ 
# ! do NOT quote it
LLVM_PATH = C:\Program Files\LLVM

OPT = -O2
OPT += -fstrict-aliasing
QAJSON4C_VERSION = tags/1.1.1
DIRENT_VERSION = v1.23

USE_SANITIZER = 0
Q = @


ifeq ($(os),macos)
LIBRARY_NAME = libminimod.dylib
TEST_NAME = testsuite
TEST_PATH = $(OUTPUT_DIR)/$(TEST_NAME)
LIB_PATH = $(OUTPUT_DIR)/$(LIBRARY_NAME)
endif
ifeq ($(os),windows)
LIBRARY_NAME = minimod.dll
endif
ifeq ($(os),linux)
LIBRARY_NAME = libminimod.so
endif

ifneq ($(os),linux)
WARNINGS += -Weverything
NOWARNINGS += -Wno-error-unused-parameter
NOWARNINGS += -Wno-error-unused-function
NOWARNINGS += -Wno-error-unused-variable
else
WARNINGS += -Wall
endif

WARNINGS += -Werror

# MACROS/HELPERS
# --------------
ifeq ($(os),macos)
	DEVNULL = /dev/null
	ensure_dir=mkdir -p $(@D) 2> $(DEVNULL) || exit 0
	RM = rm
	TOUCH = touch $(1)
	CONDITIONAL_CLONE=if [ ! -d $(2) ]; then git clone -q $(1) $(2) ; fi
endif
ifeq ($(os),windows)
	DEVNULL = NUL
	CC = "$(LLVM_PATH)\bin\clang.exe"
	LINKER = "$(LLVM_PATH)\bin\lld-link.exe"
	ensure_dir=mkdir $(subst /,\,$(@D)) 2> $(DEVNULL) || exit 0
	RM = del
	TOUCH = copy /b $(subst /,\,$(1)) +,, $(subst /,\,$(1)) > $(DEVNULL)
	CONDITIONAL_CLONE=if not exist $(2) ( git clone -q $(1) $(2) )
endif
ifeq ($(os),linux)
	DEVNULL = /dev/null
	ensure_dir=mkdir -p $(@D) 2> $(DEVNULL) || exit 0
	RM = rm
	TOUCH = touch $(1)
	CONDITIONAL_CLONE=if [ ! -d $(2) ]; then git clone -q $(1) $(2) ; fi
endif

# SOURCE FILES
# ------------
srcs += deps/qajson4c/src/qajson4c/qajson4c.c
srcs += deps/qajson4c/src/qajson4c/qajson4c_internal.c
deps/qajson4c/src/qajson4c/%.o: deps/qajson4c/src/qajson4c/%.h

srcs += deps/miniz/miniz.c
deps/miniz/miniz.o: deps/miniz/miniz.h

srcs += src/minimod.c
src/minimod.o: include/minimod/minimod.h
src/minimod.o: src/netw.h
src/minimod.o: src/util.h

srcs += src/util.c
src/util.o: src/util.h

srcs += src/netw.c
src/netw.o: src/netw.h

ifeq ($(os),macos)
srcs += src/netw-macos.m
src/netw-macos.o: src/netw.h
endif

ifeq ($(os),windows)
srcs += src/netw-win.c
src/netw-win.o: src/netw.h
endif

test_srcs += tests/simple.c

# OBJECT FILES
# ------------
objs += $(subst .c,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.c,$(srcs))))
objs += $(subst .m,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.m,$(srcs))))

$(objs): CPPFLAGS += -DMINIMOD_BUILD_LIB

test_objs += $(subst .c,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.c,$(test_srcs))))

# LINKER OPTIONS
# --------------
TARGET_ARCH = -m64 -g -march=core2

ifeq ($(os),macos)
TARGET_ARCH += -arch x86_64 -mmacosx-version-min=$(MIN_MACOS_VERSION) -stdlib=libc++

LDLIBS += -lc++

$(LIB_PATH): LDLIBS += -framework Foundation
ifeq ($(USE_SANITIZER),1)
TARGET_ARCH += -fsanitize=address
TARGET_ARCH += -fsanitize=undefined
endif
endif

ifeq ($(os),windows)
TARGET_ARCH += -gcodeview
LDFLAGS += /NOLOGO /MACHINE:X64 /NODEFAULTLIB /INCREMENTAL:NO
LDFLAGS += /SUBSYSTEM:WINDOWS /DEBUG
LDLIBS += libucrt.lib
LDLIBS += libvcruntime.lib
LDLIBS += libcmt.lib
LDLIBS += libcpmt.lib
LDLIBS += ws2_32.lib
LDLIBS += kernel32.lib
LDLIBS += advapi32.lib
LDLIBS += crypt32.lib
endif

ifeq ($(os),linux)
# do not add exoprts from included static libs (i.e. OpenSSL)
# to this shared library's list of exports.
LDFLAGS += -Wl,--exclude-libs,ALL
LDLIBS += -lstdc++ -lpthread
# statically links OpenSSL, which is quite huge.
# maybe replace with mbdtls?
LDLIBS += -l:libssl.a
LDLIBS += -l:libcrypto.a
endif

# COMPILER OPTIONS
# ----------------
CFLAGS += -std=c11
CFLAGS += $(WARNINGS) $(NOWARNINGS) $(OPT)
OBJCFLAGS += $(WARNINGS) $(NOWARNINGS) $(OPT)

CPPFLAGS += -DMZ_ZIP_NO_ENCRYPTION

ifeq ($(os),windows)
CPPFLAGS += -DUNICODE -D_UNICODE
CPPFLAGS += -DNDEBUG
CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS
CPPFLAGS += -DCURL_STATICLIB
CPPFLAGS += -DHAS_STDINT_H
endif

ifeq ($(os),linux)
CPPFLAGS += -D_LARGEFILE64_SOURCE
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -fPIC
CFLAGS += -fvisibility=hidden
endif

ifeq ($(os),macos)
CFLAGS += -fvisibility=hidden
OBJCFLAGS += -fvisibility=hidden
endif

# TARGETS
# -------

clean-library:
	$(Q)$(RM) $(LIB_PATH)
clean-test:
	$(Q)$(RM) $(TEST_PATH)

clean: clean-library clean-test

$(objs): deps/dirent/include/dirent.h
$(objs): deps/qajson4c/src/qajson4c/qajson4c.h

minimod: $(LIB_PATH)
library: minimod

$(TEST_PATH): $(test_objs) $(LIB_PATH)
ifdef Q
	@echo Linking $@
endif
	$(Q)$(ensure_dir)

ifeq ($(os),macos)
	$(Q)$(CC) $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LIB_PATH) $(LDLIBS) $(OUTPUT_OPTION)
endif

test: $(TEST_PATH)
	$(Q)$(TEST_PATH)

$(LIB_PATH): $(objs)
ifdef Q
	@echo Linking $@
endif
	$(Q)$(ensure_dir)

ifeq ($(os),macos)
	$(Q)$(CC) -dynamiclib $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LDLIBS) $(OUTPUT_OPTION) -Wl,-install_name,@loader_path/$(notdir $@)
endif

# linker command line exceeds what windows can handle, so split object
# files and load them via @ into the linker
# thank you windows...
ifeq ($(os),windows)
	$(Q)del $(OUTPUT_DIR)\linker-input-files-1.txt
	$(Q)echo $(objs) >> $(OUTPUT_DIR)\linker-input-files-1.txt
	$(Q)$(LINKER) /DLL $(LDFLAGS) $(LDLIBS) /OUT:$(subst /,\,$@) @$(OUTPUT_DIR)\linker-input-files-1.txt
endif

ifeq ($(os),linux)
	$(Q)$(CC) -shared $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LDLIBS) $(OUTPUT_OPTION)
	$(Q)strip --strip-debug $@
endif


deps/dirent/include/dirent.h: Makefile
ifdef Q
	@echo Updating dependency: tronkko/dirent @ $(DIRENT_VERSION)
endif
	$(Q)$(call CONDITIONAL_CLONE,https://github.com/tronkko/dirent.git,deps/dirent)
	$(Q)git -C deps/dirent fetch --all --tags --quiet
	$(Q)git -C deps/dirent checkout $(DIRENT_VERSION) --quiet
	$(Q)$(call TOUCH,$@)

deps/qajson4c/src/qajson4c/qajson4c.h: Makefile
ifdef Q
	@echo Updating dependency: DeHecht/qajson4c @ $(QAJSON4C_VERSION)
endif
	$(Q)$(call CONDITIONAL_CLONE,https://github.com/DeHecht/qajson4c.git,deps/qajson4c)
	$(Q)git -C deps/qajson4c fetch --all --tags --quiet
	$(Q)git -C deps/qajson4c checkout $(QAJSON4C_VERSION) --quiet
	$(Q)$(call TOUCH,$@)


# SPECIAL FILE HANDLING
# ---------------------
$(OUTPUT_DIR)/src/%.o: CPPFLAGS += -Iinclude -Ideps/miniz -Ideps
$(OUTPUT_DIR)/tests/%.o: CPPFLAGS += -Iinclude
$(OUTPUT_DIR)/src/minimod.o: NOWARNINGS += -Wno-documentation

ifeq ($(os),windows)
$(OUTPUT_DIR)/src/%.o: CPPFLAGS += -Ideps/dirent/include
endif

# clang implied
ifneq ($(os),linux)
$(OUTPUT_DIR)/deps/%.o: NOWARNINGS += -Wno-everything
endif

# PATTERNS
# --------
$(OUTPUT_DIR)/%.o: %.c
ifdef Q
	@echo CC $<
endif
	$(Q)$(ensure_dir)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(TARGET_ARCH) -c $(OUTPUT_OPTION) $<

$(OUTPUT_DIR)/%.o: %.m
ifdef Q
	@echo Objective-C $<
endif
	$(Q)$(ensure_dir)
	$(Q)$(CC) $(CPPFLAGS) $(OBJCFLAGS) $(TARGET_ARCH) -c $(OUTPUT_OPTION) $<

