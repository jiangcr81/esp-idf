#ifndef	__RS485_EXAMPLE_H__
#define	__RS485_EXAMPLE_H__

// Note: UART2 default pins IO16, IO17 do not work on ESP32-WROVER module 
// because these pins connected to PSRAM
#define ECHO_TEST_TXD   (17)	//23
#define ECHO_TEST_RXD   (16)	//22

// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define ECHO_TEST_RTS   (18)

// CTS is not used in RS485 Half-Duplex Mode
#define ECHO_TEST_CTS  UART_PIN_NO_CHANGE

#define BUF_SIZE        (127)
#define BAUD_RATE       (9600)

// Read packet timeout
#define PACKET_READ_TICS        (500 / portTICK_RATE_MS)		//100ms
#define ECHO_TASK_STACK_SIZE    (2048)
#define ECHO_TASK_PRIO          (10)
#define ECHO_UART_PORT          (UART_NUM_2)

#define	PTL_PREFIX		0x55

#define	PTL_CMD_LED		0x05

typedef struct
{
	uint8	str_location_id[60];
	uint8	str_product_num[30];
	uint8	str_desc1[30];
	uint8	str_desc2[30];
	uint32	u32_min_qty;
	uint32	u32_max_qty;
	uint32	u32_reorder_qty;
	float	f_weight;
	float	f_sigel_weight;
	uint32	u32_current_qty;
	uint8	u8_picture[30];
	uint32	u32_adc_raw;			//ADC原始值
	uint32	u32_adc_peeling;		//ADC去皮值
	uint8	u8_led_status;			//LED指示灯
}ST_CUBBY;

typedef struct
{
	uint16	u16_mat_id;
	ST_CUBBY	m_cubby[4];
}ST_MAT;

typedef struct
{
	uint8	str_uuid[100];
	uint8	str_customer_num[50];
	uint8	str_location_uuid[60];
	uint8	str_bin_name[50];
	uint32	u32_type_id;
	uint8	str_user_id[50];
	uint8	str_password[100];
	uint8	str_endpoint[100];
	uint8	str_secret[100];
	uint8	str_wifi_user_id[50];
	uint8	str_wifi_password[100];
	uint8	str_wifi_ssid[50];
	ST_MAT	m_mat[10];
}ST_CUBBY_BIN;

void echo_task(void *arg);
void rs485_tx_package(uint8 type);
void rs485_cmd_led(uint8 idh, uint8 idl, uint8 led_value);


#endif
