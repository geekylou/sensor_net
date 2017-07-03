
void initSensor()
{
  ds.reset_search();
}

signed int searchSensor(byte *addr)
{
  byte return_code;
  byte i;
  byte present = 0;
  byte data[12];
  
  delay(250);
  if(!ds.search(addr))
  {
    return 2;
  }
  DEBUG_OUT.print("ROM =");
  for( i = 0; i < 8; i++) {
    DEBUG_OUT.write(' ');
    DEBUG_OUT.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      DEBUG_OUT.println("CRC is not valid!");
      return 2;
  }
  DEBUG_OUT.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      DEBUG_OUT.println("  Chip = DS18S20");  // or old DS1820
      return_code = 1;
      break;
    case 0x28:
      DEBUG_OUT.println("  Chip = DS18B20");
      return_code = 0;
      break;
    case 0x22:
      DEBUG_OUT.println("  Chip = DS1822");
      return_code = 0;
    default:
      DEBUG_OUT.println("Device is not a DS18x20 family device.");
      return_code = 2;
  }
  return(return_code); 
}

void startSensorConversion(byte *addr)
{ 
  int i; 
  DEBUG_OUT.print("Addr =");
  for( i = 0; i < 8; i++) {
    DEBUG_OUT.write(' ');
    DEBUG_OUT.print(addr[i], HEX);
  }
  DEBUG_OUT.println();
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
}

signed int readTempreture(int *tempreture,byte *addr)
{
  byte present = 0;
  byte i;
  byte data[12];
    
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  DEBUG_OUT.print("  Data = ");
  DEBUG_OUT.print(present, HEX);
  DEBUG_OUT.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    DEBUG_OUT.print(data[i], HEX);
    DEBUG_OUT.print(" ");
  }
  DEBUG_OUT.print(" CRC=");
  DEBUG_OUT.print(OneWire::crc8(data, 8), HEX);
  DEBUG_OUT.println();
  if (data[8] != OneWire::crc8(data, 8))
    present = -1;

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  
  if (addr[0] ==  0x10) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  *tempreture = (raw * 625)/10;
  DEBUG_OUT.print(" Raw:");
  DEBUG_OUT.print(*tempreture);
  DEBUG_OUT.println(" Celsius");
  return present;
}

