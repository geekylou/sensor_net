/*
 *  DHT Library for  Digital-output Humidity and Temperature sensors
 *
 *  Works with DHT11, DHT22 Nucleo Board tested on F103RB
 *
 *  Copyright (C) Wim De Roeve 
 *                ported to work on Nucleo Board:
 *                                                Moises Marangoni
 *                                                Somlak Mangnimit
 *                based on DHT22 sensor library by HO WING KIT
 *                Arduino DHT11 library 
 */

#ifndef MBED_DHT_H
#define MBED_DHT_H

#include "ch.h"
#include "hal.h"

enum eType{
        DHT11     = 11,
        DHT22     = 22,
    } ;

class DHT {

public:

#define DHTLIB_OK                0
#define DHTLIB_ERROR_CHECKSUM   -1
#define DHTLIB_ERROR_TIMEOUT    -2

    DHT(ioportid_t port, uint8_t pad,int DHTtype);
    ~DHT();
    uint8_t bits[5];
    int readData(void);
    int ReadHumidity(void);
    int ReadTemperature(void);
    int humidity;
    int temperature;

private:
    //time_t  _lastReadTime;
    ioportid_t _port;
	uint8_t _pin;
	
    int _DHTtype;
    int DHT_data[6];
};

#endif

