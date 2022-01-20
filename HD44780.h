/*
 * HD44780.h
 *
 *	I2C Driver for HD44780 LCD display module connected to HW061/PCF8574 8-bit I/O expander module.
 *	Operates in 4-bit mode.
 *
 *  Created on: Jan 6, 2022
 *  Author: Ian Ress
 */

#ifndef INC_HD44780_H_
#define INC_HD44780_H_

/* ####################### INCLUDES ############################# */
#include "stm32f1xx_hal.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* ####################### USER DEFINES ############################### */
#define HD44780_NUM_ROWS 		2
#define HD44780_NUM_COLS 		16
#define HW061_I2C_ADDR 			(0x27<<1) /* PCF8574 (pg. 13) */

/* ####################### DRIVER DEFINES ############################### */
/* I2C Control bits */
#define HD44780_RS				(1 << 0)
#define HD44780_RW        		(1 << 1)
#define HD44780_EN        		(1 << 2)
#define HD44780_BACKLIGHT 		(1 << 3)
#define HD44780_FUNCTION_SET 	(1 << 5)

/* Misc */
#define HD44780_NUM_ELEMENTS	(HD44780_NUM_ROWS * HD44780_NUM_COLS)

/* ####################### Enums ############################### */
typedef enum {
	HD44780_READY,
	HD44780_BUSY,
	HD44780_TIMEOUT
} HD44780_State;

typedef enum {
	LCD1602_ON,
	LCD1602_OFF
} LCD1602_State;

typedef enum {
	CLEAR_DISPLAY,
	RETURN_HOME,
	DISPLAY_ON,
	DISPLAY_OFF,
	CURSOR_ON,
	CURSOR_OFF,
	CURSOR_BLINK,
	CURSOR_UNBLINK
	//MOVE_CURSOR_RIGHT,
	//MOVE_CURSOR_LEFT
} HD44780_User_Command_List;

/* ####################### Structs ############################### */
typedef struct {
	I2C_HandleTypeDef       *HW061_I2C_Handle;
	uint8_t 				Cursor_Position[2];
	char 					Text[HD44780_NUM_ELEMENTS];
	HD44780_State			State;
	LCD1602_State			PowerState;
} HD44780_HandleTypeDef;


/* ####################### Startup Functions ############################### */
bool HD44780_Init(I2C_HandleTypeDef *I2C_Handle, HD44780_HandleTypeDef *Display_Handle);

/* ####################### Writing Functions ############################### */
void HD44780_Print(HD44780_HandleTypeDef *Display_Handle, const char *str);

/* ####################### Command Functions ############################### */
void HD44780_Transmit_Command(HD44780_HandleTypeDef *Display_Handle, HD44780_User_Command_List UserCommand);
void HD44780_Set_Cursor_Position(HD44780_HandleTypeDef *Display_Handle, uint8_t row, uint8_t column);
void HD44780_Animate_Text(HD44780_HandleTypeDef *Display_Handle, uint8_t NumberOfScrolls);

/* ####################### Read Functions ############################### */
char HD44780_Read_Character(HD44780_HandleTypeDef *Display_Handle, uint8_t row, uint8_t column);
uint8_t HD44780_Get_Row_Index(HD44780_HandleTypeDef *Display_Handle);
uint8_t HD44780_Get_Column_Index(HD44780_HandleTypeDef *Display_Handle);


#endif /* INC_HD44780_H_ */
