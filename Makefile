ifeq ($(origin CXX), default)
  CXX := clang++
endif
UNAME_S := $(shell uname -s)

STRICT_WARN := -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Werror

ifeq ($(UNAME_S),Darwin)
  HARDEN_STDLIB := -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
  PLATFORM_LDFLAGS :=
  RELEASE_LDFLAGS := -flto -Wl,-dead_strip
  STRIP_CMD := strip -x
else
  HARDEN_STDLIB := -D_GLIBCXX_ASSERTIONS
  PLATFORM_LDFLAGS := -ldl
  RELEASE_LDFLAGS := -flto -Wl,--gc-sections
  STRIP_CMD := strip --strip-unneeded
endif

BASE_CXXFLAGS := -std=c++20 $(STRICT_WARN) $(HARDEN_STDLIB) -fstack-protector-strong -Isrc
RELEASE_CXXFLAGS := $(BASE_CXXFLAGS) -O3 -flto -DNDEBUG -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
DEBUG_CXXFLAGS := $(BASE_CXXFLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer

UTIL_SRCS := $(wildcard src/util/*.cpp)
CORE_SRCS := $(wildcard src/core/*.cpp)
SURFACE_SRCS := $(wildcard src/surfaces/*.cpp)
MAIN_SRC := src/main.cpp
ALL_SRCS := $(MAIN_SRC) $(UTIL_SRCS) $(CORE_SRCS) $(SURFACE_SRCS)

HEADERS := $(wildcard src/util/*.hpp src/core/*.hpp src/ui/*.hpp src/surfaces/*.hpp)

TARGET := bin/loadout
DEBUG_TARGET := bin/loadout-debug
TEST_UTIL_BIN := bin/test_util
TEST_CORE_BIN := bin/test_core

.PHONY: all release debug test-util test-core test clean

all: release

release: $(TARGET)

$(TARGET): $(ALL_SRCS) $(HEADERS)
	@mkdir -p bin
	$(CXX) $(RELEASE_CXXFLAGS) $(ALL_SRCS) -o $(TARGET) $(RELEASE_LDFLAGS) $(PLATFORM_LDFLAGS)
	$(STRIP_CMD) $(TARGET)

debug: $(ALL_SRCS) $(HEADERS)
	@mkdir -p bin
	$(CXX) $(DEBUG_CXXFLAGS) $(ALL_SRCS) -o $(DEBUG_TARGET) $(PLATFORM_LDFLAGS)

test-util: $(UTIL_SRCS) tests/test_util.cpp $(wildcard src/util/*.hpp)
	@mkdir -p bin
	$(CXX) $(BASE_CXXFLAGS) -O2 $(UTIL_SRCS) tests/test_util.cpp -o $(TEST_UTIL_BIN)
	./$(TEST_UTIL_BIN)

test-core: $(UTIL_SRCS) $(CORE_SRCS) tests/test_core.cpp $(wildcard src/util/*.hpp src/core/*.hpp)
	@mkdir -p bin
	$(CXX) $(BASE_CXXFLAGS) -O2 $(UTIL_SRCS) $(CORE_SRCS) tests/test_core.cpp -o $(TEST_CORE_BIN)
	./$(TEST_CORE_BIN)

test: test-util test-core

clean:
	rm -rf bin
