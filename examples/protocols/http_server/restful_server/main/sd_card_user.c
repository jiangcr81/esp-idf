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
static const char *SDUSER_TAG = "sd_card_user";

char sd_file_content[SDFILE_BUFSIZE];

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

esp_err_t sd_rdwt_file(char * path_filename)
{
	char filepath[FILE_PATH_MAX];
	int i;
	char buf_temp,bufh,bufl;

	strlcpy(filepath, path_filename, sizeof(filepath));
	
	int fd = open(filepath, O_CREAT|O_RDWR, 0);
    if (fd == -1) {
        ESP_LOGE(SDUSER_TAG, "Failed to open file : %s", filepath);
        return ESP_FAIL;
    }

    char *chunk = sd_file_content;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SDFILE_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(SDUSER_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            //print file content
            ESP_LOG_STR((uint8 *)chunk, read_bytes);
			for(i=0; i<read_bytes; i++)
			{
				buf_temp = chunk[i];
				bufh = (buf_temp>>4)&0x0F;
				bufl = buf_temp&0x0F;
				buf_temp = (bufl<<4)|bufh;
				chunk[i] = buf_temp;
			}
			ESP_LOG_STR((uint8 *)chunk, read_bytes);
        }
		lseek(fd, 0, SEEK_SET);
		write(fd, chunk, read_bytes);
    } while (read_bytes > 0);

	
    /* Close file after sending complete */
    close(fd);
	return ESP_OK;
}

#endif
