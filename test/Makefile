CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I../Include -I../src
SODIUM_LIBS := $(shell pkg-config --static --libs libsodium 2>/dev/null)
ifeq ($(strip $(SODIUM_LIBS)),)
    SODIUM_LIBS = -lsodium
endif
LIBS = $(SODIUM_LIBS) -lpthread

ifeq ($(OS),Windows_NT)
    EXE_EXT = .exe
endif

ifdef SANITIZE
    CXXFLAGS += -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wno-maybe-uninitialized
    LIBS += -fsanitize=address,undefined
endif

TEST_SOURCES = test_crypto.cpp test_stego.cpp
SRC_SOURCES = ../src/crypto_utils.cpp ../src/image_gen.cpp ../src/stego.cpp ../src/image_utils.cpp ../Include/lodepng.cpp
CATCH_SRC = catch_amalgamated.cpp

test_runner: $(TEST_SOURCES) $(SRC_SOURCES) $(CATCH_SRC)
	$(CXX) $(CXXFLAGS) -o test_runner \
		$(TEST_SOURCES) $(SRC_SOURCES) $(CATCH_SRC) $(LIBS)

.PHONY: test clean
test: test_runner
	./test_runner --allow-running-no-tests
	../test/smoke_test.sh ../passpix$(EXE_EXT)

clean:
	rm -f test_runner *.o
