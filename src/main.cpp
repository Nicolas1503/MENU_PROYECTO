#include <Arduino.h>
#include <EEPROM.h>
#include "button.hpp"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define MAXITEMS 15								//Cantidad de posiciones en el display
#define ROWNUM 2
#define COLNUM 16

#define MAINMENU_NUM 3							//Cantidad de chars en el mainmenu
#define AJUSTES_NUM 6							//Cantidad de chars en el menu ajustes
#define MEDICION_NUM 2							//Cantidad de chars en el menu medicion
#define HELICE_NUM 3

// Use with SetEEPROMValueF / GetEEPROMValueF
#define A_ELISE 0
#define B_ELISE 4
#define PERIODO 8

// Use with SetEEPROMValueB / GetEEPROMValueB
#define DAY 12
#define MONTH 13
#define YEAR 14
#define HOUR 15
#define MINUTES 16
#define RESETNUM 17
/*Matriz de dos dimensiones para definir los menues*/
const char menu[MAINMENU_NUM][MAXITEMS] = {"Ajustes","Medicion","Ult. Medidas"};
const char ajustes[AJUSTES_NUM][MAXITEMS] = {"Cfg. Helices","Cfg. Periodo","Ref. Lugar","Cfg. Date","Buzzer","Atras"};
const char medicion[MEDICION_NUM][MAXITEMS] = {"Inicio","Atras"};
const char helice[HELICE_NUM][MAXITEMS] = {"Valor A","Valor B", "Atras"};

/*Definicion de menues, submenues y acciones*/
typedef enum{
	OUT,
    AJUSTES,
	MEDICION,
	ULT_MEDIDAS,
	CFG_HELICES,
	CFG_PERIODO,
	REF_LUGAR,
	CFG_DATE,
	BUZZER,
	ATRAS_AJUSTES,
	INICIO_MEDICION,
	ATRAS_MEDICION,
	TOMAR_MEDICION,
	TOMAR_FECHA_HORA,
	TOMAR_PERIODO,
	VALOR_A,
	VALOR_B,
	ATRAS_HELICE
}Menu_e;

/*Deficinicion de ubicacion dentro del menu*/
typedef enum{
	MAIN,
	AJUSTES_SUBMENU,
	MEDICION_SUBMENU,
	HELICE_SUBMENU
}Menu_state_e;

/*No se usa todavia VER*/
typedef enum{
    IN = -1,
    ROW_1 = 0,
    ROW_2 = 1,
    OUT_ROW = 2
}row_t;

/*Definicion de lo que sale de la funcion que chequea los botones*/
typedef enum{
	DONTMOVE = 0,
	UP = 1,
	DOWN = 2,
	ENTER = 3
}move_t;

/*Definicion de pines*/
const bool pullup = true;
const int up_button = 2;
const int down_button = 3;
const int enter_button = 4;
move_t buttonProcess = DONTMOVE;

/*Botones como pull up*/
Button Up(up_button,pullup);
Button Down(down_button,pullup);
Button Enter(enter_button,pullup);

Menu_e estado_actual;
Menu_e estado_anterior;
Menu_state_e menu_submenu_state;
row_t ROW_STATUS;

// EEPROM wrappers
void SetEEPROMValueB(int address, uint8_t value);
void SetEEPROMValueF(int address, float value);
void UpdateEEPROMValueB(int address, uint8_t value);
void UpdateEEPROMValueF(int address, float value);
uint8_t GetEEPROMValueB(int address);
float GetEEPROMValueF(int address);

move_t CheckButton(void);

// LCD wrappers
bool lcd_UpdateCursor(Menu_e Menu, int row, int col);
void lcd_ClearOneLine(int row);
void lcd_ClearCursor(int row);
void lcd_DisplayMenu(Menu_e Menu, Menu_state_e menu_submenu_state);
void lcd_PrintCursor(Menu_state_e menu_submenu_state, uint8_t start, uint8_t count, uint8_t cursorPosition);
void lcd_setSpaces(uint8_t day, uint8_t month, uint8_t year, uint8_t hour, uint8_t minutes);
void lcd_setValueB(uint8_t value);

