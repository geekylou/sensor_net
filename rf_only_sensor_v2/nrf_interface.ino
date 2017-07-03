//SPIClass mySPI(1); //Create an SPI instance on SPI1 port.
//RF24 radio(3,7,mySPI);

SPIClass mySPI(2); //Create an SPI instance on SPI1 port.
RF24 radio(27,26,mySPI);

// Base address for network
const uint64_t pipe = 0xFEEDF0F000LL;

void nrf_init(byte source_addr)
{
  //radio = _radio;
  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(5,15);

  // optionally, reduce the payload size.  seems to
  // improve reliability
  //radio.setPayloadSize(16);
  
  // enable dynamic payloads
  radio.enableDynamicPayloads();
  
  radio.openReadingPipe(1,pipe + source_addr);
  radio.openReadingPipe(2,pipe + 0xff);
  radio.setAutoAck(true);
  radio.setAutoAck(2, false); 
  
  radio.setChannel(40);
  radio.setDataRate(RF24_250KBPS);
  radio.startListening();
}

bool SendRadioMessage(byte dest,char *buffer, int buffer_length)
{
  DEBUG_OUT.print("Send:");DEBUG_OUT.print(dest);DEBUG_OUT.print(" ");
  radio.setAutoAck(dest != 0xff);
  radio.stopListening();
  radio.openWritingPipe(pipe + dest);
  bool return_value = radio.write( buffer, buffer_length );
  radio.startListening();
  DEBUG_OUT.println(return_value);
  return return_value;
}

bool ReceivedRadioMessageAvailable()
{
   return radio.available();
}

bool ReceiveRadioMessage(char *buffer, int *buffer_length)
{
  bool return_value = radio.available();
  if (return_value)
  {
    byte i;
    // Fetch the payload, and see if this was the last one.
    uint8_t len = radio.getDynamicPayloadSize();
    
    // If a corrupt dynamic payload is received, it will be flushed
    if(!len){
      return(false); 
    }
        
    *buffer_length = len;
    radio.read(buffer,len);

    DEBUG_OUT.write('\n');

    for( i = 0; i < len; i++) {
    DEBUG_OUT.write(' ');
    DEBUG_OUT.print(buffer[i], HEX);
    }
    DEBUG_OUT.write('\n');
  }
}
