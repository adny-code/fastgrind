## Simple Makefile replicating the logic in ../compile_with_bash/build.sh
## Build steps:
##  1. Compile pkgA / pkgB / pkgC sources into objects under build/...
##  2. Archive each package into a static library (libpkgA.a, libpkgB.a, libpkgC.a)
##  3. Compile main.cpp
##  4. Link final executable build/app with memory allocation wrapper symbols
## All variables can be overridden from command line, e.g. `make CXX=clang++`.

## Compiler / Tools
CXX       ?= g++
AR        ?= ar
ARFLAGS   ?= rcs

## Flags
CXXSTD    ?= c++11
CXXWARN    = -Wall -Wextra
OPTFLAGS  ?= -O3 -g
CXXFLAGS  ?= $(OPTFLAGS) $(CXXWARN) -std=$(CXXSTD) -DMERECORDER_INSTRUMENT
LDFLAGS   ?=

## Paths
REPO_ROOT := ../../..
BUILD_DIR := build

## Include directories
INCLUDE_DIRS := -I$(REPO_ROOT)/include -I. -IpkgA -IpkgB -IpkgC

## Allocation wrapper linker flags (same list as build.sh)
WRAP_FLAGS = \
	-Wl,--wrap=malloc \
	-Wl,--wrap=calloc \
	-Wl,--wrap=realloc \
	-Wl,--wrap=free \
	-Wl,--wrap=_Znwm \
	-Wl,--wrap=_Znam \
	-Wl,--wrap=_ZdlPv \
	-Wl,--wrap=_ZdaPv \
	-Wl,--wrap=_ZnwmRKSt9nothrow_t \
	-Wl,--wrap=_ZnamRKSt9nothrow_t \
	-Wl,--wrap=_ZdlPvRKSt9nothrow_t \
	-Wl,--wrap=_ZdaPvRKSt9nothrow_t \
	-Wl,--wrap=valloc \
	-Wl,--wrap=pvalloc \
	-Wl,--wrap=memalign \
	-Wl,--wrap=posix_memalign \
	-Wl,--wrap=reallocarray \
	-Wl,--wrap=_ZdaPvm \
	-Wl,--wrap=_ZdlPvm \
	-Wl,--wrap=aligned_alloc \
	-Wl,--wrap=_ZnwmSt11align_val_t \
	-Wl,--wrap=_ZnamSt11align_val_t \
	-Wl,--wrap=_ZdlPvSt11align_val_t \
	-Wl,--wrap=_ZdaPvSt11align_val_t \
	-Wl,--wrap=_ZdlPvmSt11align_val_t \
	-Wl,--wrap=_ZdaPvmSt11align_val_t \
	-Wl,--wrap=_ZdlPvmRKSt9nothrow_t \
	-Wl,--wrap=_ZdaPvmRKSt9nothrow_t \
	-Wl,--wrap=_ZnwmSt11align_val_tRKSt9nothrow_t \
	-Wl,--wrap=_ZnamSt11align_val_tRKSt9nothrow_t \
	-Wl,--wrap=_ZdlPvSt11align_val_tRKSt9nothrow_t \
	-Wl,--wrap=_ZdaPvSt11align_val_tRKSt9nothrow_t \
	-Wl,--wrap=_ZdlPvmSt11align_val_tRKSt9nothrow_t \
	-Wl,--wrap=_ZdaPvmSt11align_val_tRKSt9nothrow_t