bool StateMachine_Control(Menu_e Menu, Menu_state_e menu_submenu_state);

LiquidCrystal_I2C lcd(0x27,16,2);

void setup()
{
	lcd.init();                     		//Inicializo el LCD 
	lcd.backlight();
	Serial.begin(9600);

	estado_actual = AJUSTES;				//Estado actual y anterior en ajustes
	estado_anterior = AJUSTES;
	menu_submenu_state = MAIN;				//Menu actual esta en el principal

	
	lcd.clear();
	lcd.setCursor(0,ROW_STATUS);
	lcd.print(">");
	lcd_DisplayMenu(estado_actual, menu_submenu_state);			//Se muestra en el display lo que se declaro
	
	//This section is for the first time running the microcontroller without values stored on the memory
	uint8_t reset = GetEEPROMValueB(RESETNUM);
	if (reset != 1){
		SetEEPROMValueF(A_ELISE,0);
		SetEEPROMValueF(B_ELISE,0);
		SetEEPROMValueF(PERIODO,0);
		SetEEPROMValueB(RESETNUM,1);
		SetEEPROMValueB(DAY,0);
		SetEEPROMValueB(MONTH,0);
		SetEEPROMValueB(YEAR,0);
		SetEEPROMValueB(HOUR,0);
		SetEEPROMValueB(MINUTES,0);
	}
}

void loop(void)
{
	bool ret = 0;							//Si el booleano es 0 no se hace nada
	while(true)
	{
		ret = lcd_UpdateCursor(estado_actual,ROWNUM,COLNUM);			//Si se apreto algun boton se actualiza la pantalla (actualiza los estados)
		ret |= StateMachine_Control(estado_actual,menu_submenu_state);
		if (ret == 1){
			lcd_DisplayMenu(estado_actual,menu_submenu_state);
		}
		
	}
}


uint8_t GetEEPROMValueB(int address)
{
    return EEPROM.read(address);
}

float GetEEPROMValueF(int address)
{
    float val;
    return EEPROM.get(address, val);
}

void SetEEPROMValueB(int address, uint8_t value)
{
    EEPROM.write(address, value);
}

void SetEEPROMValueF(int address, float value)
{
    EEPROM.put(address, value);
}

void UpdateEEPROMValueB(int address, uint8_t value)
{
	EEPROM.update(address,value);
}

void UpdateEEPROMValueF(int address, float value)
{
	EEPROM.update(address,value);
}

/*Funcion que chequea que boton esta presionado*/
move_t CheckButton(void)
{
    if (Up.check() == LOW)
    {
      return UP;
    }
    else if (Down.check() == LOW)
    {
      return DOWN;
    }
    else if (Enter.check() == LOW)
    {
      return ENTER;
    }
    else
    {
        // DONT MOVE
    }
  return DONTMOVE;					//Siempre retorna un 0 (no hace nada), excepto que se haya apretado un boton
}

