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

# -MMD -MP: emit .d header-dependency files so editing a header recompiles every .cpp
# that includes it (without this, e.g. changing a constant in a shared header silently
# leaves stale .o files - a real bug we hit).
CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra \
            -DWIN32_LEAN_AND_MEAN -D_USE_MATH_DEFINES \
            -fpermissive \
            -ffunction-sections -fdata-sections \
            -MMD -MP

# -shared: build a DLL.  -static*: fold the C/C++ runtime in so the .asi has no
# external mingw runtime deps.  --exclude-all-symbols: export nothing (Miles
# only needs DllMain to run).
LDFLAGS  := -m32 -shared \
            -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections -Wl,--exclude-all-symbols

# Load-time deps. d3d9 is loaded dynamically at runtime. ws2_32/psapi mirror the
# libs Zeal links via #pragma comment(lib,...) (mingw resolves them via -l).
# dbghelp is for the crash handler's StackWalk64 (crash_handler.cpp). d3dx9_30 is
# the D3DX helper (D3DXCreateTexture / D3DXMatrix*) used by the bitmap-font engine;
# it matches the game's own d3dx9_30.dll (see PORTING_NOTES.md "N4").
LDLIBS   := -luser32 -lgdi32 -lws2_32 -lpsapi -ldbghelp -ld3dx9_30

