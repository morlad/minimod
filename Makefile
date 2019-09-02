# CONFIG
# ------
# destination of the build output (object files and executables)
OUTPUT_DIR := build

# by default the macos-build uses NSURLSession
# to use libcurl instead set USE_LIBCURL_ON_MACOS to 1
USE_LIBCURL_ON_MACOS = 0

# windows only:
# path to the LLVM installation
# ! do NOT add trailing \ 
# ! do NOT quote it
LLVM_PATH ?= C:\Program Files\LLVM

# set to 1 to enable all available sanitizers on this platform, 0 otherwise
ENABLE_SANITIZERS = 0

# path to NaturalDocs for 'docs'-target
NDOCS = ~/bin/NaturalDocs-1.52/NaturalDocs

ENABLE_LOG = 0

# INTERNAL CONFIG
# ---------------
QAJSON4C_VERSION = tags/1.1.1

Q = @

OPT = -O2
OPT += -fstrict-aliasing

# macos only:
MIN_MACOS_VERSION = 10.9


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


# MACROS/HELPERS
# --------------
ifeq ($(os),windows)
	DEVNULL = NUL
	CC = "$(LLVM_PATH)\bin\clang.exe"
	LINKER = "$(LLVM_PATH)\bin\lld-link.exe"
	ensure_dir=mkdir $(subst /,\,$(@D)) 2> $(DEVNULL) || exit 0
	ensure_selfdir=mkdir $(subst /,\,$@) 2> $(DEVNULL) || exit 0
	RM = del
	TOUCH = copy /b $(subst /,\,$(1)) +,, $(subst /,\,$(1)) > $(DEVNULL)
	CONDITIONAL_CLONE=if not exist $(2) ( git clone -q $(1) $(2) )
else
	DEVNULL = /dev/null
	ensure_dir=mkdir -p $(@D) 2> $(DEVNULL) || exit 0
	ensure_selfdir=mkdir -p $@ 2> $(DEVNULL) || exit 0
	RM = rm
	TOUCH = touch $(1)
	CONDITIONAL_CLONE=if [ ! -d $(2) ]; then git clone -q $(1) $(2) ; fi
endif


# NAMING OF THINGS
# ----------------
ifeq ($(os),macos)
LIBRARY_NAME = libminimod.dylib
TEST_NAME = testsuite
endif

ifeq ($(os),windows)
LIBRARY_NAME = minimod.dll
TEST_NAME = testsuite.exe
endif

ifeq ($(os),linux)
LIBRARY_NAME = libminimod.so
TEST_NAME = testsuite
endif

ifeq ($(os),freebsd)
LIBRARY_NAME = libminimod.so
TEST_NAME = testsuite
endif

TEST_PATH = $(OUTPUT_DIR)/$(TEST_NAME)
LIB_PATH = $(OUTPUT_DIR)/$(LIBRARY_NAME)


# PRIMARY TARGETS
# ---------------
all: library
.PHONY: library clean clean-library minimod all test docs format


# SOURCE FILES
# ------------
lib_srcs += src/minimod.c
lib_srcs += src/netw.c

ifeq ($(os),macos)
lib_srcs += src/util-posix.c
ifeq ($(USE_LIBCURL_ON_MACOS),0)
lib_srcs += src/netw-macos.m
else
lib_srcs += src/netw-libcurl.c
endif
endif

ifeq ($(os),windows)
lib_srcs += src/util-win.c
lib_srcs += src/netw-win.c
endif

ifeq ($(os),linux)
lib_srcs += src/util-posix.c
lib_srcs += src/netw-libcurl.c
endif

ifeq ($(os),freebsd)
lib_srcs += src/util-posix.c
lib_srcs += src/netw-libcurl.c
endif

lib_srcs += deps/qajson4c/src/qajson4c/qajson4c.c
lib_srcs += deps/qajson4c/src/qajson4c/qajson4c_internal.c
lib_srcs += deps/miniz/miniz.c

test_srcs += tests/examples.c

# OBJECT FILES
# ------------
lib_objs += $(subst .c,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.c,$(lib_srcs))))
lib_objs += $(subst .m,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.m,$(lib_srcs))))
test_objs += $(subst .c,.o,$(addprefix $(OUTPUT_DIR)/,$(filter %.c,$(test_srcs))))

