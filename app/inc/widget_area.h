/*
************************************************************************************************************************
*           Display areas given to plugin widgets.
*
*           mod-ui decides, for each area of the control screens, whether it shows the default drawing of the
*           actuators below it or a widget drawn by a plugin. A widget area belongs to a page and a subpage (or to
*           every subpage of a page) and covers whatever the firmware would draw there. The actuators below keep
*           working as usual.
*
*           The two displays are one surface to mod-ui, side by side: x 0 to 127 is the left display, 128 to 255
*           the right one. An area lies on one display, it cannot cross from one to the other.
*
*           Only the areas of the page in view hold pixels: the plugin redraws an area every time it comes into
*           view, so nothing is kept for the pages that are not shown. Each visible area keeps its own backing
*           buffer, in the same layout the plugin sends (1bpp, rows top to bottom, most significant bit leftmost).
*
*           The areas are painted over a display each time it is sent to the panel, so whatever the firmware
*           draws below a widget never shows. Not while a tool or the builder holds the display: the pixels are
*           kept and come back with the control screen.
*
*           The Duo has a single page of actuators and no subpages: mod-ui places every area on page 0.
************************************************************************************************************************
*/

#ifndef WIDGET_AREA_H
#define WIDGET_AREA_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <stdint.h>
#include "glcd.h"
#include "glcd_clip.h"


/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/

// an area added with this subpage is shown on every subpage of its page
#define WIDGET_AREA_ALL_SUBPAGES    -1

// removes every area
#define WIDGET_AREA_ALL             -1

// the surface mod-ui places the areas on: both displays, side by side
#define WIDGET_AREA_SURFACE_WIDTH   (DISPLAY_WIDTH * 2)


/*
************************************************************************************************************************
*           FUNCTION PROTOTYPES
************************************************************************************************************************
*/

// The functions that change which areas show return the displays to print again, one bit each (bit 0 the
// left display): an area that comes or goes leaves the firmware's own drawing to be put back below it.

// Adds an area, in surface coordinates, replacing any area with the same id; the displays to print again go
// in `displays`. Returns 0 when the rectangle is off the surface, crosses from one display to the other, or
// memory is short.
uint8_t WA_add(int16_t area_id, int8_t page, int8_t subpage, const glcd_rect_t *rect, uint8_t *displays);

// Removes an area, or all of them with WIDGET_AREA_ALL.
uint8_t WA_remove(int16_t area_id);

// The page and subpage now in view. Areas coming into view start blank, areas leaving it drop their pixels.
uint8_t WA_set_screen(int8_t page, int8_t subpage);

// Replaces the pixels of a rectangle of a visible area, in area coordinates, clipped to the area.
// `rows` is the hex string of the protocol, `height` rows of (`width` + 7) / 8 bytes; NULL clears the rectangle.
// Returns 0 when the area is not in view or `rows` is too short.
uint8_t WA_draw(int16_t area_id, int16_t x, int16_t y, int16_t width, int16_t height, const char *rows);

// Paints the areas in view over a display, from its driver just before the buffer is sent to the panel.
void WA_paint(glcd_t *display);


/*
************************************************************************************************************************
*           END HEADER
************************************************************************************************************************
*/

#endif
