# Get version from package.json
MAKEFILE_DIR := $(dir $(firstword $(MAKEFILE_LIST)))
VERSION := $(shell node -p "require('$(MAKEFILE_DIR)../package.json').version" 2>/dev/null || echo 0.0.0)

# Directories
SRC_DIR := ./src
VENDOR_DIR := ./vendor
BIN_DIR := ./bin
OBJ_DIR := $(BIN_DIR)/obj
PLUGINS_DIR := $(BIN_DIR)/plugins

# Output
LIB_NAME := core.dylib
LIB_OUT_PATH := $(BIN_DIR)/$(LIB_NAME)
INSERT_DYLIB_PATH := $(BIN_DIR)/insert_dylib

# Target
LOL_ROOT_DIR := "/Applications/League of Legends.app/Contents/LoL"
LOL_FRAMEWORKS_DIR := $(LOL_ROOT_DIR)/"League of Legends.app/Contents/Frameworks"

TARGET_LIB_DIR := $(LOL_FRAMEWORKS_DIR)/"Chromium Embedded Framework.framework/Libraries"
TARGET_LIB_PATH := $(TARGET_LIB_DIR)/libEGL.dylib

# "/Applications/League of Legends.app/Contents/LoL/League of Legends.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libEGL.dylib"

# Vendored sqlite3 (core/vendor/sqlite). Mirrors the sqlite3 target in
# CMakeLists.txt — keep the two lists in step, and see
# core/vendor/sqlite/README.md for what each flag buys.
#
# These have to reach the C++ side too, not just sqlite3.c: sqlite3.h itself
# branches on SQLITE_OMIT_DEPRECATED and SQLITE_OMIT_LOAD_EXTENSION, so a
# translation unit that included the header without them would see an API the
# library does not contain.
SQLITE_DEFS := \
	-DSQLITE_OMIT_LOAD_EXTENSION \
	-DSQLITE_DQS=0 \
	-DSQLITE_THREADSAFE=1 \
	-DSQLITE_DEFAULT_MEMSTATUS=0 \
	-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 \
	-DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
	-DSQLITE_MAX_EXPR_DEPTH=0 \
	-DSQLITE_OMIT_DECLTYPE \
	-DSQLITE_OMIT_DEPRECATED \
	-DSQLITE_OMIT_PROGRESS_CALLBACK \
	-DSQLITE_OMIT_SHARED_CACHE \
	-DSQLITE_USE_ALLOCA

# Compiler
CC := clang
CXX := clang++
CXXFLAGS := -std=c++20 -fPIC -arch x86_64 -I./cef -I./src -I$(VENDOR_DIR)/sqlite $(SQLITE_DEFS) -fvisibility=hidden -Wno-address-of-temporary -Wno-nonportable-include-path

# -w because this is third-party source we do not edit, so its warnings are
# noise we cannot act on.
CFLAGS := -fPIC -arch x86_64 -I$(VENDOR_DIR)/sqlite $(SQLITE_DEFS) -fvisibility=hidden -w

# Linker
LDFLAGS := -shared -dynamiclib -arch x86_64 -current_version $(VERSION) -compatibility_version 1.0.0
LDLIBS := -framework cocoa -L./cef/lib/mac -weak-lcef.d -flat_namespace
	
# Source files
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cc) $(wildcard $(SRC_DIR)/**/*.cc)
OBJCXX_SRCS := $(wildcard $(SRC_DIR)/**/*.mm)
INC_HEADERS := $(wildcard $(SRC_DIR)/*.h)

# Listed, not globbed: vendored C is a fixed, small set, and a glob over
# vendor/ would silently pick up anything dropped there later.
C_SRCS := $(VENDOR_DIR)/sqlite/sqlite3.c

# Object files
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(CPP_SRCS))
OBJCXX_OBJS := $(patsubst $(SRC_DIR)/%.mm,$(OBJ_DIR)/%.o,$(OBJCXX_SRCS))
C_OBJS := $(patsubst $(VENDOR_DIR)/%.c,$(OBJ_DIR)/vendor/%.o,$(C_SRCS))

# Default target
all: release

debug: CXXFLAGS += -DDEBUG -g
debug: CFLAGS += -g
debug: $(LIB_OUT_PATH)
debug: $(INSERT_DYLIB_PATH)

release: CXXFLAGS += -DNDEBUG -O3
release: CFLAGS += -DNDEBUG -O2
release: LDFLAGS += -flto
release: clean $(LIB_OUT_PATH)
release: $(INSERT_DYLIB_PATH)

# Rule to compile C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc ${INC_HEADERS}
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to compile Objective-C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.mm
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -x objective-c++ -c -o $@ $<

# Rule to compile vendored C source files. Deliberately not dependent on
# INC_HEADERS — sqlite3.c does not include anything from src/, and making it
# recompile whenever a core header changes would add half a minute to every
# incremental build for nothing.
$(OBJ_DIR)/vendor/%.o: $(VENDOR_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to link the dylib
$(LIB_OUT_PATH): $(CPP_OBJS) $(OBJCXX_OBJS) $(C_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(CPP_OBJS) $(OBJCXX_OBJS) $(C_OBJS) $(LDLIBS)
#	@install_name_tool -change "libcef.d.dylib" "@loader_path/../Chromium Embedded Framework" $@

$(INSERT_DYLIB_PATH):
	$(CC) -O2 -o $@ ./insert_dylib.c

install: $(LIB_OUT_PATH) $(INSERT_DYLIB_PATH)
	cp -n $(TARGET_LIB_PATH) $(TARGET_LIB_PATH).bak || true
	$(abspath $(INSERT_DYLIB_PATH)) --all-yes --inplace $(abspath $(LIB_OUT_PATH)) $(TARGET_LIB_PATH)

restore:
	cp $(TARGET_LIB_PATH).bak $(TARGET_LIB_PATH)

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(LIB_OUT_PATH)
	rm -f $(INSERT_DYLIB_PATH)

# Open plugins folder
open:
	@mkdir -p $(PLUGINS_DIR)
	@open $(PLUGINS_DIR)

.PHONY: all install restore clean open