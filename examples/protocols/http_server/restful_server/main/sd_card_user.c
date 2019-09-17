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

#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
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

uint8 sd_card_det(void)
{
	uint8 u8_ret = 0;
	gpio_pad_select_gpio(SD_DET);

    gpio_set_direction(SD_DET, GPIO_MODE_INPUT);
	
	u8_ret = gpio_get_level(SD_DET);

	return u8_ret;
}

#endif
