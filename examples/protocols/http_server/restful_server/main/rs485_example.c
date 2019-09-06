/* Uart Events Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "config.h"

/**
 * This is a example example which echos any data it receives on UART back to the sender.
 *
 * - port: UART2
 * - rx buffer: on
 * - tx buffer: off
 * - flow control: off
 *
 * This example has been tested on a 3 node RS485 Serial Bus
 * 
 */

static const char *TAG = "RS485_ECHO_APP";

static ST_CUBBY_BIN	m_bin;

uint8 calc_str_sum(uint8 * pstr, uint8 len)
{
	uint8 rxsumi;
	uint8 u8_uart_sum = 0;
	//计算接收数据的校验和
	for(rxsumi=0; rxsumi<len; rxsumi++)
	{
		u8_uart_sum += pstr[rxsumi];
	}
	u8_uart_sum ^= 0xA5;
	return u8_uart_sum;
}

uint16 if_data_ckeck(uint8* pstr, uint8 len)
{
	uint8 rxsumi = 0;
	uint16 u16_check_sum=0;
	for(rxsumi=0; rxsumi<len; rxsumi++)
	{
		u16_check_sum += pstr[rxsumi];
	}
	return u16_check_sum;
}

/*
 * 设置地址 
 * 55 12 01 01 01 02 01 00 00 00 00 01 00 02 02 01 00 73
 * 读取传感器数据
 * 55 10 01 01 01 02 01 00 00 00 00 03 00 00 00 6E
 * 灯设置
 * 55 11 01 01 01 02 01 00 00 00 00 05 00 01 01 00 73
 */
void rs485_tx_package(uint8 type)
{
	int uart_num = ECHO_UART_PORT;
	uint8 tx_len = 0;
	uint16 tx_sum = 0;
	uint8_t* tx_buf = (uint8_t*) malloc(BUF_SIZE);
	memset(tx_buf, 0x00, BUF_SIZE);

	if(type == 1)
	{
		//设置地址
		tx_len = 4;
		tx_buf[0] = 0xAA;
		tx_buf[1] = 0xAA;
		tx_buf[2] = 0x02;
		tx_buf[3] = 0x01;
		/*
		tx_len = 0x12;
		tx_buf[0] = PTL_PREFIX;
		tx_buf[1] = tx_len;
		tx_buf[2] = 0x01;
		tx_buf[3] = 0x01;
		tx_buf[4] = 0x01;
		tx_buf[5] = 0x02;
		tx_buf[6] = 0x01;
		tx_buf[7] = 0x00;
		tx_buf[8] = 0x00;
		tx_buf[9] = 0x00;
		tx_buf[10] = 0x00;
		tx_buf[11] = 0x01;
		tx_buf[12] = 0x00;
		tx_buf[13] = 0x02;
		tx_buf[14] = 0x02;
		tx_buf[15] = 0x01;
		*/
	}
	else if(type == 2)
	{
		//读取称重数据
		tx_len = 0x10;
		tx_buf[0] = PTL_PREFIX;
		tx_buf[1] = tx_len;
		tx_buf[2] = 0x01;
		tx_buf[3] = 0x01;
		tx_buf[4] = 0x01;
		tx_buf[5] = 0x02;	//Target Address H
		tx_buf[6] = 0x01;	//Target Address L
		tx_buf[7] = 0x00;
		tx_buf[8] = 0x00;
		tx_buf[9] = 0x00;
		tx_buf[10] = 0x00;
		tx_buf[11] = 0x03;
		tx_buf[12] = 0x00;
		tx_buf[13] = 0x00;
		
		if(tx_len > 1)
		{
			tx_sum = if_data_ckeck(tx_buf, tx_len-2);
			tx_buf[tx_len-2] = (tx_sum>>8)&0xFF;
			tx_buf[tx_len-1] = (tx_sum)&0xFF;
		}
	}
	else if(type == 3)
	{
		//灯设置
		tx_len = 0x11;
		tx_buf[0] = PTL_PREFIX;
		tx_buf[1] = tx_len;
		tx_buf[2] = 0x01;	//type
		tx_buf[3] = 0x00;	//host address h
		tx_buf[4] = 0x00;	//host address l
		tx_buf[5] = 0x02;	//device address h
		tx_buf[6] = 0x01;	//device address l
		tx_buf[7] = 0x00;	//reserved
		tx_buf[8] = 0x00;
		tx_buf[9] = 0x00;
		tx_buf[10] = 0x00;
		tx_buf[11] = 0x05;	//set LED on/off
		tx_buf[12] = 0x00;	//cmd type
		tx_buf[13] = 0x01;	//payload length
		tx_buf[14] = 0x00;	//<3>LED4;<2>LED3;<1>LED2;<0>LED1
		
		tx_sum = if_data_ckeck(tx_buf, tx_len-2);
		tx_buf[tx_len-2] = (tx_sum>>8)&0xFF;
		tx_buf[tx_len-1] = (tx_sum)&0xFF;
	}
	
	uart_write_bytes(uart_num, (char *)tx_buf, tx_len);
}

