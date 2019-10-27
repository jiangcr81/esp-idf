#ifndef __HU_PROFILE_H__
#define __HU_PROFILE_H__

#ifndef OK
#define OK   0
#endif

#define DEF_CONF_FILE	"/www/profile_def.conf"
#define TEMP_CONF_FILE	"/www/profile_temp.conf"
#define SYSTEM_CONF		"/www/profile.conf"
#define BIN_BOX_CONF	"/www/profile_box.conf"

#define MAX_LEN			512
#define MAX_KEY_LEN		64
#define MAX_SECTION_LEN	64

int hu_profile_find_section(char *sect, FILE *fp);
int hu_profile_getstr_in(char *sect, char *key, char *val, int size, FILE* fp);
int hu_profile_getstr(char *sect, char *key, char *val, int size, FILE* fp);

int hu_profile_getint(char *sect, char *key, int *val, FILE *fp);
int hu_profile_getlong(char *sect, char *key, long *val, FILE *fp);
int hu_profile_getchar(char *sect, char *key, char *val, FILE *fp);

int hu_profile_setstr(char *sect, char *key, char *val, char *fname);

#endif
