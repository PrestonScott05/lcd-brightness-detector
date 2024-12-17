#include "pic24_all.h"
#include <stdio.h>


#define RS_HIGH()        _LATB0 = 1
#define RS_LOW()         _LATB0 = 0
#define CONFIG_RS()      CONFIG_RB0_AS_DIG_OUTPUT()

#define RW_HIGH()        _LATB9 = 1
#define RW_LOW()         _LATB9 = 0
#define CONFIG_RW()      CONFIG_RB9_AS_DIG_OUTPUT()

#define E_HIGH()         _LATB13 = 1
#define E_LOW()          _LATB13 = 0
#define CONFIG_E()       CONFIG_RB13_AS_DIG_OUTPUT()



//lcd pins used
#define LCD4O          _LATB4
#define LCD5O          _LATB5
#define LCD6O          _LATB6
#define LCD7O          _LATB1
#define LCD7I          _RB1

#define GLED            _RA4 // define GLED as LATA1


#define CONFIG_LCD4_AS_INPUT() CONFIG_RB4_AS_DIG_INPUT() 
#define CONFIG_LCD5_AS_INPUT() CONFIG_RB5_AS_DIG_INPUT()
#define CONFIG_LCD6_AS_INPUT() CONFIG_RB6_AS_DIG_INPUT()
#define CONFIG_LCD7_AS_INPUT() CONFIG_RB1_AS_DIG_INPUT()

#define CONFIG_LCD4_AS_OUTPUT() CONFIG_RB4_AS_DIG_OUTPUT()
#define CONFIG_LCD5_AS_OUTPUT() CONFIG_RB5_AS_DIG_OUTPUT()
#define CONFIG_LCD6_AS_OUTPUT() CONFIG_RB6_AS_DIG_OUTPUT()
#define CONFIG_LCD7_AS_OUTPUT() CONFIG_RB1_AS_DIG_OUTPUT()

#define GET_BUSY_FLAG()  LCD7I

#define CONFIG_SLAVE_ENABLE() CONFIG_RB3_AS_DIG_OUTPUT()
#define SLAVE_ENABLE()        _LATB3 = 0
#define SLAVE_DISABLE()       _LATB3 = 1

#define VREF 3.3 // Reference voltage for ADC

volatile uint16_t adc_val = 0; // global adc value

#pragma config FWDTEN = OFF // disable watchdog timer interrupt

//timer 1 interrupt
void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void) {
    IFS0bits.T1IF = 0; // clear interrupt flag
    ADC1BUF0 = convertADC1(); // convert ADC to ADC 1 register 
    CLRWDT();
}

void initTimer1(void) {
     T1CON = 0x00;              // Stop and reset Timer 1 control register
    TMR1 = 0x00;               // Clear Timer 1 counter
    PR1 = msToU16Ticks(50, getTimerPrescale(T1CONbits)); // Set period for 50 ms
    T1CONbits.TCKPS = 0b01;    // Set prescaler (1:8)
    T1CONbits.TCS = 0;         // Use internal clock (Fcy)
    IFS0bits.T1IF = 0;         // Clear Timer 1 interrupt flag
    IEC0bits.T1IE = 1;         // Enable Timer 1 interrupt
    IPC0bits.T1IP = 4;         // Set interrupt priority (medium)
    T1CONbits.TON = 1;         // Start Timer 1
}

void configSPI1(void) {
  //spi clock = 40MHz/1*4 = 40MHz/4 = 10MHz
  SPI1CON1 = SEC_PRESCAL_1_1 |     //1:1 secondary prescale
             PRI_PRESCAL_4_1 |     //4:1 primary prescale
             CLK_POL_ACTIVE_HIGH | //clock active high (CKP = 0)
             SPI_CKE_ON          | //out changes active to inactive (CKE=1)
             SPI_MODE8_ON        | //8-bit mode
             MASTER_ENABLE_ON;     //master mode
    #if (defined(__dsPIC33E__) || defined(__PIC24E__))
    //nothing to do here. On this family, the SPI1 port uses dedicated
    //pins for higher speed. The SPI2 port can be used with remappable pins.
    //you may need to add code to disable analog functionality if the SPI ports
    //are on analog-capable pins.
    #else
    CONFIG_SDO1_TO_RP(8);      //use RP6 for SDO
    CONFIG_SCK1OUT_TO_RP(7);   //use RP7 for SCLK
    #endif

    SPI1STATbits.SPIEN = 1;  //enable SPI mode
}

//ADC configuration
void initADC1(void) {
    CONFIG_AN1_AS_ANALOG(); 
    configADC1_ManualCH0(ADC_CH0_POS_SAMPLEA_AN1, 31, 1);
}

uint16_t readADC() {
    return convertADC1();
}

//display brightness and voltrage to lcd:
void displayBrightnessAndVoltage(uint8_t brightness, float voltage) {
    char buffer[16];
    
    sprintf(buffer,"%.2f", (double)voltage);
    writeLCD(0x01, 0, 0, 1); // clear display
    writeLCD(0x80, 0, 0, 1); //move cursor to first line
	
    //display to screen
	  outStringLCD("Voltage: ");
    outStringLCD(buffer);
    
    sprintf(buffer,"%03d", brightness);
    writeLCD(0xC0, 0, 0, 1); //move cursor to second line
    //display to screen
	  outStringLCD("Brightness: ");
    outStringLCD(buffer);
}

void configDAC() {
  CONFIG_SLAVE_ENABLE();       //chip select for DAC
  SLAVE_DISABLE();             //disable the chip select
}

