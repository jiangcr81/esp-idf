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
#define PACKET_READ_TICS        (100 / portTICK_RATE_MS)		//100ms
#define ECHO_TASK_STACK_SIZE    (20480)
#define ECHO_TASK_PRIO          (10)
#define ECHO_UART_PORT          (UART_NUM_2)

#define	PTL_PREFIX		0x55

#define	MCU_TYPE_HOLTEK		0x01
#define	MCU_TYPE_STM32		0x02

#define	PTL_CMD_HTID	0xFF
#define	PTL_CMD_STMID	0xFE
#define	PTL_CMD_WEIGHT	0x03
#define	PTL_CMD_LED		0x05
#define	PTL_CMD_LCDA1	0xA1
#define	PTL_CMD_LCDA2	0xA2
#define	PTL_CMD_LCDA3	0xA3
#define	PTL_CMD_LCDA4	0xA4

#define	MAT_CNT_MAX		10
#define	BOX_CNT_MAX		4
#define	QUERY_DELAY_MAX	5

typedef struct
{
	uint8	str_location_id[60];
	uint8	str_product_num[30];
	uint8	str_desc1[30];
	uint8	str_desc2[30];
	uint32	u32_min_qty;
	uint32	u32_max_qty;
	uint32	u32_reorder_qty;		//报警数量
	float	f_wperadc;				//1ADC值对应的重量g
	float	f_weight;				//总重量
	float	f_single_weight;		//单个重量
	uint32	u32_current_qty;		//总个数
	uint8	str_picture[30];
	uint32	u32_adc_raw;			//ADC原始值
	uint32	u32_adc_peeling;		//ADC去皮值
	uint8	u8_led_status;			//LED指示灯
}ST_CUBBY;

typedef struct
{
	uint32	u16_mat_id;
	uint32	u32_lcd_id;
	ST_CUBBY	m_cubby[4];
}ST_MAT;

typedef struct
{
	uint32	u32_box_id;
	ST_CUBBY	m_cup[24];
}ST_BOX;

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
	ST_MAT	m_mat[MAT_CNT_MAX];
	ST_BOX	m_box[BOX_CNT_MAX];
	uint32	u32_single_id_tx;
	uint32	u32_single_id_rx;
	uint8	u8_bin_type;			//0:mat; 1:box
}ST_CUBBY_BIN;

extern ST_CUBBY_BIN	m_bin;

void ESP_LOG_STR(uint8 * pstr, int len);

void echo_task(void *arg);
uint8 find_mat_index(uint16 mat_id);

void rs485_tx_package(uint8 type);
void rs485_cmd_led(uint8 idh, uint8 idl, uint8 led_value);
void rs485_cmd_led_box(uint8 idh, uint8 idl, uint32 led_en, uint32 led_value);
void rs485_cmd_set_id(uint8 mcu_type, uint8 idh, uint8 idl);
void rs485_cmd_get_id(uint8 mcu_type);
void rs485_cmd_lcd(uint8 idh, uint8 idl, uint8 type, uint8 len, char * cbuf, uint8 num, uint8 mode, uint8 bg);

uint32 api_get_adc_raw(uint16 mat_id, uint8 cubby_index);
uint32 api_get_id();
void api_set_mat_id(uint8 index, uint32 mat_id);
void api_set_mat_lcd_id(uint8 index, uint32 id);
void api_peeling(void);
void api_box_taring(void);
void api_update_cubby_info(uint32 mat_id, uint8 cubby_index, ST_CUBBY cubby);
uint32 api_get_box_pn(uint32 box_id, uint32 cup_index, char * pdest);
uint32 api_get_box_wperadc(uint32 box_id, uint32 cup_index);

uint32 api_get_box_weight(uint32 box_id, uint32 cup_index);
uint32 api_get_box_qty(uint32 box_id, uint32 cup_index);


#endif