.PHONY: all clean install models install-models dist package
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo ">> Built $@"

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pull in the generated per-object header dependencies (see -MMD above).
-include $(OBJS:.o=.d)

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
	@# Atomic install: write each file to a temp name in the SAME directory, then
	@# rename(2) it over the target. rename is atomic and gives the target a fresh
	@# inode, so a client that is STILL RUNNING while we deploy keeps its old, intact
	@# mapping (the .asi is mmap'd; the fonts/textures/xml are read lazily). An
	@# in-place `cp -f` overwrites the bytes underneath that mapping instead, and the
	@# running client then faults in corrupt/zeroed pages -> the mid-session hang or
	@# crash. One shell so the acp() helper is shared across all the copies below.
	@set -e; \
	acp() { d="$$2"; t="$$d.rcp-new.$$$$"; cp -f "$$1" "$$t" && mv -f "$$t" "$$d"; }; \
	bakstock() { d="$$1"; b="$$d.rcpbak"; \
	  if [ -f "$$d" ] && [ ! -f "$$b" ]; then \
	    if grep -q "rof2ClientPlus UI override" "$$d" 2>/dev/null; then \
	      echo ">> NOTE: $$d is already our override and has no .rcpbak -> NOT backing up (refusing to clobber the stock original with our own file)"; \
	    else cp -f "$$d" "$$b"; echo ">> Backed up stock $$d -> $$b"; fi; \
	  fi; }; \
	acp $(TARGET) "$(GAME_DIR)/$(NAME).asi"; \
	echo ">> Installed (atomic) $(GAME_DIR)/$(NAME).asi"; \
	mkdir -p "$(GAME_DIR)/uifiles/default"; \
	for f in uifiles/default/*.xml; do [ -e "$$f" ] || continue; d="$(GAME_DIR)/uifiles/default/$${f##*/}"; bakstock "$$d"; acp "$$f" "$$d"; done; \
	echo ">> Installed (atomic) option-window overrides to $(GAME_DIR)/uifiles/default/ (pristine stock saved ONCE as *.rcpbak)"; \
	rm -f "$(GAME_DIR)/uifiles/rcp/EQUI_OptionsWindow.xml" "$(GAME_DIR)/uifiles/rcp/EQUI_AdvancedDisplayOptionsWnd.xml"; \
	echo ">> Removed stale relocated overrides from $(GAME_DIR)/uifiles/rcp/ (now live in uifiles/default/)"; \
	mkdir -p "$(GAME_DIR)/uifiles/rcp/fonts"; \
	for f in uifiles/rcp/fonts/*.spritefont; do [ -e "$$f" ] || continue; acp "$$f" "$(GAME_DIR)/uifiles/rcp/fonts/$${f##*/}"; done; \
	echo ">> Installed (atomic) fonts to $(GAME_DIR)/uifiles/rcp/fonts/"; \
	mkdir -p "$(GAME_DIR)/uifiles/rcp/targetrings"; \
	for f in uifiles/rcp/targetrings/*.tga; do [ -e "$$f" ] || continue; acp "$$f" "$(GAME_DIR)/uifiles/rcp/targetrings/$${f##*/}"; done; \
	echo ">> Installed (atomic) target-ring graphics to $(GAME_DIR)/uifiles/rcp/targetrings/"; \
	mkdir -p "$(GAME_DIR)/uifiles/rcp/spellicons"; \
	for f in uifiles/rcp/spellicons/*.tga; do [ -e "$$f" ] || continue; acp "$$f" "$(GAME_DIR)/uifiles/rcp/spellicons/$${f##*/}"; done; \
	echo ">> Installed (atomic) classic spell-icon sheets to $(GAME_DIR)/uifiles/rcp/spellicons/"; \
	mkdir -p "$(GAME_DIR)/maps"; \
	for f in maps/*.txt; do [ -e "$$f" ] || continue; acp "$$f" "$(GAME_DIR)/maps/$${f##*/}"; done; \
	echo ">> Installed (atomic) classic zone maps to $(GAME_DIR)/maps/ (bazaar/lavastorm/nektulos _classic)"; \
	if [ -f $(HOSTFIX) ]; then acp $(HOSTFIX) "$(GAME_DIR)/eq-window-fix"; echo ">> Installed (atomic) eq-window-fix (windowed-mode watcher)"; fi; \
	for f in launcher/*.sh; do [ -e "$$f" ] || continue; acp "$$f" "$(GAME_DIR)/$${f##*/}"; chmod +x "$(GAME_DIR)/$${f##*/}"; done; \
	echo ">> Installed (atomic) Linux/Wine launch scripts (launcher/*.sh; WINEDLLOVERRIDES + patchme baked in)"

# --- Classic model archives (the model-swap feature's runtime data) ------------------
# The 29 rcp*.s3d classic-model archives (59 MB) are NOT part of `make install`: they are
# regenerated from source .s3d archives by tools/build_models.py, and the client is told to
# load them by a separate edit to $GAME_DIR/Resources/GlobalLoad.txt (tools/patch_globalload.py).
# `make models` rebuilds + verifies all 29 into build/ (no deploy). `make install-models` also
# deploys them into $GAME_DIR (atomic) and patches GlobalLoad.txt. A full/fresh deploy is
# therefore `make install && make install-models`. See docs/MODEL_ASSETS.md and CLAUDE.md.
#
# MODEL_SRC_DIR holds all 29 build inputs (classic global*_chr + Trilogy creature archives +
# gequip.s3d); default is the local backup. Override on a fresh machine:
#   make models MODEL_SRC_DIR=/path/to/model-inputs
MODEL_SRC_DIR ?= /home/joshua/Games/RoF2-modelswap-backup/model-inputs
MODEL_BACKUP  ?= /home/joshua/Games/RoF2-modelswap-backup

models:
	python3 tools/build_models.py --src-dir "$(MODEL_SRC_DIR)"

install-models:
	@if [ -z "$(GAME_DIR)" ]; then \
	  echo "ERROR: GAME_DIR is not set (copy config.mk.example to config.mk)."; exit 1; fi
	@if [ ! -d "$(GAME_DIR)" ]; then echo "ERROR: GAME_DIR '$(GAME_DIR)' does not exist."; exit 1; fi
	python3 tools/build_models.py --src-dir "$(MODEL_SRC_DIR)" --game-dir "$(GAME_DIR)" --install

# --- Distributable folder --------------------------------------------------------------
# `make dist` assembles the payload folder $(PAYLOAD)/ mirroring the client layout: the
# .asi, the 29 rcp*.s3d, Resources/GlobalLoad.txt (stock + the mod's lines), uifiles/,
# maps, launch scripts, and an INSTALL.txt. Hand the folder to a user; copying its
# contents into the RoF2 dir is a complete install. The .s3d are regenerated from
# $(MODEL_SRC_DIR) if present, else taken from build/ or the backup's built-s3d/.
# See tools/build_dist.py and docs/MODEL_ASSETS.md.
DIST ?= dist
PAYLOAD = $(DIST)/rof2ClientPlus
dist: all
	python3 tools/build_dist.py --out "$(PAYLOAD)" --build-dir "$(BUILD)" \
	  --src-dir "$(MODEL_SRC_DIR)" --fallback-s3d "$(MODEL_BACKUP)/built-s3d"

# --- Distributable zip -----------------------------------------------------------------
# `make package` rebuilds the payload and zips its CONTENTS into $(PKG), next to it in
# $(DIST)/ (one file to hand to a user). The zip root IS the client layout (plus
# INSTALL.txt), so extracting it straight into the RoF2 directory is a complete install.
PKG ?= $(DIST)/rof2ClientPlus.zip
package: dist
	@command -v zip >/dev/null || { echo "ERROR: zip required"; exit 1; }
	@rm -f "$(PKG)"   # zip updates archives in place; start clean so no stale entries survive
	cd "$(PAYLOAD)" && zip -r -q "$(abspath $(PKG))" .
	@echo ">> packaged -> $(PKG) ($$(du -h "$(PKG)" | cut -f1))"

clean:
	rm -rf $(BUILD) $(DIST)
