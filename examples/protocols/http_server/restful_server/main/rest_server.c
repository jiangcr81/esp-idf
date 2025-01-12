/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "config.h"

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#define		PARAM_CNT_MAX		32
static char *http_cgi_params[PARAM_CNT_MAX];
static char *http_cgi_param_vals[PARAM_CNT_MAX];

static int
parse_uri_parameters(char *params)
{
  char *pair;
  char *equals;
  int loop;

  /* If we have no parameters at all, return immediately. */
  if (!params || (params[0] == '\0')) {
    return (0);
  }

  /* Get a pointer to our first parameter */
  pair = params;

  /* Parse up to LWIP_HTTPD_MAX_CGI_PARAMETERS from the passed string and ignore the
   * remainder (if any) */
  for (loop = 0; (loop < PARAM_CNT_MAX) && pair; loop++) {

    /* Save the name of the parameter */
    http_cgi_params[loop] = pair;

    /* Remember the start of this name=value pair */
    equals = pair;

    /* Find the start of the next name=value pair and replace the delimiter
     * with a 0 to terminate the previous pair string. */
    pair = strchr(pair, '&');
    if (pair) {
      *pair = '\0';
      pair++;
    } else {
      /* We didn't find a new parameter so find the end of the URI and
       * replace the space with a '\0' */
      pair = strchr(equals, ' ');
      if (pair) {
        *pair = '\0';
      }

      /* Revert to NULL so that we exit the loop as expected. */
      pair = NULL;
    }

    /* Now find the '=' in the previous pair, replace it with '\0' and save
     * the parameter value string. */
    equals = strchr(equals, '=');
    if (equals) {
      *equals = '\0';
      http_cgi_param_vals[loop] = equals + 1;
    } else {
      http_cgi_param_vals[loop] = NULL;
    }
  }

  return loop;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
	ESP_LOGE(REST_TAG, "filepath : %s", filepath);
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

#if 0
/* Simple handler for light brightness control */
static esp_err_t light_brightness_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int red = cJSON_GetObjectItem(root, "red")->valueint;
    int green = cJSON_GetObjectItem(root, "green")->valueint;
    int blue = cJSON_GetObjectItem(root, "blue")->valueint;
    ESP_LOGI(REST_TAG, "Light control: red = %d, green = %d, blue = %d", red, green, blue);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data */
static esp_err_t temperature_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "raw", esp_random() % 20);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}
#endif

/* Simple handler for getting weight sensor data */
static esp_err_t mat_get_weight_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, idh=0, idl=0;
	uint32 u32_mat_id=0;
	uint32 adc_show[4];
	char *params = NULL;
	
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
//	ESP_LOGI(REST_TAG, "uri[%s],\r\n", req->uri);
	params = (char *)strchr(req->uri, '?');
	if (params != NULL) {
		/* URI contains parameters. NULL-terminate the base URI */
		*params = '\0';
		params++;
    }

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

//	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(params);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
			idh = (u32_mat_id>>8)&0xFF;
			idl = u32_mat_id&0xFF;
		}
	}
	ESP_LOGI(REST_TAG, "mat_id=%d, idh=%d, idl=%d", u32_mat_id, idh, idl);

	adc_show[0] = api_get_adc_raw((((uint16)idh<<8)|idl), 0);
	adc_show[1] = api_get_adc_raw((((uint16)idh<<8)|idl), 1);
	adc_show[2] = api_get_adc_raw((((uint16)idh<<8)|idl), 2);
	adc_show[3] = api_get_adc_raw((((uint16)idh<<8)|idl), 3);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cubby1", adc_show[0]);
    cJSON_AddNumberToObject(root, "cubby2", adc_show[1]);
    cJSON_AddNumberToObject(root, "cubby3", adc_show[2]);
    cJSON_AddNumberToObject(root, "cubby4", adc_show[3]);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting weight sensor data */
static esp_err_t box_get_weight_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0;
	uint32 u32_box_id=0;
	uint32 cup_index = 0;
	char *params = NULL;
//	char buf2[100];
	uint32 adc_raw,adc_taring,weight,quantity;
//	memset(buf2, 0, 100);
	
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
//	ESP_LOGI(REST_TAG, "uri[%s],\r\n", req->uri);
	params = (char *)strchr(req->uri, '?');
	if (params != NULL) {
		/* URI contains parameters. NULL-terminate the base URI */
		*params = '\0';
		params++;
    }

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