void rs485_cmd_get_adc(uint8 idh, uint8 idl)
{
	int uart_num = ECHO_UART_PORT;
	uint8 tx_len = 0x10;
	uint16 tx_sum = 0;
	uint8_t* tx_buf = (uint8_t*) malloc(tx_len+1);
	memset(tx_buf, 0x00, tx_len+1);
	
	tx_buf[0] = PTL_PREFIX;
	tx_buf[1] = tx_len;
	tx_buf[2] = 0x01;	//type
	tx_buf[3] = 0x00;	//host address h
	tx_buf[4] = 0x00;	//host address l
	tx_buf[5] = idh;	//device address h
	tx_buf[6] = idl;	//device address l
	tx_buf[7] = 0x00;	//reserved
	tx_buf[8] = 0x00;	//reserved
	tx_buf[9] = 0x00;	//reserved
	tx_buf[10] = 0x00;	//reserved
	tx_buf[11] = PTL_CMD_WEIGHT;	//Get Weight
	tx_buf[12] = 0x00;	//cmd type
	tx_buf[13] = 0x00;	//payload length
	
	tx_sum = if_data_ckeck(tx_buf, tx_len-2);
	tx_buf[tx_len-2] = (tx_sum>>8)&0xFF;
	tx_buf[tx_len-1] = (tx_sum)&0xFF;
	
	uart_write_bytes(uart_num, (char *)tx_buf, tx_len);
}
/*
 *
 */
void rs485_cmd_led(uint8 idh, uint8 idl, uint8 led_value)
{
	int uart_num = ECHO_UART_PORT;
	uint8 tx_len = 0x11;
	uint16 tx_sum = 0;
	uint8_t* tx_buf = (uint8_t*) malloc(tx_len+1);
	memset(tx_buf, 0x00, tx_len+1);
	
	tx_buf[0] = PTL_PREFIX;
	tx_buf[1] = tx_len;
	tx_buf[2] = 0x01;	//type
	tx_buf[3] = 0x00;	//host address h
	tx_buf[4] = 0x00;	//host address l
	tx_buf[5] = idh;	//device address h
	tx_buf[6] = idl;	//device address l
	tx_buf[7] = 0x00;	//reserved
	tx_buf[8] = 0x00;	//reserved
	tx_buf[9] = 0x00;	//reserved
	tx_buf[10] = 0x00;	//reserved
	tx_buf[11] = PTL_CMD_LED;	//set LED on/off
	tx_buf[12] = 0x00;	//cmd type
	tx_buf[13] = 0x01;	//payload length
	tx_buf[14] = led_value;	//<7:4>EN;<3>LED4;<2>LED3;<1>LED2;<0>LED1
	
	tx_sum = if_data_ckeck(tx_buf, tx_len-2);
	tx_buf[tx_len-2] = (tx_sum>>8)&0xFF;
	tx_buf[tx_len-1] = (tx_sum)&0xFF;
	
	uart_write_bytes(uart_num, (char *)tx_buf, tx_len);
}

uint8 find_mat_index(uint16 mat_id)
{
	uint8 u8_ret = 0xFF, i;
	for(i = 0; i<MAT_CNT_MAX; i++)
	{
	//	ESP_LOGI(TAG, "m_mat_id[%d]=0x%.8X, param mat_id=0x%.8X", i, m_bin.m_mat[i].u16_mat_id, mat_id);
		if(m_bin.m_mat[i].u16_mat_id == mat_id)
		{
		//	ESP_LOGI(TAG, "found mat.");
			u8_ret = i;
			break;
		}
	}
//	ESP_LOGI(TAG, "mat not found.");
	return u8_ret;
}

uint32 u8_3_uint32(uint8 u8h, uint8 u8m, uint8 u8l)
{
	return (((uint16)u8h<<16)|((uint16)u8m<<8)|u8l);
}

void init_m_bin(void)
{
	uint8 i = 0, j = 0;
	for(i = 0; i<MAT_CNT_MAX; i++)
	{
		m_bin.m_mat[i].u16_mat_id = 0;
		for(j = 0; j < 4; j++)
		{
			m_bin.m_mat[i].m_cubby[j].u32_adc_raw = 0;
			m_bin.m_mat[i].m_cubby[j].u32_adc_peeling = 0;
			m_bin.m_mat[i].m_cubby[j].u8_led_status = 0;
		}
	}

	m_bin.m_mat[1].u16_mat_id = 0x0201;
}

uint32 api_get_adc_raw(uint16 mat_id, uint8 cubby_index)
{
	uint32 u32_ret = 0;
	uint8 u8_mat_index = 0;
	
	if(cubby_index > 3)
		return 0;
	
	u8_mat_index = find_mat_index(mat_id);
	if(u8_mat_index < MAT_CNT_MAX)
	{
		u32_ret = m_bin.m_mat[u8_mat_index].m_cubby[cubby_index].u32_adc_raw;
	}
	return u32_ret>>5;
}

