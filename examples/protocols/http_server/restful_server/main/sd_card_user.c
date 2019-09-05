/*
TF card interface

+3.3V EN | IO26
---------
 DATA2 |1| IO12
 DATA3 |2| IO13
   CMD |3| IO15
   VDD |4| +3.3V
   CLK |5| IO14
   VSS |6| GND
 DATA0 |7| IO2
 DATA1 |8| IO4
SD_DET |9| IO27
---------
*/

#include "config.h"

#if (SD_CARD_EN == 1)

void sd_card_on(void)
{
	gpio_pad_select_gpio(SD_3V3_EN);

    gpio_set_direction(SD_3V3_EN, GPIO_MODE_OUTPUT);
	
	gpio_set_level(SD_3V3_EN, 1);
}

void sd_card_off(void)
{
	gpio_pad_select_gpio(SD_3V3_EN);

    gpio_set_direction(SD_3V3_EN, GPIO_MODE_OUTPUT);
	
	gpio_set_level(SD_3V3_EN, 0);
}

#endif