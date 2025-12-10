# Makefile for Password_To_Image
# Optimized for MinGW make on Windows and native builds on Linux

# Compiler and tools
CXX ?= g++

# Detect platform
ifeq ($(OS),Windows_NT)
    # Windows with MinGW
    PLATFORM = Windows
    EXE_EXT = .exe
    RM = cmd /c del /Q
    RMDIR = cmd /c rd /s /q
    MKDIR = cmd /c if not exist
    PATH_SEP = \\
else
    # Linux/Unix
    PLATFORM = Linux
    EXE_EXT =
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    PATH_SEP = /
endif

# Project settings
TARGET = enc_dec$(EXE_EXT)
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SOURCES = src/main.cpp \
          src/crypto_utils.cpp \
          src/image_utils.cpp \
          Include/lodepng.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Include directories
INCLUDES = -I. -IInclude -Isrc

# Optimization and warning flags
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic
OPTFLAGS = -O3 -march=native -mtune=native -flto
LDFLAGS = -flto

# Platform-specific settings
ifeq ($(PLATFORM),Windows)
    # Windows/MinGW specific settings
    LIBS = -lssl -lcrypto -lws2_32 -lcrypt32
    # Use static linking on Windows to avoid DLL dependencies
    LDFLAGS += -static
else
    # Linux specific settings
    LIBS = -lssl -lcrypto -lpthread
endif

# Debug build support
ifdef DEBUG
    CXXFLAGS += -g -O0 -DDEBUG
    OPTFLAGS =
    LDFLAGS = 
else
    CXXFLAGS += $(OPTFLAGS) -DNDEBUG
endif

# Build rules
.PHONY: all clean rebuild install help

all: $(TARGET)

# Link executable
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Compile source files
$(OBJ_DIR)/%.o: %.cpp
	@echo "Compiling $<..."
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
ifeq ($(PLATFORM),Windows)
	@if exist $(BUILD_DIR) $(RMDIR) $(BUILD_DIR)
	@if exist $(TARGET) $(RM) $(TARGET)
else
	@$(RMDIR) $(BUILD_DIR) 2>/dev/null || true
	@$(RM) $(TARGET) 2>/dev/null || true
endif
	@echo "Clean complete"

# Rebuild from scratch
rebuild: clean all

# Install (copy to /usr/local/bin on Linux, not applicable on Windows)
install: $(TARGET)
ifeq ($(PLATFORM),Linux)
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installed $(TARGET) to /usr/local/bin/"
else
	@echo "Install target not supported on Windows. Copy $(TARGET) manually to desired location."
endif

# Help target
help:
	@echo "Password_To_Image Makefile"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Build the project (optimized)"
	@echo "  make DEBUG=1      - Build with debug symbols"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make rebuild      - Clean and build"
	@echo "  make install      - Install binary (Linux only)"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CXX)"

# Dependencies
-include $(OBJECTS:.o=.d)
