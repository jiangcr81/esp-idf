
//#include "stdafx.h"
#include "config.h"

#if (HU_PROFILE_EN == 1)
#include <stdio.h>
#include "stdlib.h"
#include "string.h"

#include "hu_profile.h"

/*
 * ���ļ����ж�ȡ�����ؼ���
 * ���� 0:�ҵ���; 1:û�ҵ�
 */
int hu_profile_find_section(char *sect, FILE *fp)
{
	char buf[MAX_LEN], sec[MAX_KEY_LEN], *p;
	int  ln;
	
	p = strstr(sect,"\n");
	if (p)
	{
		//���һس����ҵ��س��ͽ���
		*p = 0;
	}

	//�÷�������������
	if (sect[0] != '[')
	{
		sprintf(sec, "[%s]", sect);
		p = sec;
	}
	else
	{
		p = sect;
	}

	//�����ļ����Ķ�дλ��Ϊ�ļ���ͷ
	rewind(fp);

	//�����ؼ��ֳ���
	ln = strlen(p);

	//���ļ����ж�ȡһ������
	while(fgets(buf, sizeof(buf), fp))
	{
		if (buf[0] != '[')
		{
			//һ�����ַ����Ƿ����ţ���������һ��
			continue;
		}
		if (strncmp(buf, p, ln) == 0)
		{
			//�ļ����ݰ��������ؼ��֣�����
			return 0;
		}
	}
	return 1;
}

/*
 * �������л�ȡ��ֵ�Ե�����
 * �������
 * sect: ����
 * key:  ����
 * size: Ԥ�ڵ����ݳ���
 * fp:   �ļ���
 * ����
 * val:  ��ȡ���ļ�ֵ������
 */
int hu_profile_getstr_in(char *sect, char *key, char *val, int size, FILE* fp)
{
	int  key_len, bl;
	char *buf, *p_buf=NULL, *p, *p0;
	char b[MAX_LEN]; /* a line read from profile */
	
	if (val==NULL || fp==NULL || key==NULL)
	{
		return -1;
	}
	
	if (sect)
	{
		if (hu_profile_find_section(sect, fp) != OK)
		{
			// section not found
			return -1;
		}
	}
	else
	{
		//�����ļ����Ķ�дλ��Ϊ�ļ���ͷ
		rewind(fp);
	}
	//��ʱ�ļ���дָ�����ҵ��������ؼ��ֺ�һ�л��ļ���ͷ
	
	if (size > MAX_LEN)
	{
		bl = size + MAX_KEY_LEN;
		p_buf = buf = (char *)malloc(bl);
		if (!buf)
		{
			return -1;
		}
	}
	else
	{
		bl = sizeof(b);
		buf = b;
	}
	
	key_len = strlen(key);
	//���ļ����ж�ȡһ������
	while(fgets(buf, bl, fp))
	{
		if (buf[0]=='[')
		{
			//�ҵ�����һ�������ؼ��֣�����
			break; // end of section
		}
		p = buf;
		while(*p == ' ')
		{
			//�ƶ���һ����Ϊ�ո������
			p++;
		}

		if (strncmp(p, key, key_len)==0)
		{
			//�ҵ��˹ؼ�������
			p0 = p + key_len;
			while (*p0 == ' ')
			{
				p0++;
			}
			if (*p0 != '=')
			{
				continue;
			}
			p0++;
			/*	Won't skip the space character at the begin of value...	 
			while (*p0==' ')
            p0++;
			*/
			p = buf+strlen(buf)-1;
			while((*p == 0x0d) || (*p == 0x0a))
			{
				p--;
			}
			*(p+1)=0;
			if (strlen(p0) > (unsigned int)size)
			{
				*(p0+size-1)=0;
			}
			//����ȡ���ļ�ֵ�����ݿ�����valָ��������
			strcpy(val, p0);
			if(p_buf)
			{
				//�ͷ��ڴ�
				free(p_buf);
			}
			return OK;
		}
	}
	
	if(p_buf)
	{
		free(p_buf);
	}
	return -1;
}

