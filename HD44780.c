/*
 * HD44780.c
 *
 *	I2C Driver for HD44780 LCD display module connected to HW061/PCF8574 (NXP Semiconductor)
 *	8-bit I/O expander module. Operates in 4-bit mode.
 *
 *  Created on: Jan 6, 2022
 *  Author: Ian Ress
 */

/* ####################### Includes ############################# */
#include "HD44780.h"

/* ################### Private variables ######################## */
static uint8_t AddressCounter; /* Stores address location of LCD cursor. See HD44780 pg.9*/

/* ################### Private Function Prototypes ############### */
static void HD44780_Check_Status(HD44780_HandleTypeDef *Display_Handle);
static void HD44780_Send_Command(HD44780_HandleTypeDef *Display_Handle, uint8_t Command, bool CheckBusyFlag);
static void HD44780_Send_Data(HD44780_HandleTypeDef *Display_Handle, uint8_t Command, bool CheckBusyFlag);
static void HD44780_Error_Handler(HD44780_HandleTypeDef *Display_Handle);
static void HD44780_Get_Cursor_Position(HD44780_HandleTypeDef *Display_Handle);
static void Clear_Text_Buffer(HD44780_HandleTypeDef *Display_Handle);

/* ####################### Private Functions ##################### */
/**
  * @brief 	Reads AC register of HD44780 to see if busy flag is set. Sets Address Counter and sets
  * 		Display_Handle->State according to result. See HD44780 pg. 24 and pg. 58, Figure 26 for timing diagrams.
  * 		See PCF8574 pg. 9 for how to read I2C data.
  * @param 	Pointer to HD44780_HandleTypeDef struct.
  */
static void HD44780_Check_Status(HD44780_HandleTypeDef *Display_Handle)
{
	uint8_t ReadCommand;
	uint8_t ReadBuffer;
	uint8_t Data_7_4 = 0;
	uint8_t Data_3_0 = 0;
	uint8_t Data;

	/* Check busy flag instruction */
	ReadCommand = (0xF0 | HD44780_BACKLIGHT | HD44780_RW); /* PCF8574 pg.9: data bits must be set HIGH before read */
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadCommand, 1, 200);
	HAL_Delay(1);

	/* Read first 4 bits of data (MSB first) */
	ReadCommand |= (HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadCommand, 1, 200);
	HAL_Delay(1);
	HAL_I2C_Master_Receive(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadBuffer, 1, 200);
	Data_7_4 = (ReadBuffer & 0xF0); /*only care about first 4 bits*/
	ReadCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadCommand, 1, 200);
	HAL_Delay(1);

	/* Read last 4 bits of data */
	ReadCommand |= (HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadCommand, 1, 200);
	HAL_Delay(1);
	HAL_I2C_Master_Receive(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadBuffer, 1, 200);
	Data_3_0 = ((ReadBuffer >> 4) & 0x0F);
	ReadCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &ReadCommand, 1, 200);
	HAL_Delay(1);

	Data = (Data_7_4 | Data_3_0);
	if ((Data) & (1<<7)) { /* Busy flag set */
		Display_Handle->State = HD44780_BUSY;
	}

	else {
		AddressCounter = (Data & ~(1<<7)); /* Update AddressCounter with new data */
		Display_Handle->State = HD44780_READY;
	}
}

/**
  * @brief 	Writes control command to HD44780 (RS and R/W bits = LOW).
  * 		See HD44780 pg. 24 and pg. 58, Figure 25.
  * @param 	Pointer to HD44780_HandleTypeDef struct.
  * @param 	Hex command to be sent.
  * @param 	CheckBusyFlag.
  */
static void HD44780_Send_Command(HD44780_HandleTypeDef *Display_Handle, uint8_t Command, bool CheckBusyFlag)
{
	Display_Handle->State = HD44780_BUSY;

	uint8_t WriteCommand = 0;
	const uint8_t Command_7_4 = (0xF0 & Command);
	const uint8_t Command_3_0 = (0xF0 & (Command<<4));

	/* Write first 4 bits of data (MSB first) */
	WriteCommand |= Command_7_4 | HD44780_BACKLIGHT | HD44780_EN;
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);
	WriteCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);

	/* Write last 4 bits of data */
	WriteCommand &= 0x0F;
	WriteCommand |= (Command_3_0 | HD44780_BACKLIGHT | HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);
	WriteCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);

	/* Exit once HD44780 is finished writing an instruction (busy flag isn't set) */
	if (CheckBusyFlag) {
		for (int i = 0; i < 20; i++) {
			HD44780_Check_Status(Display_Handle);
			if (Display_Handle->State == HD44780_READY) {
				return;
			}
		}

		/* >100ms has passed at this point at this point. Throw timeout error */
		Display_Handle->State = HD44780_TIMEOUT;
		HD44780_Error_Handler(Display_Handle);
	}

	else {
		Display_Handle->State = HD44780_READY;
	} /* Not checking busy flag */
}

