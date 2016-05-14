#ifndef _CUTTER_H_
#define _CUTTER_H_

void zbar_cutter_reset(void);
void zbar_cutter_add_box(qr_point box[4]);
int zbar_cutter_need_cut(zbar_image_t *img);
int zbar_cutter_get_phrase(void);
void zbar_cutter_set_phrase(int phrase);
zbar_image_t* zbar_cutter_do_cut(zbar_image_t *snap, zbar_image_t *full);

#endif