int hu_profile_getstr(char *sect, char *key, char *val, int size, FILE* fp)
{
	int ret = 0;
	FILE *fp_def = NULL;

	if (hu_profile_getstr_in(sect, key, val, size, fp))
	{
		fp_def = fopen(DEF_CONF_FILE, "r+");
		if (fp_def)
		{
			ret = hu_profile_getstr_in(sect, key, val, size, fp_def);
			fclose(fp_def);
		}
		else
		{
			ret = -1;
		}
	}
	return ret;
}

int hu_profile_getint(char *sect, char *key, int *val, FILE *fp)
{
	char buf[MAX_KEY_LEN];
	int ret;
	
	ret = hu_profile_getstr(sect,key,buf,sizeof(buf),fp);
	if (OK == ret)
		*val = atoi(buf);
	
	return ret;
}

int hu_profile_getlong(char *sect, char *key, long *val, FILE *fp)
{
	char buf[MAX_KEY_LEN];
	int ret;
	
	ret = hu_profile_getstr(sect,key,buf,sizeof(buf),fp);
	if (OK == ret)
		*val = atol(buf);
	
	return ret;
}

int hu_profile_getchar(char *sect, char *key, char *val, FILE *fp)
{
	char buf[MAX_KEY_LEN];
	int ret;
	
	ret = hu_profile_getstr(sect,key,buf,sizeof(buf),fp);
	if (OK == ret)
	{
		memcpy(val, buf, sizeof(buf));
	//	*val = *buf;
	}
	return ret;
}

/*
 * �������ļ�д���顢��ֵ������
 * ��������
 * sect: ��������Ϊ����ӿ�ͷ�����ҵ���1�������޸�������
 * key:  ����
 * val:  ֵ����
 * fname:�����ļ���
 */