/**
  * @brief 	Writes data to HD44780 DDRAM. Same process as HD44780_Send_Command
  * 		except (RS = HIGH and R/W = LOW). See HD44780 pg. 17, Table 4 and pg. 58, Figure 25.
  * @param 	Pointer to HD44780_HandleTypeDef struct.
  * @param 	Hex data to be sent.
  */
static void HD44780_Send_Data(HD44780_HandleTypeDef *Display_Handle, uint8_t Command, bool CheckBusyFlag)
{
	Display_Handle->State = HD44780_BUSY;

	uint8_t WriteCommand = 0;
	const uint8_t Command_7_4 = (0xF0 & Command);
	const uint8_t Command_3_0 = (0xF0 & (Command<<4));

	/* Write first 4 bits of data (MSB first) */
	WriteCommand |= HD44780_BACKLIGHT | HD44780_RS;
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1); /* RS line must settle before pulling EN HIGH */
	WriteCommand |= Command_7_4 | HD44780_BACKLIGHT | HD44780_EN | HD44780_RS;
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);
	WriteCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);

	/* Write last 4 bits of data */
	WriteCommand &= 0x0F;
	WriteCommand |= (Command_3_0 | HD44780_BACKLIGHT | HD44780_EN | HD44780_RS);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);
	WriteCommand &= ~(HD44780_EN);
	HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
	HAL_Delay(1);


	/* Exit once HD44780 is finished writing an instruction (busy flag isn't set) */
	if (CheckBusyFlag) {
		for (uint8_t i = 0; i < 20; i++) {
			HD44780_Check_Status(Display_Handle);
			if (Display_Handle->State == HD44780_READY) {
				return;
			}
		}
		/* >100ms has passed at this point at this point. Throw timeout error */
		Display_Handle->State = HD44780_TIMEOUT;
		HD44780_Error_Handler(Display_Handle);
	}

	else {
		Display_Handle->State = HD44780_READY;
	} /* Not checking busy flag */
}

/**
  * @brief 	Executes when HD44780 timeout error occurs. Up to user
  * 		on how to handle.
  * @param 	Pointer to HD44780_HandleTypeDef struct.
  */
static void HD44780_Error_Handler(HD44780_HandleTypeDef *Display_Handle)
{
	while (Display_Handle->State != HD44780_READY) {
		/* IMPLEMENT FUNCTION HOWEVER YOU WANT */

		/*Display_Handle->State = HD44780_READY;*/
	}

}

/**
  * @brief 	Updates the current cursor position in Display_Handle->CursorPosition by direct read from
  * 		LCD controller. See HD44780 pgs. 10-12 for how AddressCounter relates to the cursor's position.
  * 		TODO - MAKE COMPATIBLE FOR ALL DISPLAYS (VARYING ROW/COLUMN COUNT)
  * @param 	Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  */
static void HD44780_Get_Cursor_Position(HD44780_HandleTypeDef *Display_Handle)
{
	HD44780_Check_Status(Display_Handle); /* Read position directly from LCD */
	const uint8_t AddressCopy = AddressCounter;

	if (AddressCopy < 40) {
		Display_Handle->Cursor_Position[0] = 0; /* First row */
		Display_Handle->Cursor_Position[1] = AddressCopy;
	}

	else {
		Display_Handle->Cursor_Position[0] = 1; /* Second row */
		Display_Handle->Cursor_Position[1] = (uint8_t)(AddressCopy - 64);
	}
}

/**
  * @brief 	Clears text stored in Display_Handle->Text.
  * @param 	Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  */
static void Clear_Text_Buffer(HD44780_HandleTypeDef *Display_Handle)
{
	for (int i = 0; i < HD44780_NUM_ELEMENTS; i++) {
		Display_Handle->Text[i] = '\0';
	}
}


/* ########################## Public Functions ########################### */
/**
  * @brief  Meant for end user. Initializes HD44780 through software reset.
  * 		See HD44780 pg. 46, Figure 24 for initialization sequence.
  * @param  Pointer to HAL I2C_HandleTypeDef
  * @param  Pointer to HD44780_HandleTypeDef struct.
  * @retval Boolean indicating if initialization was successful.
  */
