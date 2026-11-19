#ifndef BRAMBLE_FONT_H
#define BRAMBLE_FONT_H

#define FONT_W 5
#define FONT_H 7
#define FONT_ADV 6

/* Returns 7 row masks for c (bit 4 = left pixel). Lowercase maps to uppercase;
 * unknown characters draw as a box. */
const unsigned char *font_glyph(int c);

#endif