/*
0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15   16   17   18   19   20   21   22   23   24   25   26   27
0x55 0x1C 0x01 0x02 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x03 0x01 0x0C 0x80 0x43 0x70 0x7E 0xE6 0xB8 0x7F 0xF2 0xC8 0x81 0x3A 0x58 0x07 0x20


*/
void rs485_parse_rx(uint8 * psrc, uint8 len)
{
	uint16 tx_sum_raw = 0;
	uint16 tx_sum = 0;
	uint16 u16_mat_id = 0;
	uint8 u8_mat_index = 0;
	tx_sum_raw = ((uint16)psrc[len-2]<<8|psrc[len-1]);
	tx_sum = if_data_ckeck(psrc, len-2);
	if(tx_sum_raw == tx_sum)
	{
	//	ESP_LOGI(TAG, "sum ok!\r\n");
		if(psrc[0] == 0x55)
		{
		//	ESP_LOGI(TAG, "head ok!\r\n");
			if(psrc[1] == len)
			{
			//	ESP_LOGI(TAG, "length ok!\r\n");
				if(psrc[2] == 0x01)
				{
					u16_mat_id = ((uint16)psrc[3]<<8)|psrc[4];
				//	ESP_LOGI(TAG, "u16_mat_id = 0x%.4X", u16_mat_id);
					u8_mat_index = find_mat_index(u16_mat_id);
					if(u8_mat_index < MAT_CNT_MAX)
					{

						if(psrc[11] == PTL_CMD_WEIGHT)
						{
							m_bin.m_mat[u8_mat_index].m_cubby[0].u32_adc_raw = u8_3_uint32(psrc[14], psrc[15], psrc[16]);
							m_bin.m_mat[u8_mat_index].m_cubby[1].u32_adc_raw = u8_3_uint32(psrc[17], psrc[18], psrc[19]);
							m_bin.m_mat[u8_mat_index].m_cubby[2].u32_adc_raw = u8_3_uint32(psrc[20], psrc[21], psrc[22]);
							m_bin.m_mat[u8_mat_index].m_cubby[3].u32_adc_raw = u8_3_uint32(psrc[23], psrc[24], psrc[25]);
						//	ESP_LOGI(TAG,"cubby1:%d", m_bin.m_mat[u8_mat_index].m_cubby[0].u32_adc_raw);
						//	ESP_LOGI(TAG,"cubby2:%d", m_bin.m_mat[u8_mat_index].m_cubby[1].u32_adc_raw);
						//	ESP_LOGI(TAG,"cubby3:%d", m_bin.m_mat[u8_mat_index].m_cubby[2].u32_adc_raw);
						//	ESP_LOGI(TAG,"cubby4:%d", m_bin.m_mat[u8_mat_index].m_cubby[3].u32_adc_raw);
						//	ESP_LOGI(TAG, "store adc raw data ok!\r\n");
						}
					}
				}
			}
		}
	}
}

// An example of echo test with hardware flow control on UART
void echo_task(void *arg)
{
    const int uart_num = ECHO_UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    
    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Start RS485 application test and configure UART.");

    // Configure UART parameters
    uart_param_config(uart_num, &uart_config);
    
    ESP_LOGI(TAG, "UART set pins, mode and install driver.");
    // Set UART1 pins(TX: IO23, RX: I022, RTS: IO18, CTS: IO19)
    uart_set_pin(uart_num, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0);

    // Set RS485 half duplex mode
    uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX);

    // Allocate buffers for UART
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);

    ESP_LOGI(TAG, "UART start recieve loop.\r\n");
//	uart_write_bytes(uart_num, "Start RS485 UART test.\r\n", 24);
	init_m_bin();

    while(1) {
        //Read data from UART
        int len = uart_read_bytes(uart_num, data, BUF_SIZE, PACKET_READ_TICS);

		//*
        //Write data back to UART
        if (len > 0) {
			rs485_parse_rx(data, len);
        //	uart_write_bytes(uart_num, "\r\n", 2);
        //	char prefix[] = "RS485 Received: [";
        //	uart_write_bytes(uart_num, prefix, (sizeof(prefix) - 1));
            /*
            ESP_LOGI(TAG, "Received %u bytes:", len);
            printf("[ ");
            for (int i = 0; i < len; i++) {
                printf("0x%.2X ", (uint8_t)data[i]);
            //	uart_write_bytes(uart_num, (const char*)&data[i], 1);
                // Add a Newline character if you get a return charater from paste (Paste tests multibyte receipt/buffer)
                if (data[i] == '\r') {
                //	uart_write_bytes(uart_num, "\n", 1);
                }
            }
            printf("] \n");
            //*/
        //	uart_write_bytes(uart_num, "]\r\n", 3);
        } else {
            // Echo a "." to show we are alive while we wait for input
            //uart_write_bytes(uart_num, ".", 1);
		//	rs485_tx_package(2);
			rs485_cmd_get_adc(0x02, 0x01);

		//	rs485_cmd_led(2, 1, 3, 1);
        }
        //*/
    }
}

/*
void app_main(void)
{
    //A uart read/write example without event queue;
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIO, NULL);
}
*/
