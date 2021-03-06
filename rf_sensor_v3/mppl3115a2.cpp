/*
 MPL3115A2 Altitude Sensor Example
 By: A.Weiss, 7/17/2012, changes Nathan Seidle Sept 23rd, 2013
 Ported to ChibiOS by Louise Newberry Aug 14, 2017
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 Hardware Connections (Breakoutboard to Arduino):
 -VCC = 3.3V
 -SDA = A4
 -SCL = A5
 -INT pins can be left unconnected for this demo
 
 Usage:
 -Serial terminal at 9600bps
 -Prints altitude in meters, temperature in degrees C, with 1/16 resolution.
 -software enabled interrupt on new data, ~1Hz with full resolution
 
 During testing, GPS with 9 sattelites reported 5393ft, sensor reported 5360ft (delta of 33ft). Very close!
 
 */

#include "hal.h"
#include "chprintf.h"

#include "board_specific.h"

#define MPL3115A2_TIMEOUT 10

#define STATUS     0x00
#define OUT_P_MSB  0x01
#define OUT_P_CSB  0x02
#define OUT_P_LSB  0x03
#define OUT_T_MSB  0x04
#define OUT_T_LSB  0x05
#define DR_STATUS  0x06
#define OUT_P_DELTA_MSB  0x07
#define OUT_P_DELTA_CSB  0x08
#define OUT_P_DELTA_LSB  0x09
#define OUT_T_DELTA_MSB  0x0A
#define OUT_T_DELTA_LSB  0x0B
#define WHO_AM_I   0x0C
#define F_STATUS   0x0D
#define F_DATA     0x0E
#define F_SETUP    0x0F
#define TIME_DLY   0x10
#define SYSMOD     0x11
#define INT_SOURCE 0x12
#define PT_DATA_CFG 0x13
#define BAR_IN_MSB 0x14
#define BAR_IN_LSB 0x15
#define P_TGT_MSB  0x16
#define P_TGT_LSB  0x17
#define T_TGT      0x18
#define P_WND_MSB  0x19
#define P_WND_LSB  0x1A
#define T_WND      0x1B
#define P_MIN_MSB  0x1C
#define P_MIN_CSB  0x1D
#define P_MIN_LSB  0x1E
#define T_MIN_MSB  0x1F
#define T_MIN_LSB  0x20
#define P_MAX_MSB  0x21
#define P_MAX_CSB  0x22
#define P_MAX_LSB  0x23
#define T_MAX_MSB  0x24
#define T_MAX_LSB  0x25
#define CTRL_REG1  0x26
#define CTRL_REG2  0x27
#define CTRL_REG3  0x28
#define CTRL_REG4  0x29
#define CTRL_REG5  0x2A
#define OFF_P      0x2B
#define OFF_T      0x2C
#define OFF_H      0x2D

#define MPL3115A2_ADDRESS 0x60 // 7-bit I2C address

msg_t IIC_Write(uint8_t regAddr, uint8_t value);
uint8_t IIC_Read(uint8_t regAddr);
void setModeBarometer();
void toggleOneShot(void);
void setModeAltimeter();
void setModeActive();
void setOversampleRate(uint8_t sampleRate);
void enableEventFlags();

bool mppl3115a2_setup(BaseSequentialStream *chp)
{

  if(IIC_Read(WHO_AM_I) == 196) 
    chprintf(chp,"MPL3115A2: online!");
  else
  {
    chprintf(chp,"MPL3115A2: No response - check connections");
    return(false);
  }

  // Configure the sensor
  setModeActive();
  //setModeAltimeter(); // Measure altitude above sea level in meters
  setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa

  setOversampleRate(7); // Set Oversample to the recommended 128
  enableEventFlags(); // Enable all three pressure and temp event flags
  
  return(true);
}

#if 0

