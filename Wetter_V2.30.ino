//
// Hauptprogramm Erfassung von Wetterdaten 
// 
/* 
Erfasst werden: Lufttemperatur, Bodenfeuchte und -Temperatur, Regenmenge, 
                Wingeschwindigkeit (Langfristmittel und B�en) und die Batteriespannung 
Die Daten werden alle 5 min (Langfristtakt) erfasst. 
Windboen werden im 3-Sek-Takt ermittelt. F�r den Langfristtakt wird das B�en-Maximum ermittelt.

Die Daten werden je Langfristtakt an einen Raspberry gefunkt.
Das Sende-Telegramm hat max 32-Byte (Nutzdaten).
Es besteht je Messgroesse aus einer Messwert-Id und einem long Wert (insg 5 Byte).
Float Werte werden als i=int(100*x); Integer als i=100*i gesendet.

Da 32-Byte nicht reichen, werden nacheinander mehrere Telegramme gesendet
*/

//
// Konstanten zur Steuerung des Ablaufes
//
#define _SEROUT    // !!!!!! F�r Produktivbetrieb unbedingt abschalten, Programm wartet auf ser. Monitor
#define max_retries 6   //Anzahl der Software-Sendewiederholungen

const long short_measure_cycle=3000;
const byte long_2_short=5; //nach 5 kurzzeitmessungen eine lange
//
// Bibliotheken
//
#include <sht11_ke.h>   //Treiber fuer Bodenfeuchte und  -temp.
#include <timer.h>      //Timer-Objekte 

//
// Pin-Belegung
//
#define i2c_data_Pin 2    //sda
#define i2c_clock_Pin 3   //SCL
#define BatteriePin A5

//
// Sensorobjekt f Bodenfeuchte anlegen
//
SHT1x  Bodenfeuchte(i2c_data_Pin,i2c_clock_Pin);

//
// Zuordnung von ID's zu Kurzbezeichnungen von Messwerten => bessere Lesbarkeit
// Kurzbezeichnung wird so von Raspberry in Datenbank abgelegt.
//
#define T_St 1           // Temperatur (Luft) Stellplatz 
#define T_Bod_St 2       // Temperatur Boden Stellplatz 
#define Wi_Stae_St 3     // Wind Staerke Stellplatz (Langfristmittel)  
#define Reg_St 4         // Regen Stellplatz  
#define U_Batt_St 5      // U (Spannung) Batterie Stellplatz  
#define Hum_Bod_St 6     // Humidity (Feuchte) Boden Stellplatz      
#define Wi_Boe_St 7      // Wind Boeen Stellplatz    

//
// Globals
//

//Globals zum Speichern der Messwerte
volatile float T_Luft,T_Bod,Wi_Stae,Wi_Boe,Reg,Hum_Bod,U_Batterie;

//Id des NRF-Telegramms. Wird fortlaufend hochgezaehlt und dient Raspi zur "Verlustkontrolle".
long Paket_id=0; 

//
// Alles initialisieren
//
void setup() {

  #ifdef SEROUT
    Serial.begin(115200);
    while(! Serial);  //Warte auf Terminal. Kann zum Aufhaengen fuehren, wenn nicht am PC
    #define PRINTLN(output) Serial.println(output) ;
    #define PRINT(output) Serial.print(output) ;
    PRINTLN ("Starte Wetter !!!");
  #else
    #define PRINT(output) 
    #define PRINTLN(output) 
  #endif

  setup_counter();
  setup_nrf();
}

//
// Misst Batteriespannung
//
float alt_miss_Batterie() {
  float U_Batt=float(analogRead(BatteriePin))*10./1024.;  //2*5V;  5V ergibt 1024; *2 wg Spannungsteiler;
  return (U_Batt);
}

float miss_Batterie() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return float(result)/1000.; // Vcc in Volt*100
}

