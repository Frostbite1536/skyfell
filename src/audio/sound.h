/* RIFT audio (D-020): SNESMOD soundbank glue. One pipeline — smconv over
 * mkit.py's generated .it modules; SFX are the first module's samples,
 * played through spcEffect on the driver's effect voice (channel 8; music
 * is authored on channels 1-6 so two DSP voices stay clear, GDD 6+2). */
#ifndef AUDIO_SOUND_H
#define AUDIO_SOUND_H

#include <snes.h>

/* SFX ids = sample order in mkit.py's sfx.it — the two must stay in step */
#define SFX_JUMP 0
#define SFX_FIRE 1
#define SFX_OPEN 2
#define SFX_TELEPORT 3
#define SFX_LAND 4
#define SFX_DEATH 5

void audio_boot(void);  /* spcBoot — first thing in main(), before NMI */
void audio_start(void); /* bank + module + effects + play (slow, boot only) */
void audio_frame(void); /* spcProcess — once per main-loop frame */
void sfx_play(u8 id);
void sfx_quiet(u8 frames); /* suppress SFX briefly (room loads re-ground
                              the player = phantom LAND thud, D-020) */

#endif