//	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(params);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"box_id", 6) == 0)
		{
			u32_box_id = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"cup_index", 9) == 0)
		{
			cup_index = atoi(http_cgi_param_vals[i]);
		}
	}
//	api_get_box_pn(u32_box_id, cup_index, buf2);
//	wperadc = api_get_box_wperadc(u32_box_id, cup_index);
	adc_raw = api_get_box_adc_raw(u32_box_id, cup_index);
	adc_taring = api_get_box_adc_taring(u32_box_id, cup_index);

	weight = api_get_box_weight(u32_box_id, cup_index);
	quantity = api_get_box_qty(u32_box_id, cup_index);
//	ESP_LOGI(REST_TAG, "cup_index=%d, weight=%d, quantity=%d", cup_index, weight, quantity);

	httpd_resp_set_type(req, "application/json");
	cJSON *root = cJSON_CreateObject();
//	cJSON_AddStringToObject(root, "PartNumber", buf2);
//	cJSON_AddNumberToObject(root, "adc_raw", adc_raw);
//	cJSON_AddNumberToObject(root, "adc_taring", adc_taring);
//	cJSON_AddNumberToObject(root, "adc_delta", abs(adc_raw-adc_taring));
	cJSON_AddNumberToObject(root, "Weight", weight);
	cJSON_AddNumberToObject(root, "Quantity",quantity);
	const char *sys_info = cJSON_Print(root);
	httpd_resp_sendstr(req, sys_info);
	free((void *)sys_info);
	cJSON_Delete(root);
	return ESP_OK;
}

/* Simple handler for getting weight sensor data */
static esp_err_t mat_get_id_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint32 u32_mat_id=0;
	uint8 param_len=0, mcu_type = 1;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);
	
	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mcu_type", 8) == 0)
		{
			mcu_type = atoi(http_cgi_param_vals[i]);
		}
	}
	
	u32_mat_id = api_get_id(mcu_type);

	ESP_LOGI(REST_TAG, "get mat_id=%d", u32_mat_id);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "mat_id:", u32_mat_id);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Handler to the mat id onto the server */
static esp_err_t mat_set_id_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0,  idh=0, idl=0, mcu_type = 1;
	uint32 u32_mat_id=0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);
	
	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
			idh = (u32_mat_id>>8)&0xFF;
			idl = u32_mat_id&0xFF;
		}
		else if(strncmp(http_cgi_params[i],"mcu_type", 8) == 0)
		{
			mcu_type = atoi(http_cgi_param_vals[i]);
		}
	}
	ESP_LOGI(REST_TAG, "mat_id=%u, idh=%d, idl=%d", u32_mat_id, idh, idl);
	rs485_cmd_set_id(mcu_type, idh, idl);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
	httpd_resp_sendstr(req, "Set MAT ID successfully");

    return ESP_OK;
}

/* Handler to the set mat led onto the server */
static esp_err_t mat_set_led_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, idh=0, idl=0, led_value=0;
	uint32 u32_mat_id=0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
			idh = (u32_mat_id>>8)&0xFF;
			idl = u32_mat_id&0xFF;
		}
		else if(strncmp(http_cgi_params[i],"led_value", 9) == 0)
		{
			led_value = atoi(http_cgi_param_vals[i]);
		}
	}
//	ESP_LOGI(REST_TAG, "mat_id=%u,idh=%d, idl=%d, led_value=%d\r\n", u32_mat_id, idh, idl, led_value);

	rs485_cmd_led(idh, idl, led_value);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
//	httpd_resp_sendstr(req, "Set Cubby LED successfully");

    return ESP_OK;
}

/* Handler to the set mat led onto the server */
static esp_err_t box_set_led_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, idh=0, idl=0;
	uint32 u32_mat_id=0;
	uint32 led_value=0;
	uint32 led_en = 0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"box_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
			idh = (u32_mat_id>>8)&0xFF;
			idl = u32_mat_id&0xFF;
		}
		else if(strncmp(http_cgi_params[i],"led_value", 9) == 0)
		{
			led_value = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"led_en", 6) == 0)
		{
			led_en = atoi(http_cgi_param_vals[i]);
		}
	}
	ESP_LOGI(REST_TAG, "mat_id=%u,idh=%d, idl=%d, led_en = 0x%X, led_value=0x%X\r\n", u32_mat_id, idh, idl, led_en, led_value);

	rs485_cmd_led_box(idh, idl, led_en, led_value);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