int hu_profile_setstr(char *sect, char *key, char *val, char *fname)
{
	char buf[MAX_LEN];  /* a line read from ini file */
	char s[MAX_KEY_LEN];
	int  len, j, i, ret=0;
	FILE *fnow, *fnew;

	fnow = fopen(fname, "r");
	if (fnow==NULL)
	{  // file not exists, create one
		fnow = fopen(fname, "w");
		if (fnow == NULL)
		{
			ret = -1;
			goto unlock_exit;
		}
		fclose(fnow);
		fnow=fopen(fname, "r");
	}
	
	// skip the un-changed items to enhance the performance
	if (!hu_profile_getstr(sect, key, buf, MAX_LEN, fnow))
	{
		if (0 == strcmp(buf, val))	// Same as original, not to set again...
		{
			fclose(fnow);
			ret = 0;
			goto unlock_exit;
		}
	}
	fseek(fnow, 0, SEEK_SET);
	
	// create a temp file
	fnew = fopen(TEMP_CONF_FILE, "r");
	if (fnew==NULL)
	{
		// file not exists, create one
		fnew = fopen(TEMP_CONF_FILE,"w");
		if (fnew == NULL)
		{
			ret = -1;
			goto unlock_exit;
		}
		fclose(fnew);
	}
	fnew = fopen(TEMP_CONF_FILE, "w");
	if (fnew==NULL)
	{
		fclose(fnew);
		ret = -1;
        goto unlock_exit;
	}
	
	if(sect == NULL)
	{
		j = 0;
		len = strlen(key);
		//���ļ����ж�ȡһ������
		while (fgets(buf, sizeof(buf), fnow))
		{
			if ((buf[len] == '=' || buf[len] == ' ') &&
				(strncmp(buf,key,len)==0))
			{
				j = 1;
				fprintf(fnew,"%s=%s\n",key,val);
				len=strlen(buf);
				if(buf[len-1]!='\n') {
					i=fgetc(fnow);
					while(i!='\n' && i!=EOF)
						i=fgetc(fnow);
				}
			}
			else {
				ret = fputs(buf,fnew);
				if (ret < 0)
				{
					goto closeall_removenew;
				}
			}
		}
		if(!j)
			goto append;
		goto exit0;
	}
	
	if (sect[0] == '[')
		strcpy(s, sect);
	else
		sprintf(s, "[%s]", sect);
	len = strlen(s);
	
	//���ļ����ж�ȡһ������
	while(fgets(buf, sizeof(buf), fnow))
	{
		if (buf[0] != '[')
		{
			//��ȡ�����ݲ�������
			ret = fputs(buf, fnew);
			//��һָ�����ַ���д���ļ���
			if (ret < 0)
			{
				goto closeall_removenew;
			}
			continue;
		}
		if (strncmp(buf, s, len) == 0)
		{
			//�ҵ�������
			ret = fputs(buf, fnew);
			if (ret < 0)
			{
				goto closeall_removenew;
			}
			len = strlen(key);
			//���ļ����ж�ȡһ������
			while(fgets(buf, sizeof(buf), fnow))
			{
				if (buf[0] == '[')
				{
					//�ҵ�����һ������������ֵ�Դ�����һ������֮��
					fprintf(fnew, "%s=%s\n", key, val);
					ret = fputs(buf, fnew);
					if (ret < 0)
					{
						goto closeall_removenew;
					}
save_rest:
					while(fgets(buf,sizeof(buf),fnow))
					{
						ret = fputs(buf,fnew);
						if (ret < 0)
						{
							goto closeall_removenew;
						}
					}
					fclose(fnew);
					fclose(fnow);
					//ɾ�����ļ������������ļ�
					remove(SYSTEM_CONF);
					rename(TEMP_CONF_FILE, SYSTEM_CONF);
				//	sprintf(buf,"/bin/cp -f %s %s > /dev/null 2> /dev/null",TEMP_CONF_FILE,fname);
				//	system(buf);
				//	sync();
				//	remove(TEMP_CONF_FILE);
					ret = OK;
					goto unlock_exit;
				}
				j=0;
				while(buf[j]==' ')
					j++;
				if (strncmp(buf+j, key, len)==0)
				{
					//�ҵ��˼�ֵ������
					//find '='
					j += len;
					while(buf[j] == ' ')
						j++;
					if(buf[j] != '=')
						goto not_match;
					fprintf(fnew, "%s=%s\n", key, val);
					len=strlen(buf);
					if(buf[len-1] != '\n')
					{
						j = fgetc(fnow);
						while(j!='\n' && j!=EOF)
							j=fgetc(fnow);
					}
					goto save_rest;
				}
				else { // section not match
not_match:
					ret = fputs(buf,fnew);
					if (ret < 0)
					{
						goto closeall_removenew;
					} 
				}
			}
			goto append;
		} 
		else
		{
			ret = fputs(buf,fnew);
			if (ret < 0)
			{
				goto closeall_removenew;
			}
		}
	} 
	//section not found append the section
	if (sect[0] == '[')
		fprintf(fnew,"%s\n",sect);
	else
		fprintf(fnew,"[%s]\n",sect);
append:
	fprintf(fnew,"%s=%s\n", key, val);
exit0:
	fclose(fnow);
	fclose(fnew);
	//ɾ�����ļ������������ļ�
	remove(SYSTEM_CONF);
	rename(TEMP_CONF_FILE, SYSTEM_CONF);
//	sprintf(buf,"/bin/cp -f %s %s >/dev/null 2> /dev/null",TEMP_CONF_FILE,fname);
//	system(buf);
//	sync();
//	remove(TEMP_CONF_FILE);
	ret=OK;
	goto unlock_exit;
closeall_removenew:
	fclose(fnow);
	fclose(fnew);
	remove(TEMP_CONF_FILE);
	ret = -1;
unlock_exit:

	return ret;
}

#endif