bool lcd_UpdateCursor(Menu_e Menu, int row, int col)	//Dentro de esta funcion esta el chekbutton
{
	static move_t lastButtonProcess = DONTMOVE;			//Variables que quedan fijas segun el ultimo llamado de la funcion
	static Menu_e firstMenu = AJUSTES;
	static Menu_e lastMenu = ULT_MEDIDAS;
	static Menu_state_e lastMenuState = MAIN;

	buttonProcess = CheckButton();						//Aqui se recibe, UP, DOWN, ENTER O DONT MOVE

	if (buttonProcess != DONTMOVE)						//Si es distinto de DONTMOVE se hace algo 
	{
		lastButtonProcess = buttonProcess;
		if (buttonProcess == DOWN){						//Si el boton fue DOWN 
			if(estado_actual != lastMenu)				//Si el estado actual es distinto de el ultimo item del menu se suma uno
			{
				estado_actual = estado_actual + 1;
			}
		}
		else if (buttonProcess == UP){					//Si el boton fue UP 
			if(estado_actual != firstMenu)				//Si el estado actual es distinto del primer item del menu se resta uno
			{
				estado_actual = estado_actual - 1;
			}
		}
		else if (buttonProcess == ENTER)				//Si se aprieta el boton enter se chequea en que estado esta
		{
			if (lastButtonProcess == DOWN || lastButtonProcess == UP)
			{
				lcd.clear();
			}
			switch(estado_actual)
			{
				case AJUSTES:								
				{
					menu_submenu_state = AJUSTES_SUBMENU; 	//Se pasa del MAIMENU al menu de ajustes
					estado_actual = CFG_HELICES;			//Primer estado de este submenu
				}
				break;
				case ATRAS_AJUSTES:
				{
					menu_submenu_state = MAIN;				//Se pasa del menu de ajustes al MAINMENU
					estado_actual = AJUSTES;				//Primer estado de este menu 
				}
				break;
				case MEDICION:
				{
					menu_submenu_state = MEDICION_SUBMENU;		//Se pasa del MAIMENU al menu de medicion
					estado_actual = INICIO_MEDICION;			//Primer estado de este menu 			
				}
				break;

				case INICIO_MEDICION:
				{
					menu_submenu_state = MEDICION_SUBMENU;		//Cuando se aprieta enter en inicio medicion se pasa al estado tomar medicion
					estado_actual = TOMAR_MEDICION;
				}
				break;

				case ATRAS_MEDICION:
				{
					menu_submenu_state = MAIN;					//Se pasa del menu medicion al MAINMENU
					estado_actual = AJUSTES;					//Primer estado de este menu 
				}
				break;
				
				case CFG_DATE:
				{
					menu_submenu_state = AJUSTES_SUBMENU;
					estado_actual = TOMAR_FECHA_HORA;			//Cuando se aprieta enter en config. fecha/hora se pasa al estado cambiar fecha/hora
				}
				break;

				case CFG_PERIODO:
				{
					menu_submenu_state = AJUSTES_SUBMENU;
					estado_actual = TOMAR_PERIODO;				//Cuando se aprieta enter en config. periodo se pasa al estado config. periodo
				}
				break;

				case CFG_HELICES:
				{
					menu_submenu_state = HELICE_SUBMENU;
					estado_actual = VALOR_A;				//Cuando se aprieta enter en config. periodo se pasa al estado config. periodo
				}
				break;

				case ATRAS_HELICE:
				{
					menu_submenu_state = AJUSTES_SUBMENU;
					estado_actual = CFG_HELICES;				//Cuando se aprieta enter en config. periodo se pasa al estado config. periodo
				}
				break;
			}
		}

		/*Definicion de primeros y ultimos elementos en cada menu*/
		switch(menu_submenu_state)
		{
		case MAIN: 
			firstMenu = AJUSTES;
			lastMenu = ULT_MEDIDAS; 
			break;
		case AJUSTES_SUBMENU: 
			firstMenu = CFG_HELICES;
			lastMenu = ATRAS_AJUSTES; 
			break;
		case MEDICION_SUBMENU:
			firstMenu = INICIO_MEDICION;
			lastMenu = ATRAS_MEDICION;
			break;
		case HELICE_SUBMENU:
			firstMenu = VALOR_A;
			lastMenu = ATRAS_HELICE;
			break;
		}
		

		Serial.println("Cambio de estado");
		Serial.println(estado_actual);
		return 1;
	}
	return 0;
}

/*Se usa?*/
void lcd_ClearOneLine(int row)
{
	for(uint8_t i=0; i < COLNUM ; i++)
	{
		lcd.setCursor(i,row);
		lcd.print(" ");
	}
}

/*Se usa?*/
void lcd_ClearCursor(int row)
{
	lcd.setCursor(0,row);
	lcd.print(" ");
}

