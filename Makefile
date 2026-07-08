# rof2ClientPlus - a 32-bit ASI mod for the RoF2 EverQuest client.
# Cross-compiled on Linux with mingw-w64 (no MSVC / no Windows needed).

CXX      := i686-w64-mingw32-g++
NAME     := rof2ClientPlus
BUILD    := build
TARGET   := $(BUILD)/$(NAME).asi

# Per-machine config (game install path), kept out of git. Copy
# config.mk.example to config.mk and set GAME_DIR there. Leading '-' means
# "don't error if absent", so `make` still builds with no local config.
-include config.mk

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra \
            -DWIN32_LEAN_AND_MEAN -D_USE_MATH_DEFINES \
            -fpermissive \
            -ffunction-sections -fdata-sections

# -shared: build a DLL.  -static*: fold the C/C++ runtime in so the .asi has no
# external mingw runtime deps.  --exclude-all-symbols: export nothing (Miles
# only needs DllMain to run).
LDFLAGS  := -m32 -shared \
            -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections -Wl,--exclude-all-symbols

# Load-time deps. d3d9 is loaded dynamically at runtime. ws2_32/psapi mirror the
# libs Zeal links via #pragma comment(lib,...) (mingw resolves them via -l).
# dbghelp is for the crash handler's StackWalk64 (crash_handler.cpp).
LDLIBS   := -luser32 -lgdi32 -lws2_32 -lpsapi -ldbghelp

.PHONY: all clean install
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo ">> Built $@"

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

# Host-side helper (built with the HOST gcc, not mingw): keeps the windowed-mode
# game window visible under KWin/XWayland (see tools/eq_window_fix.c for the bug).
# Only built when a host gcc + X11 headers exist, so the cross-build alone still
# works anywhere.
HOST_CC  := gcc
HOSTFIX  := $(BUILD)/eq-window-fix
HAVE_X11 := $(shell command -v $(HOST_CC) >/dev/null 2>&1 && [ -e /usr/include/X11/Xlib.h ] && echo yes)

ifeq ($(HAVE_X11),yes)
all: $(HOSTFIX)

$(HOSTFIX): tools/eq_window_fix.c | $(BUILD)
	$(HOST_CC) -O2 -Wall -Wextra -o $@ $< -lX11
endif

# Copy the built .asi into the game root so Miles loads it on next launch.
# GAME_DIR comes from config.mk (or: make install GAME_DIR=/path/to/RoF2).
install: $(TARGET)
	@if [ -z "$(GAME_DIR)" ]; then \
	  echo "ERROR: GAME_DIR is not set."; \
	  echo "  Copy config.mk.example to config.mk and set GAME_DIR,"; \
	  echo "  or run: make install GAME_DIR=/path/to/RoF2"; \
	  exit 1; \
	fi
	@if [ ! -d "$(GAME_DIR)" ]; then \
	  echo "ERROR: GAME_DIR '$(GAME_DIR)' does not exist."; exit 1; \
	fi
	cp -f $(TARGET) "$(GAME_DIR)/$(NAME).asi"
	@echo ">> Installed to $(GAME_DIR)/$(NAME).asi"
	mkdir -p "$(GAME_DIR)/uifiles/rcp"
	cp -f uifiles/rcp/*.xml "$(GAME_DIR)/uifiles/rcp/"
	@echo ">> Installed uifiles to $(GAME_DIR)/uifiles/rcp/"
	@if [ -f $(HOSTFIX) ]; then \
	  cp -f $(HOSTFIX) "$(GAME_DIR)/eq-window-fix"; \
	  echo ">> Installed eq-window-fix (windowed-mode watcher)"; \
	fi

clean:
	rm -rf $(BUILD)
