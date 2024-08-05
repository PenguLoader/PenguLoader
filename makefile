VERSION := 1.1.0

# Directories
SRC_DIR := core/src
BIN_DIR := bin
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

# Compiler
CC := clang
CXX := clang++
CXXFLAGS := -std=c++20 -fPIC -arch x86_64 -I./core/cef -I./core/src -fvisibility=hidden -Wno-address-of-temporary -Wno-nonportable-include-path

# Linker
LDFLAGS := -shared -dynamiclib -arch x86_64 -current_version $(VERSION) -compatibility_version 1.0.0
LDLIBS := -framework cocoa -Lcore/cef/lib/mac -weak-lcef.d -flat_namespace

# Source files
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cc) $(wildcard $(SRC_DIR)/**/*.cc)
OBJCXX_SRCS := $(wildcard $(SRC_DIR)/**/*.mm)
INC_HEADERS := $(wildcard $(SRC_DIR)/*.h)

# Object files
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(CPP_SRCS))
OBJCXX_OBJS := $(patsubst $(SRC_DIR)/%.mm,$(OBJ_DIR)/%.o,$(OBJCXX_SRCS))

# Default target
all: debug

debug: CXXFLAGS += -DDEBUG -g
debug: $(LIB_OUT_PATH)

release: CXXFLAGS += -DNDEBUG -O3
release: LDFLAGS += -flto
release: clean $(LIB_OUT_PATH)

# Rule to compile C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc ${INC_HEADERS}
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to compile Objective-C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.mm
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -x objective-c++ -c -o $@ $<

# Rule to link the dylib
$(LIB_OUT_PATH): $(CPP_OBJS) $(OBJCXX_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(CPP_OBJS) $(OBJCXX_OBJS) $(LDLIBS)
#	@install_name_tool -change "libcef.d.dylib" "@loader_path/../Chromium Embedded Framework" $@

$(INSERT_DYLIB_PATH):
	$(CC) -O2 -o $@ core/insert_dylib.c

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