void writeDAC (uint8_t dacval) {
  SLAVE_ENABLE();                 //assert Chipselect line to DAC
  ioMasterSPI1(0b00001001);      //control byte that enables DAC A
  ioMasterSPI1(dacval);          //write DAC value
  SLAVE_DISABLE();
}

void configBusAsOutLCD(void) {
  RW_LOW();                  //RW=0 to stop LCD from driving pins
  CONFIG_LCD4_AS_OUTPUT();   //D4
  CONFIG_LCD5_AS_OUTPUT();   //D5
  CONFIG_LCD6_AS_OUTPUT();   //D6
  CONFIG_LCD7_AS_OUTPUT();   //D7
}
//Configure 4-bit data bus for input
void configBusAsInLCD(void) {
  CONFIG_LCD4_AS_INPUT();   //D4
  CONFIG_LCD5_AS_INPUT();   //D5
  CONFIG_LCD6_AS_INPUT();   //D6
  CONFIG_LCD7_AS_INPUT();   //D7
  RW_HIGH();                   // R/W = 1, for read
}

//Output lower 4-bits of u8_c to LCD data lines
void outputToBusLCD(uint8_t u8_c) {
  LCD4O = u8_c & 0x01;          //D4
  LCD5O = (u8_c >> 1)& 0x01;    //D5
  LCD6O = (u8_c >> 2)& 0x01;    //D6
  LCD7O = (u8_c >> 3)& 0x01;    //D7
}
//Configure the control lines for the LCD
void configControlLCD(void) {
  CONFIG_RS();     //RS
  CONFIG_RW();     //RW
  CONFIG_E();      //E
  RW_LOW();
  E_LOW();
  RS_LOW();
}
//Pulse the E clock, 1 us delay around edges for
//setup/hold times
void pulseE(void) {
  DELAY_US(1);
  E_HIGH();
  DELAY_US(1);
  E_LOW();
  DELAY_US(1);
}
void writeLCD(uint8_t u8_Cmd, uint8_t u8_DataFlag,
              uint8_t u8_CheckBusy, uint8_t u8_Send8Bits) {

  uint8_t u8_BusyFlag;
  uint8_t u8_wdtState;
  if (u8_CheckBusy) {
    RS_LOW();            //RS = 0 to check busy
    // check busy
    configBusAsInLCD();  //set data pins all inputs
    u8_wdtState = _SWDTEN;  //save WDT enable state
    CLRWDT();  			   //clear the WDT timer
    _SWDTEN = 1;            //enable WDT to escape infinite wait
    do {
      E_HIGH();
      DELAY_US(1);  // read upper 4 bits
      u8_BusyFlag = GET_BUSY_FLAG();
      E_LOW();
      DELAY_US(1);
      pulseE();              //pulse again for lower 4-bits
    } while (u8_BusyFlag);
    _SWDTEN = u8_wdtState;   //restore WDT enable state
  } else {
    DELAY_MS(10); // don't use busy, just delay
  }
  configBusAsOutLCD();

  if (u8_DataFlag) {
    RS_HIGH();
    }   // RS=1, data byte
  else {
    RS_LOW();             // RS=0, command byte
  }

  outputToBusLCD(u8_Cmd >> 4);  // send upper 4 bits
  pulseE();

  if (u8_Send8Bits) {
    outputToBusLCD(u8_Cmd);     // send lower 4 bits
    pulseE();
  }
}

// Initialize the LCD, modify to suit your application and LCD
void initLCD() {
  DELAY_MS(50);          //wait for device to settle
  writeLCD(0x20,0,0,0); // 4 bit interface
  writeLCD(0x28,0,0,1); // 2 line display, 5x7 font
  writeLCD(0x28,0,0,1); // repeat
  writeLCD(0x06,0,0,1); // enable display
  writeLCD(0x0C,0,0,1); // turn display on; cursor, blink is off
  writeLCD(0x01,0,0,1); // clear display, move cursor to home
  DELAY_MS(3);
}


void initRGBLED(void) {
    CONFIG_RA4_AS_DIG_OUTPUT();
    //init with green on

    GLED = 1;

}
// interrupt service routine for timer interrupt

//Output a string to the LCD
void outStringLCD(char *psz_s) {
    while (*psz_s) {
        writeLCD(*psz_s, 1, 1, 1);  // Send each character to LCD
        psz_s++;
    }
}

int main(void) {
    configControlLCD();
    initLCD();
    initRGBLED();
    initADC1();
    configSPI1();
    configDAC();
    initTimer1();

    uint16_t adc_val;
    uint8_t dac_val;
    uint8_t brightness;
    float voltage;
    float measured_voltage_dark = 0.5;
    float measured_voltage_bright = 3.0;
//test for LCD
	  writeLCD(0x01, 0, 0, 1); // clear display
    writeLCD(0x80, 0, 0, 1); //move cursor to first line
	  outStringLCD("hello");
    DELAY_MS(1000);

    while (1) {
        adc_val = readADC();
        dac_val = (adc_val) >> 4 & 0xFF; // convert to 8 bit
        writeDAC(dac_val);

        // calculate brightness and voltage
        voltage = VREF - ((float)adc_val * VREF / 4095.0);
        brightness = (voltage - measured_voltage_dark) / (measured_voltage_bright - measured_voltage_dark) * 255;
        if (brightness > 255) brightness = 255;
        if (brightness < 0) brightness = 0;

        //display to lcd
        displayBrightnessAndVoltage(brightness, voltage);
        
		
      //green led
      if (voltage > 1.7) {
        GLED = 1;
      }
      else {
        GLED = !GLED;
      }

      DELAY_MS(10);
	}
    return 0;
}