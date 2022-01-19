 <h1 align="center"> HD44780 I2C Driver</h1>

 # Driver Overview #
 This driver is targeted for STM32 boards and HD44780-based I2C display modules. Currently works on 16x2 display such as the one shown below:
 <p align="center">
 <img src="https://user-images.githubusercontent.com/79535234/150162473-e139d7c3-d8eb-4695-877e-66c1ef45de0c.png" width="300" />
 <p>

 **Main featues:**
 * Standard LCD control (write, get cursor position, set cursor position, animate text, ...). Fully explained in [Driver Summary](#driver-summary) section.
 * Busy flag check and address counter read between commands.
 * User defined error handler if I2C communication between STM32 and HD44780 times out.
  
 # Hardware/Software Requirements #
 * LCD display with HD44780 driver.
 * [PCF8574-based 8-bit I/O to I2C module](https://www.amazon.com/1602LCD-Display-Serial-Interface-Arduino/dp/B01MXGXPKU/ref=asc_df_B01MXGXPKU/?tag=hyprod-20&linkCode=df0&hvadid=167146065113&hvpos=&hvnetw=g&hvrand=358405417949042193&hvpone=&hvptwo=&hvqmt=&hvdev=c&hvdvcmdl=&hvlocint=&hvlocphy=9004331&hvtargid=pla-305845701989&psc=1/) that operates in four bit mode.
 * STM32-based board and 5V supply for LCD.
 * STM32CubeIDE (HAL for I2C communication).
  
 # Porting #
 1. Add HD44780.c and HD44780.h into STM32CubeIDE project.
 2. Configure I2C pins using CubeMX and HAL-generated code.
 3. `#include "HD44780.h"`
 4. Define `HD44780_NUM_ROWS`, `HD44780_NUM_COLS`, and `HW061_I2C_ADDR` in `HD44780.h`.
 4. In `main.c` create a HD44780_HandleTypeDef object and intialize display with `HD44780_Init()`.

**Initialization and Print Example**
   
`HD44780.h`
```c
#define HD44780_NUM_ROWS 		2
#define HD44780_NUM_COLS 		16
#define HW061_I2C_ADDR 			(0x27<<1) /* PCF8574 (pg. 13) */
```
   
`main.c`
```c
#include "main.h"
#include "HD44780.h"

I2C_HandleTypeDef hi2c1;              /* Automatically generated by HAL */
HD44780_HandleTypeDef HD44780_Handle; /* Create HD44780_HandleTypeDef object */
   
void SystemClock_Config(void);        /* Automatically generated by HAL */
static void MX_GPIO_Init(void);       /* Automatically generated by HAL */
static void MX_I2C1_Init(void);       /* Automatically generated by HAL */

int main(void)
{
  HAL_Init();                         /* Automatically generated by HAL */
  SystemClock_Config();               /* Automatically generated by HAL */
  MX_GPIO_Init();                     /* Automatically generated by HAL */
  MX_I2C1_Init();                     /* Automatically generated by HAL */

  if (HD44780_Init(&hi2c1, &HD44780_Handle))              /* Returns true if initialization successful */
  {
	  HD44780_Print(&HD44780_Handle, "Hello World Row1");   /* Prints text on first row */
	  HD44780_Set_Cursor_Position(&HD44780_Handle, 1, 0);   /* Moves cursor to second row */
	  HD44780_Print(&HD44780_Handle, "Hello World Row2");   /* Prints text on second row */
  }
}
```
	 
# Driver Summary #
```c
bool HD44780_Init(I2C_HandleTypeDef *I2C_Handle, HD44780_HandleTypeDef *Display_Handle)
```
<details>
<summary>Description</summary>
<p>Establishes I2C communication between the STM32 and HD44780 display. Initializes the display through a software reset and places cursor at (0,0) position.
	
### Parameters: ###
* **I2C_Handle** -- pointer to the I2C handle created by HAL
* **Display_Handle** -- pointer to HD44780_HandleTypeDef object that user creates
	 
### Returns: ###
* **true** -- if initialization successful.</p>
	
### Example Call ###
```c
I2C_HandleTypeDef hi2c1; /* Automatically generated by HAL */
HD44780_HandleTypeDef MyDisplay; /* Create HD44780_HandleTypeDef object */
HD44780_Init(&hi2c1, &MyDisplay);
```
</details>
<br>
<br>
	 
```c
void HD44780_Print(HD44780_HandleTypeDef *Display_Handle, const char *str)
```
<details>
<summary>Description</summary>
<p>Prints text to the LCD starting at current cursor position. Automatically goes to new row when there's no more display room on the 1st row. Stops printing and sets cursor to (0,0) if there's no more display room.
	
### Parameters: ###
* **Display_Handle** -- pointer to HD44780_HandleTypeDef object that user creates
* **str** -- string to print
	
### Example Call ###
```c
HD44780_Print(&MyDisplay, "Hello World!");
```
</details>
<br>
<br>
	 
```c
void HD44780_Transmit_Command(HD44780_HandleTypeDef *Display_Handle, HD44780_User_Command_List UserCommand)
```
<details>
<summary>Description</summary>
<p>Sends pre-defined commands to the display.
	
### Parameters: ###
* **Display_Handle** -- pointer to HD44780_HandleTypeDef object that user creates
* **UserCommand** -- command defined in HD44780_User_Command_List enum. This list includes:
	* CLEAR_DISPLAY -- clears contents of display and returns cursor to (0,0)
	* RETURN_HOME -- returns cursor to (0,0)
	* DISPLAY_OFF -- turns backlight off
	* CURSOR_ON -- turns cursor on
	* CURSOR_OFF -- turns cursor off
	* CURSOR_BLINK -- blinks the cursor
	* CURSOR_UNBLINK -- stops blinking the cursor
	* DISPLAY_ON -- turns the backlight on
	
### Example Call ###
```c
HD44780_Transmit_Command(&MyDisplay, CURSOR_ON);
```
</details>
<br>
<br>
	 
```c
void HD44780_Set_Cursor_Position(HD44780_HandleTypeDef *Display_Handle, uint8_t row, uint8_t column)
```
<details>
<summary>Description</summary>
<p>Sets the cursor position. Only runs if row is between 0 and HD44780_NUM_ROWS and if column is between 0 and HD44780_NUM_COLS.
	
### Parameters: ###
* **Display_Handle** -- pointer to HD44780_HandleTypeDef object that user creates
* **row** -- 0-indexed row
* **column** -- 0-indexed column
	
### Example Call ###
```c
HD44780_Set_Cursor_Position(&MyDisplay, 0, 5); /* First row, 6th column */
```
</details>
<br>
<br>

```c
void HD44780_Animate_Text(HD44780_HandleTypeDef *Display_Handle, uint8_t NumberOfScrolls)
```
<details>
<summary>Description</summary>
<p>Scrolls all of the LCD's text from left to right. Only executes if there is currently text on the display.
	
### Parameters: ###
* **Display_Handle** -- pointer to HD44780_HandleTypeDef object that user creates
* **NumberOfScrolls** -- number of full sweeps across the entire display to perform. Each sweep takes ~4 seconds on a 16x2 display. 
	
### Example Call ###
```c
HD44780_Animate_Text(&MyDisplay, 2);
```
</details>
<br>
<br>
	 

	 