void lcd_DisplayMenu(Menu_e Menu, Menu_state_e menu_submenu_state)					//Funcion que imprime las opciones en el display	
{
	switch(Menu)
	{
		case OUT:											//Se define el tope en el enum para no entrar en estados que no existen
			Menu = AJUSTES;
		break;

		case AJUSTES:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,0);		//Donde comienza a mostrar, cuantos voy a mostrar, donde esta el cursor
		}
		break;

		case MEDICION:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,1);
		}
		break;

		case ULT_MEDIDAS:
		{
			lcd_PrintCursor(menu_submenu_state,2,1,0);
		}
		break;

		case CFG_HELICES:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,0);
		}
		break;

		case CFG_PERIODO:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,1);
		}
		break;
		
		case REF_LUGAR:
		{
			lcd_PrintCursor(menu_submenu_state,2,2,0);
		}
		break;

		case CFG_DATE:
		{
			lcd_PrintCursor(menu_submenu_state,2,2,1);
		}
		break;

		case BUZZER:
		{
			lcd_PrintCursor(menu_submenu_state,4,2,0);
		}
		break;

		case ATRAS_AJUSTES:
		{
			lcd_PrintCursor(menu_submenu_state,4,2,1);
		}
		break;

		case INICIO_MEDICION:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,0);
		}
		break;

		case ATRAS_MEDICION:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,1);
		}
		break;

		case TOMAR_MEDICION:
		{
			// no hacer nada
		}
		break;

		case VALOR_A:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,0);
		}
		break;

		case VALOR_B:
		{
			lcd_PrintCursor(menu_submenu_state,0,2,1);
		}
		break;

		case ATRAS_HELICE:
		{
			lcd_PrintCursor(menu_submenu_state,2,1,0);
		}
		break;

		default:
			Menu = AJUSTES;
		break;
	}
}

/* Funcion para imprimir en pantalla*/
void lcd_PrintCursor(Menu_state_e menu_submenu_state, uint8_t start, uint8_t count, uint8_t cursorPosition) //Estados, donde comienza en el array que corresponde
{
	lcd.clear();
	lcd.setCursor(0,cursorPosition);
	lcd.print(">");
	bool cursor = 0;
	if (count <= ROWNUM){
		for (uint8_t i=start; i<count+start; i++)						//Cuenta para mostrar los menu en la pantalla
		{
			lcd.setCursor(1,cursor);									//Se coloca el cursor en la posicion 1 de columna y la fila 0
			cursor = !cursor;											//Se coloca el cursor en la fila 1 en la siguiente iteracion

			if (menu_submenu_state == MAIN){
				lcd.print(menu[i]);										//Muestra solo el array del menu principal
			}
			else if (menu_submenu_state == AJUSTES_SUBMENU){
				lcd.print(ajustes[i]);									//Muestra solo el array del submenu ajustes
			}
			else if (menu_submenu_state == MEDICION_SUBMENU){
				lcd.print(medicion[i]);									//Muestra solo el array el submenu medicion
			}
			else if (menu_submenu_state == HELICE_SUBMENU){
				lcd.print(helice[i]);									
			}
		}
	}
}

