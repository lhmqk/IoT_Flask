/*
 * dht_20.c
 *
 *  Created on: Dec 22, 2022
 *      Author: acer
 */

#include "dht20.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

#define SLAVE_ADDRESS_DHT20 (0x38 << 1)

uint16_t value_x10[2] = {0, 0};
char temp[20], humid[20];

void dht20_init(){
	//Set register when call a wrong reset
	uint8_t init[3];

	init[0] = 0xA8;
	init[1] = 0x00;
	init[2] = 0x00;
	HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) init, 3, 0xFF);
	HAL_Delay(20);

	init[0] = 0xBE;
	init[1] = 0x08;
	init[2] = 0x00;
	HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) init, 3, 0xFF);
	HAL_Delay(20);
}

void dht20_reset(uint8_t regist){
	//reset register
	uint8_t reset[3], reply[3];

	reset[0] = regist;
	reset[1] = 0x00;
	reset[2] = 0x00;
 	HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) reset, 3, 0xFF);
	HAL_Delay(10);

	HAL_I2C_Master_Receive(&hi2c1, SLAVE_ADDRESS_DHT20 | 0x01, (uint8_t*) reply, 3, 0xFF);
	reset[0] = 0xB0 | regist;
	reset[1] = reply[1];
	reset[2] = reply[2];
	HAL_Delay(10);

	HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) reset, 3, 0xFF);
}

void dht20_start(){
	//query the DHT20
	uint8_t status[1];
	HAL_I2C_Master_Receive(&hi2c1, SLAVE_ADDRESS_DHT20 | 0x01, (uint8_t*) status, 1, 0xFF);

	if((status[0] & 0x18) != 0x18){
		dht20_reset(0x1B);
		dht20_reset(0x1C);
		dht20_reset(0x1E);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, RESET);
	}

	if ((status[0] & 0x18) == 0x18){
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, SET);
	}

	uint8_t data[3] = {0xAC, 0x33, 0x00};

	//HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) trigger, 1, 0xFF);
	HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_DHT20, (uint8_t*) data, 3, 0xFF);
	HAL_Delay(80);
}

void dht20_read(uint16_t* value){
	dht20_start();
	uint8_t data[7];
	uint32_t Temper = 0, Humid = 0;
	HAL_I2C_Master_Receive(&hi2c1, SLAVE_ADDRESS_DHT20 | 0x01, (uint8_t*) data, 7, 0xFF);

	//Humid
	Humid = (Humid | data[1]) << 8;
	Humid = (Humid | data[2]) << 8;
	Humid = Humid | data[3];
	Humid = Humid >> 4;
    Humid = (Humid * 100 * 10 / 1024 / 1024);
    value[0] = Humid;

	//Temperature
    Temper = (Temper | data[3]) << 8;
    Temper = (Temper | data[4]) << 8;
    Temper = Temper | data[5];
    Temper = Temper & 0xfffff;
    Temper = Temper*200*10/1024/1024 - 500;
	value[1] = Temper;

}

void dht20_output(){
	dht20_read(value_x10);
	char msg[64];
	uint32_t checkSum = 0;

	//Checksum temp and publish
	checkSum = msgCheckSum(&msg[0], sprintf(msg, "!TEMP:%d.%d#",value_x10[1]/10,value_x10[1]%10));
	sprintf(msg, "!TEMP:%d.%d:%lu#",value_x10[1]/10,value_x10[1]%10,checkSum);
	HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);

	//Checksum humid and publish
	checkSum = msgCheckSum(&msg[0], sprintf(msg, "!HUMID:%01d.%d#",value_x10[0]/10,value_x10[0]%10));
	sprintf(msg, "!HUMID:%01d.%d:%lu#",value_x10[0]/10,value_x10[0]%10,checkSum);
	HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);

	//Display on the LCD
	sprintf(temp, "TEMP:  %d.%d %cC",value_x10[1]/10,value_x10[1]%10 ,0b11011111);
	sprintf(humid,"HUMID: %01d.%d %%   ",value_x10[0]/10,value_x10[0]%10);
	lcd_show_value();
}