//Returns the number of meters above sea level
int readAltitude()
{
  uint8_t msb, csb, lsb;
  toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

  //Wait for PDR bit, indicates we have new pressure data
  int counter = 0;
  while( (IIC_Read(STATUS) & (1<<1)) == 0)
  {
      if(++counter > 100) return(-999); //Error out
  }

  {
  msg_t status = MSG_OK;
  
  uint8_t i2c_rx_data[3];
  uint8_t i2c_tx_data[2];
  
  i2c_tx_data[0] = OUT_P_MSB;
  /* sending */
  i2cAcquireBus(&SENSOR_I2C);
  status = i2cMasterTransmitTimeout(&SENSOR_I2C, MPL3115A2_ADDRESS,
                          i2c_tx_data, 1, i2c_rx_data, 3, MPL3115A2_TIMEOUT);
  i2cReleaseBus(&SENSOR_I2C);
  
  msb = i2c_rx_data[0];
  csb = i2c_rx_data[1];
  lsb = i2c_rx_data[2];
  }

  toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

  // The least significant bytes l_altitude and l_temp are 4-bit,
  // fractional values, so you must cast the calulation in (float),
  // shift the value over 4 spots to the right and divide by 16 (since 
  // there are 16 values in 4-bits). 
  float tempcsb = (lsb>>4)/16.0;

  float altitude = (float)( (msb << 8) | csb) + tempcsb;

  return(altitude);
}



//Returns the number of feet above sea level
float readAltitudeFt()
{
  return(readAltitude() * 3.28084);
}

#endif

//Reads the current pressure in Pa
//Unit must be set in barometric pressure mode
int readPressure()
{
  toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading
  uint8_t msb, csb, lsb;
  
  //Wait for PDR bit, indicates we have new pressure data
  int counter = 0;
  while( (IIC_Read(STATUS) & (1<<2)) == 0)
  {
      if(++counter > 100) return(-999); //Error out
  }

  {
  msg_t status = MSG_OK;
  
  uint8_t i2c_rx_data[3];
  uint8_t i2c_tx_data[2];
  
  i2c_tx_data[0] = OUT_P_MSB;
  /* sending */
  i2cAcquireBus(&SENSOR_I2C);
  status = i2cMasterTransmitTimeout(&SENSOR_I2C, MPL3115A2_ADDRESS,
                          i2c_tx_data, 1, i2c_rx_data, 3, MPL3115A2_TIMEOUT);
  i2cReleaseBus(&SENSOR_I2C);
  
  msb = i2c_rx_data[0];
  csb = i2c_rx_data[1];
  lsb = i2c_rx_data[2];
  }
  

  // Pressure comes back as a left shifted 20 bit number
  long pressure_whole = (long)msb<<16 | (long)csb<<8 | (long)lsb;
  pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.

  lsb &= 0b00110000; //Bits 5/4 represent the fractional component
  lsb >>= 4; //Get it right aligned
  //float pressure_decimal = (float)lsb/4.0; //Turn it into fraction

  int pressure = pressure_whole;// + pressure_decimal;

  return(pressure_whole);
}

int readTemp(int *temperature)
{
  toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

  //Wait for TDR bit, indicates we have new temp data
  int counter = 0;
  while( (IIC_Read(STATUS) & (1<<1)) == 0)
  {
      if(++counter > 100) return(-999); //Error out
  }
  
  uint8_t msb, lsb;
  
  {
  msg_t status = MSG_OK;
  
  uint8_t i2c_rx_data[2];
  uint8_t i2c_tx_data[2];
  
  i2c_tx_data[0] = OUT_T_MSB;
  /* sending */
  i2cAcquireBus(&SENSOR_I2C);
  status = i2cMasterTransmitTimeout(&SENSOR_I2C, MPL3115A2_ADDRESS,
                          i2c_tx_data, 1, i2c_rx_data, 2, MPL3115A2_TIMEOUT);
  i2cReleaseBus(&SENSOR_I2C);
  
  msb = i2c_rx_data[0];
  lsb = i2c_rx_data[1];
  }
  
  // The least significant bytes l_altitude and l_temp are 4-bit,
  // fractional values, so you must cast the calulation in (float),
  // shift the value over 4 spots to the right and divide by 16 (since 
  // there are 16 values in 4-bits). 
  int templsb = ((lsb>>4)*100)/16; //temp, fraction of a degree

  *temperature = ((msb * 100) + templsb) * 10;

  return 1;
}