/*Funcion que se encarga de realizar las acciones dentro de cada menu*/
bool StateMachine_Control(Menu_e Menu, Menu_state_e menu_submenu_state)
{
	switch(Menu)
	{
		case TOMAR_MEDICION:
		{

			move_t buttonProcess = DONTMOVE;
			bool buttonOut = 1;
			while(buttonProcess != ENTER)								//Me aparto del codigo principal mientras el boton sea distinto de enter
			{
				float dotcount = 1000.0;
				uint8_t dotiter = 0;

				/* Muestro el periodo en segundos*/
				lcd.clear();
				lcd.setCursor(0,0);
				lcd.print("T = ");
				float periodo = GetEEPROMValueF(PERIODO);
				lcd.setCursor(4,0);
				lcd.print(periodo/1000); //Convierto de milisegundos a segundos
				lcd.print("sec");

				unsigned long lastmillis = millis();
				unsigned long timedone = 0; 		//Se le asigna el tiempo transcurrido dentro del while loop siguiente

				while(buttonOut)
				{
					if (CheckButton() != ENTER)
					{
						timedone = millis() - lastmillis;
						if(timedone >= periodo)
						{
							buttonOut = 0;
						}
					}
					else{
						buttonOut = 0;
					}
					
					/* 	Muestra un punto cada 1 segundo transcurrido
						Esto es solo un ejemplo */
					if(timedone == dotcount)
					{
						lcd.setCursor(dotiter,1);
						lcd.print(".");
						dotcount = dotcount + 1000.0;
						if(dotiter < 16)
						{
							dotiter++;
						}
						else{
							lcd_ClearOneLine(1);
							dotiter = 0;
						}
					}
				}
				buttonProcess = ENTER;
			}
			estado_actual = INICIO_MEDICION;
			return 1;
		}
		break;

		/*Funcion para configurar la fecha y hora y guardarla en la EEPROM*/
		case TOMAR_FECHA_HORA:
		{
			move_t buttonProcess = DONTMOVE;
			uint8_t day = GetEEPROMValueB(DAY);
			uint8_t month = GetEEPROMValueB(MONTH);
			uint8_t hour = GetEEPROMValueB(HOUR);
			uint8_t minutes = GetEEPROMValueB(MINUTES);
			uint8_t year = GetEEPROMValueB(YEAR);
			uint8_t enterCount = 0;
			bool outFechaHora = 1;
			bool setDay = 1;
			bool setMonth = 1;
			bool setHour = 1;
			bool setMinutes = 1;
			bool setYear = 1;
			bool buttonOut = 1;
			while(outFechaHora){
				lcd_setSpaces(day,month,year,hour,minutes);
				while(setDay)
				{
					buttonProcess = CheckButton();
					switch(buttonProcess)
					{
						case DONTMOVE:
						break;
						case UP:
						{
							if(day < 31){
								day++;					//Si se oprime el boton hacia arriba se incrementa en uno
							}
							lcd.setCursor(0,1);
							if (day < 10)				//Se imprime en la pantalla el valor, se pone un 0 adelante en el caso de que sea <10
							{
								lcd.print("0");
								lcd.print(day);
							}
							else{
								lcd.print(day);
							}
						}
						break;

						case DOWN:
						{
							if(day > 1){
								day--;
							}
							lcd.setCursor(0,1);
							if (day < 10)
							{
								lcd.print("0");
								lcd.print(day);
							}
							else{
								lcd.print(day);
							}
						}
						break;

						case ENTER:
						{
							setDay = 0;
							enterCount++;
						}
						break;
					}
				}

				while(setMonth)
				{
					buttonProcess = CheckButton();
					switch(buttonProcess)
					{
						case DONTMOVE:
						break;
						case UP:
						{
							if(month < 12){
								month++;
							}
							lcd.setCursor(3,1);
							if (month < 10)
							{
								lcd.print("0");
								lcd.print(month);
							}
							else{
								lcd.print(month);
							}
						}
						break;

						case DOWN:
						{
							if(month > 1){
								month--;
							}
							lcd.setCursor(3,1);
							if (month < 10)
							{
								lcd.print("0");
								lcd.print(month);
							}
							else{
								lcd.print(month);
							}
						}
						break;

						case ENTER:
						{
							setMonth = 0;
							enterCount++;
						}
						break;
					}
				}

				while(setYear)
				{
					buttonProcess = CheckButton();
					switch(buttonProcess)
					{
						case DONTMOVE:
						break;
						case UP:
						{
							if(year < 50){
								year++;
							}
							lcd.setCursor(6,1);
							if (year < 10)
							{
								lcd.print("0");
								lcd.print(year);
							}
							else{
								lcd.print(year);
							}
						}
						break;

						case DOWN:
						{
							if(year > 1){
								year--;
							}
							lcd.setCursor(6,1);
							if (year < 10)
							{
								lcd.print("0");
								lcd.print(year);
							}
							else{
								lcd.print(year);
							}
						}
						break;

						case ENTER:
						{
							setYear = 0;
							enterCount++;
						}
						break;
					}
				}

				while(setHour)
				{
					buttonProcess = CheckButton();
					switch(buttonProcess)
					{
						case DONTMOVE:
						break;
						case UP:
						{
							if(hour < 23){
								hour++;
							}
							lcd.setCursor(10,1);
							if (hour < 10)
							{
								lcd.print("0");
								lcd.print(hour);
							}
							else{
								lcd.print(hour);
							}
						}
						break;

						case DOWN:
						{
							if(hour > 0){
								hour--;
							}
							lcd.setCursor(10,1);
							if (hour < 10)
							{
								lcd.print("0");
								lcd.print(hour);
							}
							else{
								lcd.print(hour);
							}
						}
						break;

						case ENTER:
						{
							setHour = 0;
							enterCount++;
						}
						break;
					}
				}

				while(setMinutes)
				{
					buttonProcess = CheckButton();
					switch(buttonProcess)
					{
						case DONTMOVE:
						break;
						case UP:
						{
							if(minutes < 59){
								minutes++;
							}
							lcd.setCursor(13,1);
							if (minutes < 10)
							{
								lcd.print("0");
								lcd.print(minutes);
							}
							else{
								lcd.print(minutes);
							}
						}
						break;

						case DOWN:
						{
							if(minutes > 0){
								minutes--;
							}
							lcd.setCursor(13,1);
							if (minutes < 10)
							{
								lcd.print("0");
								lcd.print(minutes);
							}
							else{
								lcd.print(minutes);
							}
						}
						break;

						case ENTER:
						{
							setMinutes = 0;
							enterCount++;
						}
						break;
					}
				}

				UpdateEEPROMValueB(DAY,day);
				UpdateEEPROMValueB(MONTH,month);
				UpdateEEPROMValueB(YEAR,year);
				UpdateEEPROMValueB(HOUR,hour);
				UpdateEEPROMValueB(MINUTES,minutes);
				outFechaHora = 0;
			}
			estado_actual = CFG_DATE;
			return 1;
		}
		break;

		case TOMAR_PERIODO:
		{
			bool outPeriodo = 1;
			move_t buttonProcess = DONTMOVE;
			float periodo = GetEEPROMValueF(PERIODO);
			lcd.clear();
			lcd.setCursor(0,0);
			lcd.print("Periodo");
			lcd.setCursor(0,1);
			lcd.print(periodo);
			lcd.print(" mSeg");
			while(outPeriodo)
			{
				buttonProcess = CheckButton();
				switch(buttonProcess)
				{
					case DONTMOVE:break;
					case ENTER:
					{
						outPeriodo = 0;
						estado_actual = CFG_PERIODO;
					}
					break;

					case UP:
					{
						periodo = periodo + 100;			//Si se oprime el boton hacia arriba se incrementa el periodo 
						lcd.setCursor(0,1);
						lcd.print(periodo);
						lcd.print("mSeg");	
					}
					break;

					case DOWN:
					{
						if((periodo - 100) != 0){			//Si se oprime el boton hacia arriba se decrementa el periodo 
							periodo = periodo - 100;
						}
						lcd.setCursor(0,1);
						lcd.print(periodo);
						lcd.print("mSeg");
					}
					break;
				}
			}
			SetEEPROMValueF(PERIODO,periodo);	
		}
		break;
	}
	return 0;
} 

void lcd_setSpaces(uint8_t day, uint8_t month, uint8_t year, uint8_t hour, uint8_t minutes)
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Cfg. fecha/hora");
	lcd.setCursor(0,1);
	lcd_setValueB(day);
	lcd.setCursor(2,1);
	lcd.print("/");
	lcd.setCursor(3,1);
	lcd_setValueB(month);
	lcd.setCursor(5,1);
	lcd.print("/");
	lcd.setCursor(6,1);
	lcd_setValueB(year);
	lcd.setCursor(10,1);
	lcd_setValueB(hour);
	lcd.setCursor(12,1);
	lcd.print(":");
	lcd.setCursor(13,1);
	lcd_setValueB(minutes);
}

void lcd_setValueB(uint8_t value)
{
	if (value < 10)
	{
		lcd.print("0");
		lcd.print(value);
	}
	else
	{
		lcd.print(value);
	}
}
