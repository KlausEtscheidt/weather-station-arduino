/*
             Funktionen zur Funk Kommunikation �ber NRF24L01+-Transceiver
*/

//
// Konstanten zur Steuerung des Ablaufes
//

//
// Bibliotheken
//
#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>
#include <SPI.h>

//
// Pin-Belegung
//
#define CE_Pin 8            // F�r Mega CE:=d6=6 CSN:=d7=7 ; f�r Micro  CE:=8 CSN:=9
#define CSN_Pin 9

//
//Funk-Objekt anlegen
//
RF24 radio(CE_Pin,CSN_Pin);

//
// Globals
//
//Puffer fuer zu funkendes Telegramm
byte message[32] ;
byte *message_ptr=message;

// Adressen Node1 micro Node2 Raspi Garten Node3 Raspi innen
byte nrf_addresses[][6] = {"Node1","Node2","Node3"};


//
// Sende-Parameter einstellen  => Abgleichen mit Empfaenger
//
void setup_nrf() {
  
  radio.begin();

  radio.setPALevel(2);
//  radio.setPALevel(RF24_PA_HIGH);
  radio.setCRCLength(RF24_CRC_16 );
  radio.setDataRate(RF24_250KBPS);
  
  radio.setChannel(0x70);
  radio.setPayloadSize(32);
  radio.setAutoAck(true);
  radio.setRetries(10,15);            // Smallest time between retries, max no. of retries

  //Ziel-Adresse Node1 wird mit uebertragen; Empf. muss f�r diese Adresse Reading-Pipe oeffnen
  radio.openWritingPipe(nrf_addresses[0]);  //geht in TX- und RX-Pipe-0-Register

  radio.stopListening();                                    // First, stop listening so we can talk.

  #ifdef SEROUT
  Serial.println(F("RF24 init Started"));
  Serial.print("Channel: ");
  Serial.print(radio.getChannel());
  Serial.print(" PayloadSize: ");
  Serial.print(radio.getPayloadSize());
  Serial.print(" DataRate: ");
  Serial.print(radio.getDataRate());
  Serial.print(" CRCLength: ");
  Serial.println(radio.getCRCLength());
  printf_begin();
  radio.printDetails();
  #endif

  delay(200);

}

//
// Telegramm einmalig senden (nur retries durch Hardware)
//
bool send_nrf() {
   bool ok;
   radio.powerUp();
   ok=radio.write( message, sizeof(message));
   radio.powerDown();
   return(ok);  
}

//
// Telegramm zur�cksetzen
//
void reset_message() {
  for (int i=0;i<32;i++) {
     message[i]=0;
  }
  message_ptr=message;
}

//
// ID und Wert eines Messwertes an Telegramm anh�ngen
//
void add_2_message(byte type,long wert){
	add_type_2_message(type);
	add_long_2_message(wert);
}

//
// 1 Byte ID des Messwertes zum Telegramm dazu
//
void add_type_2_message(byte type){
    *message_ptr=type;
    message_ptr++;
}

//
// 4 Byte Messwert zum Telegramm dazu
//
void add_long_2_message(long wert){
  byte buf[4];
  long_2_byte(wert,buf);
  for (int i=0;i<4;i++) {
    *message_ptr=buf[i];
    message_ptr++;
  }
}

//
// Long-wert in 4 Byte wandeln
//
void long_2_byte(long n,byte *buf) {
  int i; 
  for (i=0;i<4;i++) {
    buf[i] = (byte) n;
    n=n>>8;
  }
}

//
// Telegramm z Debuggen ausgeben
//
void  print_message() {
  for (int i=0;i<32;i++) {
     Serial.print(message[i]);
     Serial.print(" ; ");
  }
  Serial.println();
}

//
// Nur zum Testen der Kommunikation
//
#ifdef SEROUT 
void test_nrf() {

  for (long data=1;data<600000;data++) {

	  long i;
	  Serial.println(data);

	  for (i=0;i<max_retries;i++) {
		reset_message();
		long now=millis();
		add_type_2_message(99);
		add_long_2_message(data);
		add_type_2_message(98);
		add_long_2_message(i);
	  
		if (send_nrf()==true) {
			break;
		 } else { 
			#ifdef SEROUT
			Serial.print("-");
			#endif
			delay(500);
		 }      
		 
	  }

	  if (i>=max_retries) {
		Serial.println("send failed"); 
	  } else {
		Serial.println("send ok"); 
	  }

	  delay(2000);
  }  //next data

}
#endif    
