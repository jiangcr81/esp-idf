#ifndef __SD_CARD_USER_H__
#define	__SD_CARD_USER_H__

#define		SD_3V3_EN		26
#define		SD_DET			27

void sd_card_on(void);
void sd_card_off(void);
uint8 sd_card_det(void);

#endif