//Sets the mode to Barometer
//CTRL_REG1, ALT bit
void setModeBarometer()
{
  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<7); //Clear ALT bit

  IIC_Write(CTRL_REG1, tempSetting);
}

//Sets the mode to Altimeter
//CTRL_REG1, ALT bit
void setModeAltimeter()
{
  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting |= (1<<7); //Set ALT bit
  IIC_Write(CTRL_REG1, tempSetting);
}

//Puts the sensor in standby mode
//This is needed so that we can modify the major control registers
void setModeStandby()
{
  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<0); //Clear SBYB bit for Standby mode
  IIC_Write(CTRL_REG1, tempSetting);
}

//Puts the sensor in active mode
//This is needed so that we can modify the major control registers
void setModeActive()
{
  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting |= (1<<0); //Set SBYB bit for Active mode
  IIC_Write(CTRL_REG1, tempSetting);
}

//Setup FIFO mode to one of three modes. See page 26, table 31
//From user jr4284
void setFIFOMode(uint8_t f_Mode)
{
  if (f_Mode > 3) f_Mode = 3; // FIFO value cannot exceed 3.
  f_Mode <<= 6; // Shift FIFO byte left 6 to put it in bits 6, 7.

  uint8_t tempSetting = IIC_Read(F_SETUP); //Read current settings
  tempSetting &= ~(3<<6); // clear bits 6, 7
  tempSetting |= f_Mode; //Mask in new FIFO bits
  IIC_Write(F_SETUP, tempSetting);
}

//Call with a rate from 0 to 7. See page 33 for table of ratios.
//Sets the over sample rate. Datasheet calls for 128 but you can set it 
//from 1 to 128 samples. The higher the oversample rate the greater
//the time between data samples.
void setOversampleRate(uint8_t sampleRate)
{
  if(sampleRate > 7) sampleRate = 7; //OS cannot be larger than 0b.0111
  sampleRate <<= 3; //Align it for the CTRL_REG1 register

  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= 0b11000111; //Clear out old OS bits
  tempSetting |= sampleRate; //Mask in new OS bits
  IIC_Write(CTRL_REG1, tempSetting);
}

//Clears then sets the OST bit which causes the sensor to immediately take another reading
//Needed to sample faster than 1Hz
void toggleOneShot(void)
{
  uint8_t tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<1); //Clear OST bit
  IIC_Write(CTRL_REG1, tempSetting);

  tempSetting = IIC_Read(CTRL_REG1); //Read current settings to be safe
  tempSetting |= (1<<1); //Set OST bit
  IIC_Write(CTRL_REG1, tempSetting);
}

//Enables the pressure and temp measurement event flags so that we can
//test against them. This is recommended in datasheet during setup.
void enableEventFlags()
{
  IIC_Write(PT_DATA_CFG, 0x07); // Enable all three pressure and temp event flags 
}

// These are the two I2C functions in this sketch.
uint8_t IIC_Read(uint8_t regAddr)
{
  msg_t status = MSG_OK;
  
  uint8_t i2c_rx_data[2];
  uint8_t i2c_tx_data[2];
  
  i2c_tx_data[0] = regAddr;
  
  /* sending */
  i2cAcquireBus(&SENSOR_I2C);
  status = i2cMasterTransmitTimeout(&SENSOR_I2C, MPL3115A2_ADDRESS,
                          i2c_tx_data, 1, i2c_rx_data, 2, MPL3115A2_TIMEOUT);
  i2cReleaseBus(&SENSOR_I2C);
  
  if (status == 0)
    return i2c_rx_data[0];
  return -1;
}

msg_t IIC_Write(uint8_t regAddr, uint8_t value)
{
  msg_t status = MSG_OK;
  
  uint8_t i2c_tx_data[2];
  
  i2c_tx_data[0] = regAddr;
  i2c_tx_data[1] = value;
  
  /* sending */
  i2cAcquireBus(&SENSOR_I2C);
  status = i2cMasterTransmitTimeout(&SENSOR_I2C, MPL3115A2_ADDRESS,
                          i2c_tx_data, 2, NULL, 0, MPL3115A2_TIMEOUT);
  i2cReleaseBus(&SENSOR_I2C);
  return status;
}
