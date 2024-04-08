VERSION := 1.1.0

# Directories
SRC_DIR := core/src
BIN_DIR := bin
OBJ_DIR := $(BIN_DIR)/obj
PLUGINS_DIR := $(BIN_DIR)/plugins

# Output
LIB_NAME := core.dylib
LIB_OUT_DIR := $(BIN_DIR)/dylib
LIB_OUT_PATH := $(LIB_OUT_DIR)/$(LIB_NAME)

# Target
LOL_ROOT_DIR := "/Applications/League of Legends.app/Contents/LoL"
LOL_FRAMEWORKS_DIR := $(LOL_ROOT_DIR)/"League of Legends.app/Contents/Frameworks"

TARGET_LIB_NAME := libEGL.dylib
TARGET_LIB_DIR := $(LOL_FRAMEWORKS_DIR)/"Chromium Embedded Framework.framework/Libraries"
TARGET_LIB_PATH := $(TARGET_LIB_DIR)/$(TARGET_LIB_NAME)
LIB_ORIG_DIR := $(LIB_OUT_DIR)
LIB_ORIG_PATH := $(LIB_ORIG_DIR)/$(TARGET_LIB_NAME)

# "/Applications/League of Legends.app/Contents/LoL/League of Legends.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libEGL.dylib"

# Compiler
CXX := clang++
CXXFLAGS := -std=c++20 -g -fPIC -arch x86_64 -I./core -I./core/src/ -fvisibility=hidden -Wno-address-of-temporary -Wno-nonportable-include-path

# Linker
LDFLAGS := -shared -dynamiclib -arch x86_64 -current_version $(VERSION) -compatibility_version 1.0.0
LDLIBS := -framework cocoa -F $(LOL_FRAMEWORKS_DIR) -framework "Chromium Embedded Framework" -Wl,-reexport_library,"$(LIB_ORIG_PATH)"

# Source files
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cc) $(wildcard $(SRC_DIR)/**/*.cc)
OBJCXX_SRCS := $(wildcard $(SRC_DIR)/**/*.mm)
INC_HEADERS := $(wildcard $(SRC_DIR)/*.h)

# Object files
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(CPP_SRCS))
OBJCXX_OBJS := $(patsubst $(SRC_DIR)/%.mm,$(OBJ_DIR)/%.o,$(OBJCXX_SRCS))

# Default target
all: $(LIB_OUT_PATH)

# Rule to compile C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc ${INC_HEADERS}
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to compile Objective-C++ source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.mm
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -x objective-c++ -c -o $@ $<

$(LIB_ORIG_PATH):
	@mkdir -p $(LIB_ORIG_DIR)
	@cp -n $(TARGET_LIB_PATH) $(LIB_ORIG_PATH)

# Rule to link the dylib
$(LIB_OUT_PATH): $(CPP_OBJS) $(OBJCXX_OBJS) $(LIB_ORIG_PATH)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(CPP_OBJS) $(OBJCXX_OBJS) $(LDLIBS)
	@install_name_tool -change "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "@loader_path/../Chromium Embedded Framework" $@
	@install_name_tool -change "./$(TARGET_LIB_NAME)" "$(abspath $(LIB_ORIG_PATH))" $@
	dsymutil $@

install: $(LIB_OUT_PATH)
	cp $(LIB_OUT_PATH) $(TARGET_LIB_PATH)

restore: $(LIB_ORIG_PATH)
	cp $(LIB_ORIG_PATH) $(TARGET_LIB_PATH)

clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(LIB_OUT_PATH)
	@rm -rf $(LIB_OUT_PATH).dSYM

# Open plugins folder
open:
	@mkdir -p $(PLUGINS_DIR)
	@open $(PLUGINS_DIR)

.PHONY: all install clean