bool HD44780_Init(I2C_HandleTypeDef *I2C_Handle, HD44780_HandleTypeDef *Display_Handle)
{
	if (HAL_I2C_IsDeviceReady(I2C_Handle, HW061_I2C_ADDR, 10, HAL_MAX_DELAY) == HAL_OK) {
		HAL_Delay(45);
		Display_Handle->HW061_I2C_Handle = I2C_Handle;
		Display_Handle->Cursor_Position[0] = 0;
		Display_Handle->Cursor_Position[1] = 0;

		/* Must send commands in 8-bit mode */
		uint8_t WriteCommand = (0x30 | HD44780_EN);
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1); /* EN must be HIGH for at least 450ns, data setup time at least 80ns */
		WriteCommand &= ~(HD44780_EN);
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(5);

		WriteCommand |= HD44780_EN; /* Instruction = 0x30 */
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);
		WriteCommand &= ~(HD44780_EN);
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);

		WriteCommand |= HD44780_EN; /* Instruction = 0x30 */
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);
		WriteCommand &= ~(HD44780_EN);
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);

		WriteCommand = (0x20 | HD44780_EN); /* Instruction = 0x20 */
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);
		WriteCommand &= ~(HD44780_EN);
		HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &WriteCommand, 1, 200);
		HAL_Delay(1);

		/* Can now send commands in 4-bit mode */
		WriteCommand = HD44780_FUNCTION_SET; /* CURRENTLY ONLY SUPPORT 4-BIT MODE */
			if (HD44780_NUM_ROWS == 2) {
				WriteCommand |= (1<<3);
			} /* else 1 row used */

		HD44780_Send_Command(Display_Handle, WriteCommand, true); /* 4-bit mode, num of rows, 5x8 character font */

		HD44780_Send_Command(Display_Handle, 0x08, true); /* Turn display off */

		HD44780_Send_Command(Display_Handle, 0x01, true); /* Clear display */

		HD44780_Send_Command(Display_Handle, 0x06, true); /* Auto-increment, no display shift */

		HD44780_Send_Command(Display_Handle, 0x0C, true); /* Turn display on */
		/* END OF SOFTWARE RESET */

		HD44780_Transmit_Command(Display_Handle, CURSOR_ON);
		Display_Handle->PowerState = LCD1602_ON;
		return true;
	}

	else {
		return false;
	} /* No established I2C connection with slave device */
}

/**
  * @brief Meant for end-user. Sends pre-defined commands to HD44780.
  * @param Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @param UserCommand from enum HD44780_User_Command_List (CLEAR_DISPLAY, RETURN_HOME, ...)
  */
void HD44780_Transmit_Command(HD44780_HandleTypeDef *Display_Handle, HD44780_User_Command_List UserCommand)
{
	uint8_t Command;

	if (Display_Handle->PowerState == LCD1602_ON) {
		switch(UserCommand) {
			case CLEAR_DISPLAY:
				Command = 0x01;
				HD44780_Send_Command(Display_Handle, Command, true);
				Clear_Text_Buffer(Display_Handle);
				Display_Handle->Cursor_Position[0] = 0;
				Display_Handle->Cursor_Position[1] = 0;
				break;

			case RETURN_HOME:
				Command = 0x02;
				HD44780_Send_Command(Display_Handle, Command, true);
				Display_Handle->Cursor_Position[0] = 0;
				Display_Handle->Cursor_Position[1] = 0;
				break;

			case DISPLAY_OFF:
				Command = 0;
				HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &Command, 1, 200);
				Display_Handle->PowerState = LCD1602_OFF;
				break;

			case CURSOR_ON:
				Command = 0x0E;
				HD44780_Send_Command(Display_Handle, Command, true);
				break;

			case CURSOR_OFF:
				Command = 0x0C;
				HD44780_Send_Command(Display_Handle, Command, true);
				break;

			case CURSOR_BLINK:
				Command = 0x0D;
				HD44780_Send_Command(Display_Handle, Command, true);
				break;

			case CURSOR_UNBLINK:
				Command = 0x0C;
				HD44780_Send_Command(Display_Handle, Command, true);
				break;

			default:
				break;
		}
	}

	else if (Display_Handle->PowerState == LCD1602_OFF) {
		switch (UserCommand) {
			case DISPLAY_ON:
				Command = HD44780_BACKLIGHT;
				HAL_I2C_Master_Transmit(Display_Handle->HW061_I2C_Handle, HW061_I2C_ADDR, &Command, 1, 200);
				Display_Handle->PowerState = LCD1602_ON;
				break;

			default:
				break;
		}
	}
}

/**
  * @brief 	Meant for end-user. Prints text to the LCD starting at current
  * 		cursor position. Automatically goes to new row when there's no more display
  * 		room on the 1st row. Stops printing and sets cursor to (0,0) if there's no more
  * 		display room.
  * 		TODO - MAKE COMPATIBLE FOR ALL DISPLAYS (VARYING ROW/COLUMN COUNT)
  * @param 	Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @param 	String to print.
  */
