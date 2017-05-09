/*
          Funktionen fuer Zaehler und Interrupts, insbesondere Regen und WInd

Die beiden Sensoren arbeiten mit Reed-Schalter, deren Schaltvorgänge (Clicks) Interrupts ausloesen.
In den Interrupt-Funktionen werden die clicks gezaehlt, wobei Prellvorgänge anhand der Schaltzeit
erkannt und ignoriert werden.
Die echten Clicks werden bei einer Lang- oder Kurzfrist-Messung (Windboen) in Messwerte umgerechnet,
die Zaehler dabei genullt.
 */

//
// Konstanten zur Steuerung des Ablaufes
//
#define DEBUG

//
// Pin-Belegung
//
#define windspeedPin  0    // Impulse vom Windrad
#define RegenPin  7    // Impulse vom Regenmesser

//
// Globals
//

// Zeit der letzten Messung (Übernahme der Click-Counter in Messwert, Rücksetzen der Counter)
volatile long wind_last_short_time; //Wind Kurzfrist
volatile long wind_last_long_time; //Wind Langfrist
volatile long rain_last_time; //Regen

//Zeit des letzten Schaltvorgangs (um Zeit fuer High-Low-Wechsel zu bestimmen => Fehler oder echte Clicks) 
volatile long wind_lastclick_t=0; 
volatile long rain_lastclick_t=0; //Zeit des letzten Schaltvorgangs

// Fehlerclicks (zu kurze Zeit fuer High-Low-Wechsel durch Prellen)
volatile long wind_err_counter=0; //Zaehler f Fehler-Clicks Zeit zu kurz
volatile long rain_err_counter=0; //Zaehler f Fehler-Clicks Zeit zu kurz

// Echte Clicks
volatile long wind_old_counter=0; // Wind Kurzfrist
volatile long wind_counter=0;     // Wind Langfrist
volatile long rain_counter=0;     // Regen   

// Maximalwerte f Windboen
/* Muss der global sein ? */
volatile float wind_boe_temp_max=0; //Boeen-Maximum in km/h

//
// Alles initialisieren
//
void setup_counter(){
  rain_last_time=micros();        //Zeit als Startzeit merken
  wind_last_long_time=micros();
  wind_last_short_time=micros();  
  pinMode(windspeedPin,INPUT);  //Pins konfigurieren und mit Interrupt-Funktion verknüpfen
  pinMode(RegenPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(windspeedPin), count_wind_clicks, FALLING);
  attachInterrupt(digitalPinToInterrupt(RegenPin), count_rain_clicks, FALLING); 
}

//
// Interrupt-Funktion fuer Regenmesser
//
void count_rain_clicks() {
  
  long now=micros();
  long delta_t=now-rain_lastclick_t;

  //zu kurze Störimpulse ignorieren
  if (delta_t>100000) {
     rain_counter++;
     rain_lastclick_t=now;
     #ifdef DEBUG2
       Serial.println(delta_t);
       Serial.println(rain_counter);
     #endif
  } else {
     rain_err_counter++;
     #ifdef DEBUG2
       Serial.print(".......");    
       Serial.println(delta_t);    
     #endif
  }
  
}


//
// Interrupt-Funktion fuer Winrad
//
void count_wind_clicks() {
  long now=micros();
  long delta_t=now-wind_lastclick_t;

  //zu kurze Störimpulse ignorieren
  if (delta_t>10000) {
     wind_counter++;
     wind_lastclick_t=now;
     #ifdef DEBUG2
        Serial.println(delta_t);
        Serial.println(wind_short_counter);
     #endif
  } else {
     wind_err_counter++;
     #ifdef DEBUG2
       Serial.print(".......");    
       Serial.println(delta_t);    
    #endif
  }
  
}

//
// Wind-Kurzfrist-Hilfs-Messung
//
// Berechnet im Kurzfristintervall die Windgeschwindigkeit als wind_boe_temp
// und bestimmt den Maximalwert im Langfristintervall als Wi_Boe
//
void windboe() {

  //akt Zeit und Counter sichern
  long  now=micros();
  long wc_akt=wind_counter;
  
  //Zeit- und Click-Different seit letzter Messung (Zeit in sec)
  float delta_t=float(now-wind_last_short_time)/1000000.;  
  long delta_c=wc_akt-wind_old_counter
    
  //Zeit und Clicks dieser Messung merken (muessen bei Langzeitmessung rueckgesetzt werden)
  wind_last_short_time=now;
  wind_old_counter=wc_akt;
    
  //Windgeschwindigkeit:  vwind=n*2*Pi*r/0.261685 mit r=70 mm und n=Umdrehungen je sec bei 2 clicks je Umdrehung
  //1 click je s also 0,8403 m/s oder 3,025 km/h
  float wind_boe_akt=float(delta_c)*3.025/delta_t;  //Km/h

  //Neues Böen-Maximum ?
  if (wind_boe_akt>wind_boe_temp_max) {
    wind_boe_temp_max=wind_boe_akt; //merken
  }

  #ifdef DEBUG3
     Serial.print("counter,secs,boe im kmh ");    
     Serial.println(delta_c);    
     Serial.println(delta_t);    
     Serial.println(wind_boe_temp_max);    
  #endif 
  
}

//
// Wind-Langfrist-Messung
//
// Setzt auch Global Wi_Boe
//
// Ist Kurzfristmessung uebergeordnet und nullt deren Zaehler nachdem die Werte vorher gesichert (Boe)
// bzw zu den Langfristwerten dazu addiert wurden
//
float miss_wind() {
  //akt Zeit und Counter sichern
  long  now=micros();
  long wc_akt=wind_counter;
  
  //Zeit seit letzter Messung in sec
  float delta_t=float(now-wind_last_long_time)/1000000.;  

  //Zeit dieser Messung merken
  wind_last_long_time=now;
   
  //Eigenen Zaehler Nullen
  wind_counter=0;
  
  //Kurzfrist-Messung neu initialisieren
  wind_old_counter=0;
  wind_last_short_time=now; //Zeit dieser Messung merken
  Wi_Boe=wind_boe_temp_max; //Windboe als Messwert sichern
  wind_boe_temp_max=0;
  
  #ifdef DEBUG3
     Serial.print("counter,secs ");    
     Serial.println(wc_akt);    
     Serial.println(delta_t);    
  #endif
  
  //vwind=n*2*Pi*r/0.261685 mit r=70 mm und n=Umdrehungen je sec bei 2 clicks je Umdrehung
  //1 click je s also 0,8403 m/s
  return(float(wc_akt)*3.025/delta_t);  //Km/h
}

//
// Regen-Messung
//
float miss_regen() {

  //akt Zeit und Counter sichern
  long  now=micros();
  float rain_temp=rain_counter;
  
  //Zeit seit letzter Messung in sec
  float delta_t=float(now-rain_last_time)/1000000.;  

  //Zeit dieser Messung merken
  rain_last_time=now;
  
  //Zaehler updaten
  rain_counter=0;
  
  #ifdef DEBUG_
     Serial.print("counter,secs ");    
     Serial.println(rain_temp);    
     Serial.println(delta_t);    
  #endif

  return(rain_temp*0.45*60/delta_t); //=.45 mm Regen per click umgerechnet auf 1 minute
}

