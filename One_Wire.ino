/*
             Funktionen fuer Temperatur-Sensoren und 1-Wire-Bus-Teilnehmer
*/

//
// Bibliotheken
//
#include <OneWire.h>

//
// Pin-Belegung
//
#define One_Wire_Pin  10    // Datenleitung 1 Wire

//
//Objekt fuer one wire anlegen
//
OneWire  ds(One_Wire_Pin);  // on pin 10 (a 4.7K resistor is necessary)

//
// Liefert fuer Sensor-Nummer 0 die Lufttemperatur 
//
float miss_Temperatur(int sensor_nr){
  //28 FF AB 33 86 16 5 1 gekapselterSensor mit kurzem Kabel

  byte addresses[][8]={{0x28,0xFF,0xAB,0x33,0x86,0x16,0x05,0x01},{0x28,0x80,0x46,0x65,0x08,0x00,0x00,0xB5},{0x28,0xE6,0xBA,0x63,0x08,0x00,0x00,0x4A}};
  return(read_Temperature(addresses[sensor_nr]));

}

//
// Hardware-Treiber zum Lesen eines DS18B20-Temp-Sensors
//
float read_Temperature(byte addr[]) {
  byte i;
  byte data[12];
  float celsius;

  //Reset the 1-wire bus. Usually this is needed before communicating with any device.
  ds.reset();
  //Adresse waehlen
  ds.select(addr);
  //Write a byte, and leave power applied to the 1 wire bus.
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  //Messwert lesen
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];

  //Sensor sollte immer DS18B20 sein (Byte 0 Adresse = 0x28
  byte cfg = (data[4] & 0x60);
  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  //// default is 12 bit resolution, 750 ms conversion time
  return((float)raw / 16.0);
 
}

//
//
//
void debugprint_Adress(byte addr[]){
      //Addresse ausgeben
    byte i;
    Serial.print("ROM =");
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
    Serial.println();

}

void get_1_Wire_addr() {
  byte addr[8];

  Serial.println("Suche Busteilnehmer");
  while ( ds.search(addr)) {
    delay(250);
    debugprint_Adress(addr);
  }
  return;

}