#ifdef SEROUT
//
// Messwerte zum Debuggen auf Terminal ausgeben
//
void ser_print() {
  Serial.print(" T-Luft: ");
  Serial.print(T_Luft);

  Serial.print(" T-Boden: ");
  Serial.print(T_Bod);

  Serial.print(" Feuchte: ");
  Serial.print(Hum_Bod);

  Serial.print(" Windstaerke: ");
  Serial.print(Wi_Stae);

  Serial.print(" Batterie: ");
  Serial.print(U_Batterie);

  Serial.print(" Windboen: ");
  Serial.print(Wi_Boe);

  Serial.print(" Regen: ");
  Serial.println(Reg);
  
}
#endif


//
// Alle Messungen im Langfrist-Intervall ausf�hren
//
void long_cycle_measure(){
    
  PRINT(millis());
  PRINT(" Messe lang: ");  

  T_Luft=miss_Temperatur(0);
  Hum_Bod=Bodenfeuchte.readHumidity();
  T_Bod=Bodenfeuchte.getTemperature();
  Wi_Stae=miss_wind(); // Setzt auch Global Wi_Boe
  Reg=miss_regen();
  U_Batterie=miss_Batterie();

  #ifdef SEROUT
     ser_print();
  #endif

}

  
//
// Telegramme zusammenstellen 
//
// Die Daten muessen vorher aktuell in Globals abgelegt werden

// Message  zusammenstellen
void compose_message(long id,int retries,int sub_msg ) {
  reset_message();
  add_2_message(99,id);
  add_2_message(98,retries);
  if (sub_msg==1)	{
	  add_2_message(T_St,int(100*T_Luft));
	  add_2_message(T_Bod_St,int(100*T_Bod));
	  add_2_message(Wi_Stae_St,int(100*Wi_Stae));
	  add_2_message(Reg_St,int(100*Reg));
  } else {
	  add_2_message(U_Batt_St,int(100*U_Batterie));
	  add_2_message(Hum_Bod_St,int(100*Hum_Bod));
	  add_2_message(Wi_Boe_St,int(100*Wi_Boe));
  }
}


void send_data() {
    
	//Daten auf zwei Telegramme aufteilen
	for (int sub_msg=1;sub_msg<=2;sub_msg++) {
  
  		PRINT(millis());
  		PRINT(" sende  ");
  		PRINTLN(sub_msg);
  		
  		long now=millis(); //Zeit
      Paket_id++;
      
  		int retry;
  		//Versuche Telegramm zu senden
  		for (retry=0;retry<max_retries;retry++) {
    			compose_message(Paket_id,retry,sub_msg); //Telegramm-Anfang mit Zeit und Anzahl Sende-Versuche
    			if (send_nrf()==true) {
      				break;
    			 } else { 
      				PRINT(".");
      				delay(100);
        		}
    	}      
      #ifdef SEROUT2
      if (retry>=max_retries) {
          Serial.println("send failed"); 
      } else {
          Serial.println("send ok"); 
      }
      #endif
      delay(500);
	} // naechstes Telegr
 

}

// 
// Fuehrt Messungen im Lang- und Kurzfristintervall aus, wenn entsprechende Timer abgelaufen
// Die Langfristdaten werden versendet;die Kurzfristdaten (Windb�en) sind hier als Maxima enthalten
//
void loop() {
  long start;
  
  // long_2_short mal Kurzfristmessung
  for (int i=1;i<=long_2_short;i++) {
   
     start=millis();
     
     // Messwerte holen
	 windboe_tmp();    

     //Beim Vorletzten auch Langfristmessung
     if (i==long_2_short-1) { 
        long_cycle_measure();
     }
     //Beim Letzten auch Daten Senden (Aufteilung noetig, da sonst zu Langsam
     if (i==long_2_short) { 
        // Messwerte funken
        send_data();
     }
     
     long waitfor=start+short_measure_cycle-millis();
     if (waitfor>0) {
        PRINTLN( waitfor);
        delay(waitfor) ;
     } else {
        PRINTLN("weiter");
     }
  
  }
  
}