# HEADER DEPENDENCIES
# -------------------
$(OUTPUT_DIR)/src/minimod.o: include/minimod/minimod.h src/netw.h src/util.h deps/qajson4c/src/qajson4c/qajson4c.h
$(OUTPUT_DIR)/deps/qajson4c/src/qajson4c/%.o: deps/qajson4c/src/qajson4c/qajson4c.h
$(OUTPUT_DIR)/deps/miniz/miniz.o: deps/miniz/miniz.h
$(OUTPUT_DIR)/src/util.o: src/util.h
$(OUTPUT_DIR)/src/netw.o: src/netw.h
$(OUTPUT_DIR)/src/netw-macos.o: src/netw.h
$(OUTPUT_DIR)/src/netw-win.o: src/netw.h src/util.h
$(test_objs): include/minimod/minimod.h


# WARNINGS
# --------
WARNINGS += -Werror

# basically GCC vs clang
ifneq ($(os),linux)
WARNINGS += -Weverything
NOWARNINGS += -Wno-error-unused-parameter
NOWARNINGS += -Wno-error-unused-function
NOWARNINGS += -Wno-error-unused-variable
else
WARNINGS += -Wall
NOWARNINGS += -Wno-unused-result
NOWARNINGS += -Wno-unused-function
endif


# COMPILER OPTIONS
# ----------------
CFLAGS += -std=c99
CFLAGS += $(WARNINGS) $(NOWARNINGS) $(OPT)
OBJCFLAGS += $(WARNINGS) $(NOWARNINGS) $(OPT)
ifeq ($(ENABLE_LOG),1)
CPPFLAGS += -DMINIMOD_LOG_ENABLE
endif

ifeq ($(os),macos)
CFLAGS += -fvisibility=hidden
OBJCFLAGS += -fvisibility=hidden
endif

ifeq ($(os),windows)
CPPFLAGS += -DUNICODE -D_UNICODE
CPPFLAGS += -DNDEBUG
CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS
CPPFLAGS += -DHAS_STDINT_H
endif

ifeq ($(os),linux)
CPPFLAGS += -D_LARGEFILE64_SOURCE
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -D_GNU_SOURCE
CFLAGS += -fPIC
CFLAGS += -fvisibility=hidden
endif

ifeq ($(os),freebsd)
CFLAGS += -fvisibility=hidden
CFLAGS += -fPIC
endif


# SPECIAL FILE HANDLING
# ---------------------
$(lib_objs): CPPFLAGS += -DMINIMOD_BUILD_LIB
$(lib_objs): CPPFLAGS += -DMZ_ZIP_NO_ENCRYPTION

$(OUTPUT_DIR)/src/%.o: CPPFLAGS += -Iinclude -Ideps/miniz -Ideps
$(OUTPUT_DIR)/tests/%.o: CPPFLAGS += -Iinclude

$(OUTPUT_DIR)/deps/miniz/miniz.o: CPPFLAGS += -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0

ifeq ($(os),macos)
$(OUTPUT_DIR)/src/netw-libcurl.o: NOWARNINGS += -Wno-disabled-macro-expansion
endif

ifeq ($(os),windows)
$(OUTPUT_DIR)/deps/miniz/miniz.o: CPPFLAGS += -D_LARGEFILE64_SOURCE=1
$(OUTPUT_DIR)/src/%.o: NOWARNINGS += -Wno-error-reserved-id-macro
$(OUTPUT_DIR)/src/%.o: NOWARNINGS += -Wno-error-nonportable-system-include-path
endif

ifeq ($(os),linux)
$(OUTPUT_DIR)/deps/miniz/miniz.o: CPPFLAGS += -D_LARGEFILE64_SOURCE=1
endif

ifeq ($(os),freebsd)
$(OUTPUT_DIR)/src/netw-libcurl.o: CPPFLAGS += -I/usr/local/include
$(OUTPUT_DIR)/src/netw-libcurl.o: NOWARNINGS += -Wno-disabled-macro-expansion
$(OUTPUT_DIR)/src/netw-libcurl.o: NOWARNINGS += -Wno-error-reserved-id-macro
$(OUTPUT_DIR)/src/minimod.o: NOWARNINGS += -Wno-error-padded
endif

# basically clang
ifneq ($(os),linux)
$(OUTPUT_DIR)/deps/%.o: NOWARNINGS += -Wno-everything
endif


# LINKER OPTIONS
# --------------
TARGET_ARCH = -m64 -g -march=core2

