#ifndef SPRITE_H
#define SPRITE_H

class pic8;

unsigned char* sprite_data_8(pic8* pmask, unsigned char color, unsigned short* sprite_length);

void make_sprite(pic8* ppic, int index);
void make_sprite(pic8* ppic);

#endif
