# === Configuration ===

# Compiler
CC = clang

# --- Projects ---
ENGINE_NAME = engine
TESTBED_NAME = testbed

# --- Directories ---
SRC_ENGINE = engine/src
SRC_TESTBED = testbed/src
OBJ_ENGINE = obj/engine
OBJ_TESTBED = obj/testbed
BIN_DIR = bin

# --- Output Binaries ---
ENGINE_TARGET = $(BIN_DIR)/lib$(ENGINE_NAME).so
TESTBED_TARGET = $(BIN_DIR)/$(TESTBED_NAME)

# --- File Discovery ---
ENGINE_SOURCES = $(shell find $(SRC_ENGINE) -name "*.c")
ENGINE_OBJECTS = $(patsubst $(SRC_ENGINE)/%.c, $(OBJ_ENGINE)/%.o, $(ENGINE_SOURCES))

TESTBED_SOURCES = $(shell find $(SRC_TESTBED) -name "*.c")
TESTBED_OBJECTS = $(patsubst $(SRC_TESTBED)/%.c, $(OBJ_TESTBED)/%.o, $(TESTBED_SOURCES))

# --- Build Flags ---
DEFINES = -D_DEBUG -DKEXPORT
INCLUDE_FLAGS = -I$(SRC_ENGINE) -I$(SRC_TESTBED) -I$(VULKAN_SDK)/include
CFLAGS = -g -Wall -Werror -Wvarargs -fPIC
LINKER_FLAGS_ENGINE = -shared -fPIC -lvulkan
LINKER_FLAGS_TESTBED = -lvulkan -ldl

CPPFLAGS = $(DEFINES) $(INCLUDE_FLAGS)

# === Targets ===

.PHONY: all clean engine testbed

all: $(ENGINE_TARGET) $(TESTBED_TARGET)

# --- ENGINE (.so library) ---
engine: $(ENGINE_TARGET)

$(ENGINE_TARGET): $(ENGINE_OBJECTS)
	@echo "Linking $(ENGINE_NAME) shared library..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(ENGINE_OBJECTS) -o $@ $(LINKER_FLAGS_ENGINE)

$(OBJ_ENGINE)/%.o: $(SRC_ENGINE)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# --- TESTBED (executable) ---
testbed: $(TESTBED_TARGET)

$(TESTBED_TARGET): $(TESTBED_OBJECTS) $(ENGINE_TARGET)
	@echo "Linking $(TESTBED_NAME) executable..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(TESTBED_OBJECTS) -o $@ $(LINKER_FLAGS_TESTBED) -L$(BIN_DIR) -l$(ENGINE_NAME) -Wl,-rpath,'$$ORIGIN'

$(OBJ_TESTBED)/%.o: $(SRC_TESTBED)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# --- Clean build artifacts ---
clean:
	@echo "Cleaning..."
	rm -rf obj $(BIN_DIR)