void HD44780_Print(HD44780_HandleTypeDef *Display_Handle, const char *str)
{
	HD44780_Get_Cursor_Position(Display_Handle);
	uint8_t row = Display_Handle->Cursor_Position[0];
	uint8_t column = Display_Handle->Cursor_Position[1];
	uint8_t startindex = ((row * HD44780_NUM_COLS) + column);; /*Find index where text ends in Display_Handle->Text */

	for (int i = 0; i < strlen(str); i++) {
		if ((startindex + i) > (HD44780_NUM_ELEMENTS - 1)) { /* No more display room */
			HD44780_Set_Cursor_Position(Display_Handle, 0, 0);
			return;
		}

		HD44780_Send_Data(Display_Handle, (uint8_t)str[i], true); /* Updates AddressCounter automatically */
		Display_Handle->Text[startindex + i] = str[i];

		if (AddressCounter == HD44780_NUM_COLS) { /* end of first row, go to second row */
			HD44780_Set_Cursor_Position(Display_Handle, 1, 0);
		}
	}
	HD44780_Get_Cursor_Position(Display_Handle); /* Update cursor position at end */
}

/**
  * @brief  Meant for end-user. Sets the cursor position WILL NOT WORK IF
  * 		DISPLAY IS LEFT OR RIGHT SHIFTED AT ANY POINT.
  * 		TODO - MAKE COMPATIBLE FOR ALL DISPLAYS (VARYING ROW/COLUMN COUNT)
  * @param 	Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @param 	Row index (0-indexed).
  * @param 	Column index (0-indexed).
  */
void HD44780_Set_Cursor_Position(HD44780_HandleTypeDef *Display_Handle, uint8_t row, uint8_t column)
{
	uint8_t Command;
	if ((row >= HD44780_NUM_ROWS) | (column >= HD44780_NUM_COLS)) {
		return;
	}

	else{
		if (row == 0){
			Command = (0x80 | column);
			HD44780_Send_Command(Display_Handle, Command, true);
		}

		else{
			Command = (0x80 | 0x40 | column);
			HD44780_Send_Command(Display_Handle, Command, true);
		}
	}
}

/**
  * @brief  Meant for end-user. Retrieves the character on LCD display at specified coordinates.
  * TODO - MAKE COMPATIBLE FOR ALL DISPLAYS (VARYING ROW/COLUMN COUNT)
  * @param  Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @param	Row coordinate.
  * @param	Column coordinate.
  * @retval Character at specified coordinate.
  */
char HD44780_Read_Character(HD44780_HandleTypeDef *Display_Handle, uint8_t row, uint8_t column)
{
	if ((row >= HD44780_NUM_ROWS) | (column >= HD44780_NUM_COLS)) {
		return 0;
	}

	else {
		uint8_t index = ((row * HD44780_NUM_COLS) + column);
		return (Display_Handle->Text[index]);
	}

}

/**
  * @brief  Meant for end-user. Retrieves the current row number the cursor is on.
  * @param  Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @retval Current row (0-indexed).
  */
uint8_t HD44780_Get_Row_Index(HD44780_HandleTypeDef *Display_Handle)
{
	return Display_Handle->Cursor_Position[0];
}

/**
  * @brief  Meant for end-user. Retrieves the current column number the cursor is on.
  * @param  Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @retval Current column (0-indexed).
  */
uint8_t HD44780_Get_Column_Index(HD44780_HandleTypeDef *Display_Handle)
{
	return Display_Handle->Cursor_Position[1];
}

/**
  * @brief  Meant for end-user. Scrolls LCD text from left to right. See HD44780 pgs. 10-12 and 27.
  * @param  Pointer to HD44780_HandleTypeDef struct that user defines in beginning of program.
  * @param	Number of times to scroll the text across the LCD.
  */
void HD44780_Animate_Text(HD44780_HandleTypeDef *Display_Handle, uint8_t NumberOfScrolls)
{

	bool TextPresent = false;
	for (int i = 0; i < HD44780_NUM_ELEMENTS; i++) {
		if (Display_Handle->Text[i] != 0){
			TextPresent = true;
			break;
		}
	}

	if (TextPresent) {
		int BytesPerRow = 80/HD44780_NUM_ROWS;
		int NumberOfShifts = (NumberOfScrolls * BytesPerRow);
		for (int i = 0; i < NumberOfShifts; i++) {
			HD44780_Send_Command(Display_Handle, 0x1C, false);
			HAL_Delay(100); /* Scroll speed */
		}
	}
}
