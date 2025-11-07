# === Configuration ===

# Compiler
CC = clang

# --- Projects ---
ENGINE_NAME = engine
TESTBED_NAME = testbed
TESTS_NAME = tests

# --- Directories ---
SRC_ENGINE = engine/src
SRC_TESTBED = testbed/src
SRC_TESTS = tests/src
OBJ_ENGINE = obj/engine
OBJ_TESTBED = obj/testbed
OBJ_TESTS = obj/tests
BIN_DIR = bin

ASSET_SRC = assets
ASSET_DST = $(BIN_DIR)/assets
SHADER_SRC_DIR = $(ASSET_SRC)/shaders
SHADER_DST_DIR = $(ASSET_DST)/shaders

# --- Output Binaries ---
ENGINE_TARGET = $(BIN_DIR)/lib$(ENGINE_NAME).so
TESTBED_TARGET = $(BIN_DIR)/$(TESTBED_NAME)
TESTS_TARGET = $(BIN_DIR)/$(TESTS_NAME)

# --- File Discovery (C sources) ---
ENGINE_SOURCES = $(shell find $(SRC_ENGINE) -name "*.c")
ENGINE_OBJECTS = $(patsubst $(SRC_ENGINE)/%.c, $(OBJ_ENGINE)/%.o, $(ENGINE_SOURCES))

TESTBED_SOURCES = $(shell find $(SRC_TESTBED) -name "*.c")
TESTBED_OBJECTS = $(patsubst $(SRC_TESTBED)/%.c, $(OBJ_TESTBED)/%.o, $(TESTBED_SOURCES))

TESTS_SOURCES = $(shell find $(SRC_TESTS) -name "*.c")
TESTS_OBJECTS = $(patsubst $(SRC_TESTS)/%.c, $(OBJ_TESTS)/%.o, $(TESTS_SOURCES))

# --- Shaders (after renaming to .vert/.frag) ---
SHADER_SOURCES = $(wildcard $(SHADER_SRC_DIR)/*)
SHADER_OUTPUTS = $(patsubst $(SHADER_SRC_DIR)/%, $(SHADER_DST_DIR)/%.spv, $(SHADER_SOURCES))

# --- Build Flags ---
DEFINES = -D_DEBUG -DKEXPORT
INCLUDE_FLAGS = -I$(SRC_ENGINE) -I$(SRC_TESTBED) -I$(VULKAN_SDK)/include
CFLAGS = -g -Wall -Werror -Wvarargs -fPIC
CPPFLAGS = $(DEFINES) $(INCLUDE_FLAGS)

LINKER_FLAGS_ENGINE = -shared -fPIC -lvulkan -lX11 -lxcb -lX11-xcb -lwayland-client -lxkbcommon -lm
LINKER_FLAGS_TESTBED = -lvulkan -ldl -L$(BIN_DIR) -l$(ENGINE_NAME) -Wl,-rpath,'$$ORIGIN'

# === Targets ===

.PHONY: all clean engine testbed shaders assets tests run_tests

# Default build: everything
all: $(ENGINE_TARGET) $(TESTBED_TARGET) shaders assets $(TESTS_TARGET) run_tests

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
	$(CC) $(TESTBED_OBJECTS) -o $@ $(LINKER_FLAGS_TESTBED)

$(OBJ_TESTBED)/%.o: $(SRC_TESTBED)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# --- TESTS (executable) ---
tests: $(TESTS_TARGET)

$(TESTS_TARGET): $(TESTS_OBJECTS) $(ENGINE_TARGET)
	@echo "Linking $(TESTS_NAME) executable..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(TESTS_OBJECTS) -o $@ $(LINKER_FLAGS_TESTBED)

$(OBJ_TESTS)/%.o: $(SRC_TESTS)/%.c
	@echo "Compiling test source $<..."
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

run_tests: $(TESTS_TARGET)
	@echo "Running tests..."
	@$(BIN_DIR)/$(TESTS_NAME)

# --- Shaders (GLSL → SPIR-V) ---
$(SHADER_DST_DIR)/%.spv: $(SHADER_SRC_DIR)/%
	@echo "Compiling shader $< -> $@"
	@mkdir -p $(dir $@)
	$(VULKAN_SDK)/bin/glslc -o $@ $<

shaders: $(SHADER_OUTPUTS)

# --- Copy other assets (non-shader) ---
assets:
	@echo "Copying assets..."
	@mkdir -p $(ASSET_DST)
	rsync -a $(ASSET_SRC)/ $(ASSET_DST)/ --exclude 'shaders'

# --- Clean ---
clean:
	@echo "Cleaning..."
	rm -rf obj $(BIN_DIR)
