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

# Audio (D-020): defining AUDIOFILES/SOUNDBANK before the include makes
# snes_rules add soundbank.asm to SFILES and run smconv over the .it files.
# sfx.it MUST stay first (spcLoadEffect indices; INV-AUD-001 checks on it).
AUDIOFILES := $(GEN)/audio/sfx.it $(GEN)/audio/track01.it
export SOUNDBANK := $(GEN)/audio/soundbank

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
FLAVORPS = src/main src/core/vblank src/core/dbgcmd src/core/rng src/game/room \
           src/game/player src/game/entity src/game/portal src/game/chamber \
           src/audio/sound

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

# Phase 1 art is generated SNES-native by deterministic scripts (prophet's
# pattern — no image tools in the build loop). Grouped targets (&:) need
# make >= 4.3 (MSYS2 ships 4.4.1).
GENTILES := $(GEN)/tiles.chr $(GEN)/tiles.pal $(GEN)/tiles.map $(GEN)/tiles.png

$(GENTILES) &: tools/art/mktiles.py tools/tilesdef.py
	$(PYTHON) tools/art/mktiles.py

GENOBJ := $(GEN)/obj.chr $(GEN)/obj.pal $(GEN)/obj.png

$(GENOBJ) &: tools/art/mkobj.py tools/art/mktiles.py
	$(PYTHON) tools/art/mkobj.py

# Rooms: ASCII metatile grid -> Tiled .tmj (mkmaps) -> tmx2snes (D-011) ->
# baked map/attr binaries + checksum (roomglue, D-012/INV-ENG-005).
# Add each authored room to ROOMS (mkmaps/roomglue glob on their own).
ROOMS := room01 room02
ROOMTMJ := $(foreach r,$(ROOMS),$(GEN)/maps/$(r).tmj)
ROOMM16 := $(foreach r,$(ROOMS),$(GEN)/maps/$(r).m16)

$(ROOMTMJ) &: tools/mkmaps.py tools/tilesdef.py \
		$(foreach r,$(ROOMS),assets/maps/$(r).txt)
	$(PYTHON) tools/mkmaps.py

$(GEN)/maps/%.m16 $(GEN)/maps/%.b16 $(GEN)/maps/%.t16 &: \
		$(GEN)/maps/%.tmj $(GEN)/tiles.map
	$(TMXCONV) $(GEN)/maps/$*.tmj $(GEN)/tiles.map

GENROOMS := $(GEN)/rooms.asm $(GEN)/rooms.h $(GEN)/roomtabs.h
$(GENROOMS) &: tools/roomglue.py $(ROOMM16)
	$(PYTHON) tools/roomglue.py

# Phase 3: trig LUT + the Mode 7 chamber (its own generator — tmx2snes
# doesn't speak Mode 7's byte-planar world; DECISIONS Phase 3 checkpoint)
GENLUT := $(GEN)/lut.asm
$(GENLUT): tools/mklut.py
	$(PYTHON) tools/mklut.py

GENCHAM := $(GEN)/chamber.asm $(GEN)/chamber.h
$(GENCHAM) &: tools/art/mkchamber.py tools/art/mktiles.py assets/maps/chamber01.txt
	$(PYTHON) tools/art/mkchamber.py

# Audio pipeline (D-020): mkit.py synthesizes the .it modules; snes_rules'
# own rule runs smconv over them ($(SOUNDBANK).asm: $(AUDIOFILES)); the
# INV-AUD-001 stamp asserts the ARAM budget on smconv's generated sizes.
SMCONVFLAGS := -s -o $(SOUNDBANK) -V -b 5 -f

$(AUDIOFILES) &: tools/audio/mkit.py
	@mkdir -p $(GEN)/audio
	$(PYTHON) tools/audio/mkit.py

# smconv's one rule names only the .asm; .h/.bnk appear with it
$(SOUNDBANK).h $(SOUNDBANK).bnk: $(SOUNDBANK).asm ;

$(GEN)/audio/.aramok: tools/audio/checkbank.py $(SOUNDBANK).asm
	$(PYTHON) tools/audio/checkbank.py $(SOUNDBANK).h
	@touch $@

$(ROMNAME).sfc: $(GEN)/audio/.aramok
src/audio/sound.ps: $(SOUNDBANK).h

# Title/end-card font: pvsneslib's example font via gfx4snes (Phase 0's
# rule, restored for D-019)
$(GEN)/pvsneslibfont.pic $(GEN)/pvsneslibfont.pal &: assets/fonts/pvsneslibfont.png
	@mkdir -p $(GEN)
	@cp $< $(GEN)/pvsneslibfont.png
	$(GFXCONV) -s 8 -o 16 -u 16 -p -e 0 -i $(GEN)/pvsneslibfont.png

bitmaps: $(GENTILES) $(GENOBJ) $(GENROOMS) $(GENLUT) $(GENCHAM) $(GEN)/pvsneslibfont.pic

# rooms.obj: snes_rules collects sources by wildcard at parse time — on a
# clean tree the generated rooms.asm doesn't exist yet, so append its object
# (guarded against double-link) AND pin it as an .sfc prerequisite (the
# prereq list was captured when snes_rules was parsed). Prophet's pattern.
ROOMSOBJ := $(GEN)/rooms.obj
ifeq ($(filter $(ROOMSOBJ),$(OFILES)),)
OFILES += $(ROOMSOBJ)
endif
$(GEN)/rooms.obj: $(GEN)/rooms.asm
$(ROMNAME).sfc: $(ROOMSOBJ)

LUTOBJ := $(GEN)/lut.obj
ifeq ($(filter $(LUTOBJ),$(OFILES)),)
OFILES += $(LUTOBJ)
endif
$(GEN)/lut.obj: $(GEN)/lut.asm
$(ROMNAME).sfc: $(LUTOBJ)

CHAMOBJ := $(GEN)/chamber.obj
ifeq ($(filter $(CHAMOBJ),$(OFILES)),)
OFILES += $(CHAMOBJ)
endif
$(GEN)/chamber.obj: $(GEN)/chamber.asm
$(ROMNAME).sfc: $(CHAMOBJ)

# incbin consumers must see fresh binaries/headers
data.obj: $(GENTILES) $(GENOBJ) $(GEN)/pvsneslibfont.pic $(GEN)/pvsneslibfont.pal
src/game/room.ps: $(GEN)/rooms.h
src/game/chamber.ps: $(GEN)/chamber.h
src/main.ps src/game/entity.ps: $(GEN)/roomtabs.h

# snes_rules tracks NO header dependencies — a tuning.h edit left a stale
# player.obj in the ROM once (identical goldens exposed it). Coarse but
# correct: any project header change rebuilds every C translation unit.
HDRS := $(wildcard src/core/*.h src/game/*.h src/ui/*.h src/audio/*.h)
$(addsuffix .ps,$(FLAVORPS)): $(HDRS)

clean: cleanBuildRes cleanRom cleanGfx
	@rm -rf $(GEN) build artifacts
