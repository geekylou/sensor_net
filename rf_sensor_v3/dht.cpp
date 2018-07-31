/*
 *  DHT Library for  Digital-output Humidity and Temperature sensors
 *
 *  Works with DHT11, DHT22 Nucleo Board tested on F103RB
 *
 *  Copyright (C) Wim De Roeve 
 *                ported to work on Nucleo Board:
 *                                              Moises Marangoni
 *                                              Somlak Mangnimit
 *                based on DHT22 sensor library by HO WING KIT
 *                Arduino DHT11 library 
 */

#include "dht.h"

#include "chprintf.h"
#include "board_specific.h"

#define DHT_DATA_BIT_COUNT 41

DHT::DHT(ioportid_t port, uint8_t pin,int DHTtype) {
    _pin = pin;
	_port = port;
    _DHTtype = DHTtype;
}

DHT::~DHT() {
}

int DHT::readData() {
	chprintf((BaseSequentialStream *)&SD1,"readData!\r\n");
    //Timer tmr;
    // BUFFER TO RECEIVE
    //uint8_t bits[5];
    uint8_t cnt = 7;
    uint8_t idx = 0;
    
	systime_t start_time; 

    // EMPTY BUFFER
    for(int i=0; i< 5; i++) bits[i] = 0;

	palSetPadMode(_port, _pin, PAL_MODE_OUTPUT_OPENDRAIN); 
    // REQUEST SAMPLE
    palWritePad(_port,_pin,0);
    chThdSleepMilliseconds(18);
    palWritePad(_port,_pin,1);
    chThdSleepMicroseconds(40);

	palSetPadMode(_port, _pin, PAL_MODE_INPUT_PULLUP);
	
    // ACKNOWLEDGE or TIMEOUT
    unsigned int loopCnt = 10000;
    
    while(!palReadPad(_port,_pin)) if(!loopCnt--)return DHTLIB_ERROR_TIMEOUT;

    loopCnt = 10000;
    
    while(palReadPad(_port,_pin)) if(!loopCnt--)return DHTLIB_ERROR_TIMEOUT;

    // READ OUTPUT - 40 BITS => 5 BYTES or TIMEOUT
    for(int i=0; i<40; i++){
        
        loopCnt = 10000;
        
        while(!palReadPad(_port,_pin))if(loopCnt-- == 0)return DHTLIB_ERROR_TIMEOUT;

        start_time = chVTGetSystemTime();

        loopCnt = 10000;
        
        while(palReadPad(_port,_pin))if(!loopCnt--)return DHTLIB_ERROR_TIMEOUT;

        if(ST2US(chVTTimeElapsedSinceX(start_time)) > 40) bits[idx] |= (1 << cnt);
                
        if(cnt == 0){   // next byte?
        
            cnt = 7;    // restart at MSB
            idx++;      // next byte!
            
        }else cnt--;
        
    }
    // WRITE TO RIGHT VARS
    // as bits[1] and bits[3] are allways zero they are omitted in formulas.
    //humidity    = bits[0]; 
    //temperature = bits[2]; 

    uint8_t sum = bits[0] + bits[1] + bits[2] + bits[3];

    if(bits[4] != sum)return DHTLIB_ERROR_CHECKSUM;
       
    return DHTLIB_OK;
}
int DHT::ReadTemperature() {
    //int retornotemp;

    switch (_DHTtype) {
        case DHT11:
            temperature = bits[2];
            return temperature * 100;
        case DHT22:
            temperature = bits[2] & 0x7F;
            temperature *= 256;
            temperature += bits[3];
            temperature *= 100;
           if (bits[2] & 0x80)
            {temperature *= -1;}
            return temperature;
    }
    return 0;
}

int DHT::ReadHumidity() {
    //int v;

    switch (_DHTtype) {
        case DHT11:
            humidity = bits[0];
            return humidity*1000;
        case DHT22:
            humidity = bits[0];
            humidity *= 256;
            humidity += bits[1];
            humidity *= 100;
            return humidity;
    }
    return 0;
}
