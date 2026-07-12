;== RIFT: THE SKYFELL ENGINE — LoROM + FastROM cartridge header (D-006) ==

.MEMORYMAP                      ; Begin describing the system architecture.
  SLOTSIZE $8000                ; The slot is $8000 bytes in size.
  DEFAULTSLOT 0
  SLOT 0 $8000                  ; ROM slot.
  SLOT 1 $0 $2000               ; WRAM low (mirrored $7E:0000-$1FFF).
  SLOT 2 $2000 $E000            ; WRAM upper bank $7E (DBR=$7E at run time).
  SLOT 3 $0 $10000
.ENDME

.ROMBANKSIZE $8000              ; 32 KB per LoROM bank.
.ROMBANKS 16                    ; 16 x 32KB = 512KB (4 Mbit) per D-006.

.SNESHEADER
  ID "SNES"

  NAME "RIFT SKYFELL ENGINE  "  ; Program title, exactly 21 bytes.
  ;    "123456789012345678901"

  FASTROM                       ; 3.58MHz (D-006); lib objs = LoROM_FastROM.
  LOROM

  CARTRIDGETYPE $02             ; ROM + SRAM.
  ROMSIZE $09                   ; 4 Megabits (512KB).
  SRAMSIZE $03                  ; 64 kilobits = 8KB SRAM (D-006).
  COUNTRY $01                   ; U.S. / NTSC (D-008).
  LICENSEECODE $00
  VERSION $00
.ENDSNES

.SNESNATIVEVECTOR               ; Native Mode interrupt vector table.
  COP EmptyHandler
  BRK EmptyHandler
  ABORT EmptyHandler
  NMI VBlank                    ; pvsneslib vblank handler.
  IRQ EmptyHandler
.ENDNATIVEVECTOR

.SNESEMUVECTOR                  ; Emulation Mode interrupt vector table.
  COP EmptyHandler
  ABORT EmptyHandler
  NMI EmptyHandler
  RESET tcc__start              ; Execution starts in pvsneslib crt0.
  IRQBRK EmptyHandler
.ENDEMUVECTOR