//	httpd_resp_sendstr(req, "Set Cubby LED successfully");

    return ESP_OK;
}

/* Handler to the set wifi param onto the server */
static esp_err_t wifi_set_param_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0;
	uint32 u32_mat_id = 0;
	uint8 u8_cubby_index = 0;
	uint8 cmd_index = 0;
	ST_CUBBY one_cubby;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"midl1", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(0, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midr1", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(1, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midl2", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(2, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midr2", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(3, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midl3", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(4, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midr3", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(5, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midl4", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(6, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midr4", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(7, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midl5", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(8, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"midr5", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_id(9, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdl1", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(0, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdr1", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(1, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdl2", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(2, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdr2", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(3, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdl3", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(4, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdr3", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(5, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdl4", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(6, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdr4", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(7, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdl5", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(8, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"lcdr5", 5) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		//	if(u32_mat_id > 0)
			{
				api_set_mat_lcd_id(9, u32_mat_id);
			}
		}
		else if(strncmp(http_cgi_params[i],"peeling_all", 11) == 0)
		{
			if(strncmp(http_cgi_param_vals[i],"true", 4) == 0)
			{
				api_peeling();
			}
		}
		else if(strncmp(http_cgi_params[i],"box_taring_all", 14) == 0)
		{
			if(strncmp(http_cgi_param_vals[i],"true", 4) == 0)
			{
				api_box_taring();
				cmd_index = 2;
			}
		}
		else if(strncmp(http_cgi_params[i],"update_cubby_info", 17) == 0)
		{
			if(strncmp(http_cgi_param_vals[i],"true", 4) == 0)
			{
				cmd_index = 1;
			}
		}
		else if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"cubby_index", 11) == 0)
		{
			u8_cubby_index = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"location_id", 11) == 0)
		{
			cur_len = strlen(http_cgi_param_vals[i]);
			if(cur_len > 60) cur_len = 60;
			memcpy(one_cubby.str_location_id, http_cgi_param_vals[i], cur_len);
			one_cubby.str_location_id[cur_len] = 0;
		}
		else if(strncmp(http_cgi_params[i],"product_num", 11) == 0)
		{
			cur_len = strlen(http_cgi_param_vals[i]);
			if(cur_len > 30) cur_len = 30;
			memcpy(one_cubby.str_product_num, http_cgi_param_vals[i], cur_len);
			one_cubby.str_product_num[cur_len] = 0;
		}
		else if(strncmp(http_cgi_params[i],"desc1", 5) == 0)
		{
			cur_len = strlen(http_cgi_param_vals[i]);
			if(cur_len > 30) cur_len = 30;
			memcpy(one_cubby.str_desc1, http_cgi_param_vals[i], cur_len);
			one_cubby.str_desc1[cur_len] = 0;
		}
		else if(strncmp(http_cgi_params[i],"desc2", 5) == 0)
		{
			cur_len = strlen(http_cgi_param_vals[i]);
			if(cur_len > 30) cur_len = 30;
			memcpy(one_cubby.str_desc2, http_cgi_param_vals[i], cur_len);
			one_cubby.str_desc2[cur_len] = 0;
		}
		else if(strncmp(http_cgi_params[i],"picture", 7) == 0)
		{
			cur_len = strlen(http_cgi_param_vals[i]);
			if(cur_len > 30) cur_len = 30;
			memcpy(one_cubby.str_picture, http_cgi_param_vals[i], cur_len);
			one_cubby.str_picture[cur_len] = 0;
		}
		else if(strncmp(http_cgi_params[i],"min_qty", 7) == 0)
		{
			one_cubby.u32_min_qty = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"max_qty", 7) == 0)
		{
			one_cubby.u32_max_qty = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"reorder_qty", 11) == 0)
		{
			one_cubby.u32_reorder_qty = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"single_weight", 13) == 0)
		{
			one_cubby.f_single_weight = atof(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"weight_per_adc", 14) == 0)
		{
			one_cubby.f_wperadc = atof(http_cgi_param_vals[i]);
		}
	}

	if(cmd_index == 1)
	{
		api_update_cubby_info(u32_mat_id, u8_cubby_index, one_cubby);
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
	if(cmd_index == 0) {
		httpd_resp_sendstr(req, "Assign system mat Id array successfully");
	} else if(cmd_index == 1) {
		httpd_resp_sendstr(req, "Update Cubby information successfully");
	} else if(cmd_index == 2) {
		httpd_resp_sendstr(req, "Paring Box information successfully");
	}

    return ESP_OK;
}

/* Handler to the set mat lcd onto the server */
static esp_err_t mat_set_lcd_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, idh=0, idl=0, lcd_type = 0xA1, str_len = 32, cubby_i = 1, lcd_mode = 0, bg_mode = 1;
	uint32 u32_mat_id = 0;
	char *lcd_buf = NULL;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;

	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);

	param_len = parse_uri_parameters(buf);

	for(int i=0; i<param_len; i++)
	{
	//	ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			u32_mat_id = atoi(http_cgi_param_vals[i]);
			idh = (u32_mat_id>>8)&0xFF;
			idl = u32_mat_id&0xFF;
		}
		else if(strncmp(http_cgi_params[i],"lcd_str", 7) == 0)
		{
			lcd_buf = http_cgi_param_vals[i];
			str_len = strlen(lcd_buf);
		}
		else if(strncmp(http_cgi_params[i],"lcd_type", 8) == 0)
		{
			lcd_type = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"lcd_num", 7) == 0)
		{
			cubby_i = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"lcd_mode", 8) == 0)
		{
			lcd_mode = atoi(http_cgi_param_vals[i]);
		}
		else if(strncmp(http_cgi_params[i],"bg_mode", 7) == 0)
		{
			bg_mode = atoi(http_cgi_param_vals[i]);
		}
	}

	rs485_cmd_lcd(idh, idl, lcd_type, str_len, lcd_buf, cubby_i, lcd_mode, bg_mode);


	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
	httpd_resp_sendstr(req, "Set Cubby LED successfully");

    return ESP_OK;
}

