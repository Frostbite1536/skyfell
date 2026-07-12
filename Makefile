# RIFT: THE SKYFELL ENGINE — build contract (CLAUDE.md): make / make test / make run / make clean
#
# Requires PVSNESLIB_HOME with FORWARD slashes (e.g. C:/Users/LCM/snesdev/pvsneslib).
# On this machine, make lives in MSYS2 — invoke from PowerShell/cmd as:
#   C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/LCM/Github/skyfell && make"
# (Git Bash cannot pass env vars to MSYS2 binaries — sibling prophet's docs/CONTINUATION.md.)

ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error PVSNESLIB_HOME not set. Use a forward-slash path, no backslashes)
endif

# LoROM + FastROM (D-006): selects pvsneslib's LoROM_FastROM lib objects.
export FASTROM := 1

# TEST=1 compiles the debug-block writes + test hooks in (INV-TEST-001, D-010).
ifeq ($(TEST),1)
export CFLAGS += -DTEST_BUILD
endif

GEN := src/data/generated

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

# snes_rules' Windows branch (ifeq ($(OS),Windows_NT)) rebuilds this path by
# string surgery that yields "C::\..." for a C:/-style PVSNESLIB_HOME — ls
# finds nothing, no lib objects reach the linkfile, and the link dies on the
# first crt0 symbol (EmptyHandler). Whether the branch even runs depends on
# the parent shell (PowerShell passes OS=Windows_NT into MSYS2; Git Bash
# drops it). wlalink accepts C:/-style forward-slash paths directly; recipes
# expand at run time, so this post-include reassignment wins on every parent
# shell. (Proven in sibling prophet.)
LIBDIRSOBJSW := $(LIBDIRSOBJS)

export ROMNAME := skyfell
MESEN  ?= C:/Users/LCM/snesdev/mesence/Mesen.exe
PYTHON ?= python

.PHONY: all bitmaps test run clean flavorcheck

# The TEST define changes codegen; a stale object from the other flavor must
# never link into this one (a TEST-flavored "release" ROM shipped that way
# once in prophet). build/.flavor stamps the last build's flavor and forces a
# clean when it changes — purging the whole compile chain (.ps/.asp/.asm),
# not just objects, because explicit-prerequisite .ps files survive make's
# auto-intermediate deletion and would carry their flavor across the swap.
FLAVOR := $(if $(filter 1,$(TEST)),test,release)
FLAVORPS = src/main src/core/vblank

flavorcheck:
	@mkdir -p build
	@if [ "`cat build/.flavor 2>/dev/null`" != "$(FLAVOR)" ]; then \
		$(MAKE) cleanBuildRes cleanRom >/dev/null 2>&1 || true; \
		rm -f $(addsuffix .ps,$(FLAVORPS)) $(addsuffix .asp,$(FLAVORPS)) \
		      $(addsuffix .asm,$(FLAVORPS)); \
		echo $(FLAVOR) > build/.flavor; \
	fi

all: flavorcheck bitmaps $(ROMNAME).sfc
	@mkdir -p build
	@cp $(ROMNAME).sfc build/$(ROMNAME)$(if $(filter 1,$(TEST)),-test,).sfc
	@echo ROM ready: build/$(ROMNAME)$(if $(filter 1,$(TEST)),-test,).sfc

test:
	@$(MAKE) TEST=1 all
	$(PYTHON) tools/run_tests.py

run: all
	"$(MESEN)" build/$(ROMNAME).sfc &

# Phase 0 font: pvsneslib's example font converted by gfx4snes — a placeholder
# until skyfell's own font lands (Phase 1 HUD). Converted beside the copied
# PNG so outputs land in the gitignored $(GEN). Grouped target (&:) needs
# make >= 4.3 (MSYS2 ships 4.4.1).
$(GEN)/pvsneslibfont.pic $(GEN)/pvsneslibfont.pal &: assets/fonts/pvsneslibfont.png
	@mkdir -p $(GEN)
	@cp $< $(GEN)/pvsneslibfont.png
	$(GFXCONV) -s 8 -o 16 -u 16 -p -e 0 -i $(GEN)/pvsneslibfont.png

bitmaps: $(GEN)/pvsneslibfont.pic

# incbin consumer must see fresh binaries
data.obj: $(GEN)/pvsneslibfont.pic $(GEN)/pvsneslibfont.pal

clean: cleanBuildRes cleanRom cleanGfx
	@rm -rf $(GEN) build artifacts
