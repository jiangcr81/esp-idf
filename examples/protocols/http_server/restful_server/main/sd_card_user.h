#ifndef __SD_CARD_USER_H__
#define	__SD_CARD_USER_H__

#define		SD_3V3_EN		26
#define		SD_DET			27


#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SDFILE_BUFSIZE (10240)


void sd_card_on(void);
void sd_card_off(void);

esp_err_t sd_rdwt_file(char * path_filename);

#endif