/*
 * http://192.168.4.1/mat/get_weight?mat_id=0201
 */
esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);
#if 0
    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    /* URI handler for fetching temperature data */
    httpd_uri_t temperature_data_get_uri = {
        .uri = "/api/v1/temp/raw",
        .method = HTTP_GET,
        .handler = temperature_data_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &temperature_data_get_uri);

    /* URI handler for light brightness control */
    httpd_uri_t light_brightness_post_uri = {
        .uri = "/api/v1/light/brightness",
        .method = HTTP_POST,
        .handler = light_brightness_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &light_brightness_post_uri);
#endif
	/* URI handler for fetching weight sensor data */
    httpd_uri_t mat_get_weight = {
        .uri		= "/mat/get_weight",
        .method		= HTTP_GET,
        .handler	= mat_get_weight_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &mat_get_weight);

	httpd_uri_t box_get_weight = {
        .uri		= "/box/get_weight",
        .method		= HTTP_GET,
        .handler	= box_get_weight_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &box_get_weight);
	
    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri		= "/*",
        .method		= HTTP_GET,
        .handler	= rest_common_get_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);
	
	/* URI handler for set mat id */
    httpd_uri_t mat_set_id = {
        .uri       = "/mat/set_id",
        .method    = HTTP_POST,
        .handler   = mat_set_id_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &mat_set_id);
	
	/* URI handler for fetching weight sensor data */
    httpd_uri_t mat_get_id = {
        .uri		= "/mat/get_id",
        .method		= HTTP_POST,
        .handler	= mat_get_id_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &mat_get_id);

	/* URI handler for set mat led */
    httpd_uri_t mat_set_led = {
        .uri       = "/mat/set_led",
        .method    = HTTP_POST,
        .handler   = mat_set_led_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &mat_set_led);

	/* URI handler for set box led */
    httpd_uri_t box_set_led = {
        .uri       = "/box/set_led",
        .method    = HTTP_POST,
        .handler   = box_set_led_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &box_set_led);

	/* URI handler for set mat lcd */
    httpd_uri_t mat_set_lcd = {
        .uri       = "/mat/set_lcd",
        .method    = HTTP_POST,
        .handler   = mat_set_lcd_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &mat_set_lcd);

	/* URI handler for set wifi param */
    httpd_uri_t wifi_set_param = {
        .uri       = "/wifi/set_param",
        .method    = HTTP_POST,
        .handler   = wifi_set_param_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &wifi_set_param);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
