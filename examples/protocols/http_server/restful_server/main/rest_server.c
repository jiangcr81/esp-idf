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

static char *http_cgi_params[16];
static char *http_cgi_param_vals[16];

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
  for (loop = 0; (loop < 16) && pair; loop++) {

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

#if 1
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
static esp_err_t mat_weight_data_get_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, mat_id=0, idh=0, idl=0;
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
			mat_id = atoi(http_cgi_param_vals[i]);
			idh = mat_id/100;
			idl = mat_id - (idh*100);
		}
	}
//	ESP_LOGI(REST_TAG, "mat_id=%d, idh=%d, idl=%d", mat_id, idh, idl);

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

/* Handler to the mat id onto the server */
static esp_err_t mat_id_set_handler(httpd_req_t *req)
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

	ESP_LOGI(REST_TAG, "buf[%s],cur_len=%d\r\n", buf, cur_len);
//	rs485_tx_package(1);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_hdr(req, "Location", "self.reload()");
//	httpd_resp_sendstr(req, "Set MAT ID successfully");

    return ESP_OK;
}

/* Handler to the cubby led onto the server */
static esp_err_t cubby_led_set_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	uint8 param_len=0, mat_id=0, idh=0, idl=0, led_value=0;
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
		ESP_LOGI(REST_TAG, "http_cgi_params[%d]=%s,http_cgi_param_vals[%d]=%s\r\n",i,http_cgi_params[i],i,http_cgi_param_vals[i]);

		if(strncmp(http_cgi_params[i],"mat_id", 6) == 0)
		{
			mat_id = atoi(http_cgi_param_vals[i]);
			idh = mat_id/100;
			idl = mat_id - (idh*100);
		}
		else if(strncmp(http_cgi_params[i],"led_value", 9) == 0)
		{
			led_value = atoi(http_cgi_param_vals[i]);
		}
	}
	ESP_LOGI(REST_TAG, "mat_id=%d,idh=%d, idl=%d, led_value=%d\r\n", mat_id, idh, idl, led_value);

	rs485_cmd_led(idh, idl, led_value);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, HTTPD_200);
//	httpd_resp_set_hdr(req, "Location", "self.reload()");
//	httpd_resp_sendstr(req, "Set Cubby LED successfully");

    return ESP_OK;
}


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
#if 1
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
    httpd_uri_t mat_weight_data_get_uri = {
        .uri		= "/mat/get_weight",
        .method		= HTTP_GET,
        .handler	= mat_weight_data_get_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &mat_weight_data_get_uri);
	
    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri		= "/*",
        .method		= HTTP_GET,
        .handler	= rest_common_get_handler,
        .user_ctx	= rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);
	
	/* URI handler for set mat id */
    httpd_uri_t mat_id_set = {
        .uri       = "/mat_id/set",
        .method    = HTTP_POST,
        .handler   = mat_id_set_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &mat_id_set);

	/* URI handler for set cubby led */
    httpd_uri_t cubby_led_set = {
        .uri       = "/mat_led/control",
        .method    = HTTP_POST,
        .handler   = cubby_led_set_handler,
        .user_ctx  = rest_context
    };
    httpd_register_uri_handler(server, &cubby_led_set);


    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
