/* room.h — metatile room: collision/material queries, camera with edge
 * clamp, and BG1 column/row streaming through the vblank queue (D-012).
 *
 * The BG1 tilemap is a 64x32-tile window (SC_64x32) over a 128x64-tile room;
 * room_cam_set streams the entering columns/rows as the window slides (live,
 * queue-disciplined), room_cam_warp redraws the whole window under
 * force-blank (door transitions, test warps). */
#ifndef SKYFELL_GAME_ROOM_H
#define SKYFELL_GAME_ROOM_H

#include <snes.h>

/* attribute word (D-012): low byte collision, high byte material */
#define ATTR_COL(a) ((u8)(a))
#define ATTR_MAT(a) ((u8)((a) >> 8))
#define COL_EMPTY 0
#define COL_SOLID 1
#define MAT_PLAIN 0
#define MAT_BRASS 1

void room_load(u8 id, u16 camx, u16 camy); /* force-blank full setup */
void room_cam_warp(u16 x, u16 y);          /* force-blank camera jump */
void room_cam_set(u16 x, u16 y);           /* live move: streams seams */

u16 room_attr(u16 tx, u16 ty); /* 8x8-tile attribute; out of bounds = solid */
u16 room_cam_x(void);
u16 room_cam_y(void);
u16 room_w_px(void);
u16 room_h_px(void);

#endif