# Multi-line (no added spaces) using backslash-newline so the value is identical to the previous single line.
EXCLUDE_FILE_LISTS ?= /usr/include/c++/,\
/usr/include/x86_64-linux-gnu/c++/,\
/usr/lib/gcc/,\
/usr/include/x86_64-linux-gnu/,\
/usr/include/linux/,\
/usr/include/asm,\
/usr/include/asm-generic,\
/usr/include/sys/,\
/usr/include/bits/,\
/usr/include/gnu/,\
/usr/include/glib-2.0/,\
/usr/lib/x86_64-linux-gnu/glib-2.0/include/,\
/usr/include/c++/v1,\
/usr/lib/clang/,\
/usr/local/include/,\
memProbe.h,\
/usr/include/boost/,\
/usr/include/eigen3/,\
/usr/include/openssl/,\
/usr/include/libunwind/,\
/usr/include/jemalloc/,\
/usr/include/tcmalloc/,\
/usr/include/gperftools/,\
/usr/include/google/,\
/usr/include/valgrind/,\
/usr/include/cuda/,\
/usr/local/cuda/include/,\
/opt/local/include/,\
$(REPO_ROOT)/third_party/,\
$(REPO_ROOT)/extern/,\
$(REPO_ROOT)/vendor/,\
$(REPO_ROOT)/include/
# Remove any spaces introduced by newline folding to keep a pure comma list.
empty :=
space := $(empty) $(empty)
EXCLUDE_FILE_LISTS := $(subst $(space),,$(EXCLUDE_FILE_LISTS))
INSTRUMENT_FLAGS ?= -finstrument-functions -finstrument-functions-exclude-file-list=$(EXCLUDE_FILE_LISTS)

# Append instrumentation flags
CXXFLAGS += $(INSTRUMENT_FLAGS)

LINK_FLAGS ?= -Wl,--export-dynamic

## Sources
PKG_A_SRCS := pkgA/PackageA.cpp
PKG_B_SRCS := pkgB/PackageB.cpp
PKG_C_SRCS := pkgC/PackageC.cpp
MAIN_SRCS  := main.cpp

## Object files (placed under build/ keeping subdir structure)
PKG_A_OBJS := $(PKG_A_SRCS:%.cpp=$(BUILD_DIR)/%.o)
PKG_B_OBJS := $(PKG_B_SRCS:%.cpp=$(BUILD_DIR)/%.o)
PKG_C_OBJS := $(PKG_C_SRCS:%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJS  := $(MAIN_SRCS:%.cpp=$(BUILD_DIR)/%.o)

## Libraries
LIB_PKG_A := $(BUILD_DIR)/libpkgA.a
LIB_PKG_B := $(BUILD_DIR)/libpkgB.a
LIB_PKG_C := $(BUILD_DIR)/libpkgC.a

LIBS := $(LIB_PKG_C) $(LIB_PKG_B) $(LIB_PKG_A)

## Final executable
APP := $(BUILD_DIR)/app

.PHONY: all clean run rebuild

all: $(APP)

## Link final executable (order of libs matters if there are inter-dependencies)
$(APP): $(MAIN_OBJS) $(LIBS)
	@echo [LINK ] $@
	$(CXX) -pthread -o $@ $(MAIN_OBJS) $(LIBS) $(WRAP_FLAGS) $(LDFLAGS) $(LINK_FLAGS)
	@echo [DONE ] $@

## Static libraries
$(LIB_PKG_A): $(PKG_A_OBJS)
	@echo [AR   ] $@
	$(AR) $(ARFLAGS) $@ $^

$(LIB_PKG_B): $(PKG_B_OBJS)
	@echo [AR   ] $@
	$(AR) $(ARFLAGS) $@ $^

$(LIB_PKG_C): $(PKG_C_OBJS)
	@echo [AR   ] $@
	$(AR) $(ARFLAGS) $@ $^

## Generic rule for compiling C++ sources into build/ dir while mirroring path
$(BUILD_DIR)/%.o: %.cpp | prepare_dirs
	@echo [CXX  ] $<
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

## Prepare needed directory roots (order-only prerequisite for objects)
.PHONY: prepare_dirs
prepare_dirs:
	@mkdir -p $(BUILD_DIR)/pkgA $(BUILD_DIR)/pkgB $(BUILD_DIR)/pkgC

## Convenience targets
run: $(APP)
	./$(APP)

rebuild: clean all

clean:
	@echo [CLEAN] Removing $(BUILD_DIR)
	rm -rf $(BUILD_DIR)

## Print useful variables (debug helper)
.PHONY: vars
vars:
	@echo CXX=$(CXX)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo WRAP_FLAGS=$(WRAP_FLAGS)
	@echo APP=$(APP)