ifeq ($(os),macos)
TARGET_ARCH += -arch x86_64 -mmacosx-version-min=$(MIN_MACOS_VERSION)
$(LIB_PATH): LDLIBS += -framework Foundation
ifneq ($(USE_LIBCURL_ON_MACOS),0)
$(LIB_PATH): LDLIBS += -lcurl
endif
$(LIB_PATH): LDLIBS += -framework Foundation
ifeq ($(ENABLE_SANITIZERS),1)
TARGET_ARCH += -fsanitize=address
TARGET_ARCH += -fsanitize=undefined
endif
endif

ifeq ($(os),windows)
TARGET_ARCH += -gcodeview
LDFLAGS += /NOLOGO /MACHINE:X64 /NODEFAULTLIB /INCREMENTAL:NO
LDLIBS += libucrt.lib
LDLIBS += libvcruntime.lib
LDLIBS += libcmt.lib
LDLIBS += kernel32.lib
$(LIB_PATH): LDFLAGS += /SUBSYSTEM:WINDOWS
$(LIB_PATH): LDLIBS += winhttp.lib
$(TEST_PATH): LDFLAGS += /SUBSYSTEM:CONSOLE
$(TEST_PATH): LDLIBS += $(subst .dll,.lib,$(LIB_PATH))
endif

ifeq ($(os),linux)
# do not add exoprts from included static libs
# to this shared library's list of exports.
LDFLAGS += -Wl,--exclude-libs,ALL
LDLIBS += -lpthread
$(LIB_PATH): LDLIBS += -lcurl
endif

ifeq ($(os),freebsd)
LDFLAGS += -L/usr/local/lib
$(LIB_PATH): LDLIBS += -lcurl
endif


# TARGETS
# -------
clean-library:
	$(Q)$(RM) $(LIB_PATH)

clean-test:
	$(Q)$(RM) $(TEST_PATH)

clean: clean-library clean-test

minimod: $(LIB_PATH)

library: minimod

$(TEST_PATH): $(test_objs) $(LIB_PATH)
ifdef Q
	@echo Linking $@
endif
	$(Q)$(ensure_dir)
ifeq ($(os),windows)
	$(Q)$(LINKER) $(LDFLAGS) /OUT:$@ $(filter %.o,$^) $(filter %.res,$^) $(LDLIBS)
else
	$(Q)$(CC) $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LIB_PATH) $(LDLIBS) $(OUTPUT_OPTION)
endif

test: $(TEST_PATH)
	$(Q)$(TEST_PATH)

$(LIB_PATH): $(lib_objs)
ifdef Q
	@echo Linking $@
endif
	$(Q)$(ensure_dir)
ifeq ($(os),macos)
	$(Q)$(CC) -dynamiclib $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LDLIBS) $(OUTPUT_OPTION) -Wl,-install_name,@loader_path/$(notdir $@)
endif
ifeq ($(os),windows)
	$(Q)$(LINKER) /DLL $(LDFLAGS) $(LDLIBS) $(filter %.o,$^) /OUT:$(subst /,\,$@)
endif
ifeq ($(os),linux)
	$(Q)$(CC) -shared $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LDLIBS) $(OUTPUT_OPTION)
	$(Q)strip --strip-debug $@
endif
ifeq ($(os),freebsd)
	$(Q)$(CC) -shared $(TARGET_ARCH) $(LDFLAGS) $(filter %.o,$^) $(LDLIBS) $(OUTPUT_OPTION)
	$(Q)strip --strip-debug $@
endif

deps/qajson4c/src/qajson4c/qajson4c.h: Makefile
ifdef Q
	@echo Updating dependency: DeHecht/qajson4c @ $(QAJSON4C_VERSION)
endif
	$(Q)$(call CONDITIONAL_CLONE,https://github.com/DeHecht/qajson4c.git,deps/qajson4c)
	$(Q)git -C deps/qajson4c fetch --all --tags --quiet
	$(Q)git -C deps/qajson4c checkout $(QAJSON4C_VERSION) --quiet
	$(Q)$(call TOUCH,$@)


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


# MISC TARGETS
# ------------
cloc-all:
	$(Q)cloc . --quiet --exclude-dir=deps,build,docs,docs.cfg

cloc:
	$(Q)cloc src include Makefile --quiet

cloc-by-file:
	$(Q)cloc . --by-file --quiet --exclude-dir=deps,build,docs,docs.cfg

format:
	$(Q)clang-format -i tests/*.c src/*.c src/*.m src/*.h include/minimod/*.h

docs:
	$(Q)$(ensure_selfdir)
	$(Q)$(NDOCS) -i src -i include -p docs.cfg -s Default minimod -o html docs
