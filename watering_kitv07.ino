//  Steuerung der Bewässerung, basierend auf dem Elcrow Smart Plant Watering Kit
// 24.6.21: Compilierbar machen
// 25.6.21: Compilierbar
//          Das Anzeigen der Blumensymbole auf dem Display erfolgt mittels Code für jedes Symbol -> gehört in Funktion
// 27.6.21  Zeichnen der Blumensymbole in einer For-Schlaufe
//          Auswerten der Sensorfehler implementiert. Sowohl Festspannung am Eingang wie auch unterbruch oder KS werden Signalisiert
//          Anpassen der Ventilöffnung... Das Ventil muss einen Bedarf haben, es muss Freigegeben sein, der Sensor muss ohne Störung sein....dann darf gegossen werden
//          Falls eine Sensorstörung vorliegt, wird das Notprogramm gefahren.
// 28.6.21  Inbetriebnahme / Test. Diverse Printstatements eingefügt
// 29.6.21  Feuchtigkeitswert bei Fühlerfehler als xx& ausgeben
//  2.7.21  Flower und %-Angabe mit Schleife ausgeben
//  7.7.21  Debug Ausgabe des Valvetimerflags umgestellt auf flexible Grösse. Valvestatus ebenfalls ausgeben.
//  8.7.21  Umschreiben der Ventilansteuerung. Ein Status_Flag wird aufgrund der Logik gesetzt. Aufgrund des Flags wirden Ventile und Pumpe gesteuert. 
//          Die Pumpe darf nur laufen, wenn mindestens 1 Ventil offen ist.
// 12.7.21  Die Ausgänge müssen mit digitalwrite gesetzt werden ; war einfache zuweisung
//          Die Char-arrays können ohne Länge deklariert werden; benötigt 1 Char mehr als angegeben...
// 13.11.21 Feuchte-Sollwerte können mittels 'rH' gesetzt werden, Feuchte-Blume #1 wird nicht mehr angezeigt - behoben (x_Position nicht mehr aus array beziehen) Ursache nicht klar
//          Testmöglichkeit der Schaltuhr, indem die Zeiten ausgehend von einem Startwert festgesetzt werden
//          drawTH: die Zahlen werden fast immer 2 mal ausgegeben. Grund unklar... bei #1 entfernt.
// 15.7.21  optimieren der Memory-Nutzung: Strings als F(). Variabeln mit kleinstmöglichem Footprint
// 16.7.21  Die Schaltzeiten und die Ventile waren im Zeitprogramm mit dem geleichen Index angesteuert. Fehler bei der Stringausgabe der Uhrzeit behoben 
// 17.7.21  Schaltzeiten scheinen OK. IDE reagiert komisch. upload probleme, board und Com unstabil
// 18.7.21  Libraries von Elecrow neu installieren
//          

// Funktionalität:
// Einlesen der Werte der Feuchtesensoren -> done
// zusätzliches Mapping der Werte auf eine Skala von 0..100 -> done
// Zeitsteuerung: Die Bewässerung ist von Startzeit bis Endzeit aktiv 
// Bsp: 6:00 Startzeit – 8:00 Endzeit
// möglicherweise 2 Intervalle anbieten -> done
// Das Bewässern erfolgt in Intervallen 5 Min. On 5 Min. Off damit die Erde die Feuchtigkeit aufnehmen kann und der Sensor genügend Zeit zum Reagieren erhält
// Enhancement  Die Laufzeit der Pumpe wird getrackt um die Pumpe bei leerem Behälter zu sperren
// Alternativ Level-Sensor einsetzen (Ultraschall, Schwimmerschalter, Druckmessung)
// Enhancement  Prüfen ob der Sensor reagiert auf ein Bewässern
// Wert einlesen und mit einem später eingelesenen Wert vergleichen; erste Ableitung der Feuchtigkeit
// Enhancement  rolling average wäre aufgrund der schwankenden Fühlerwerte eine echte Verbesserung Bsp: Ist = Ist +1/k(Sensorwert - Ist) 
// Ist = Ist*(1-1/k) + Senssorwert / k
// Ringbuffer um Sensorausschläge zu Dämpfen; Speicherintensiv
// Enhancement: Die Ventile werden jeweils nur einzeln geöffnet (erleichtert tracking der Wassermenge pro Ventil)
// Enhancement: Zeitprogramm zur Verfügung stellen um ohne Feuchte-Bedingung zu giessen. Nutzbar auch zu Testzwecken
// Enhancement: Sollwertanzeige auf Display anstelle Fühlernr.
// Enhancement: Datenlogging

#include <Wire.h>
// I2C Adresse der RTC ist 0x68 für DS1307 und DS3231
#define RTC_I2C_ADDRESS 0x68
#include <U8glib.h>
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);    // I2C
//#include "Wire.h"
#include <RTClib.h>
RTC_DS1307 RTC;

#define EIN_SCHALTEN HIGH
#define AUS_SCHALTEN LOW


struct schaltRelais_t{
  uint8_t Valveprognr;
  int ein_zeit; 
  int aus_zeit;
};

/*
// Der Beispielcode enthält eine Intervall-Schaltuhr
struct timerRelais_t{
  byte pin;
  byte dauer;
  int timer1; 
  int timer2;
};
*/

// HW definitionen
// set all moisture sensors PIN ID
const uint8_t moisture_pin[] = { A0 , A1 , A2 , A3 } ;
// set water-valves Outputs
const uint8_t valvepin[] = {6, 8, 9, 10};

// set water pump Pin
const uint8_t pump = 4;

// set Reserve Pin
//#define res 7

// set button
const uint8_t button = 12;


boolean Valvetimeprog[] = {0, 0, 0, 0, 0} ; // Freigabe der Zeitsteuerung Ventil-Flags


//valve_state    1:open   0:close
boolean valve_state_flag[] = {0, 0, 0, 0} ;   //Anforderung das Ventil Feuchtebedingt zu öffnen

// declare moisture values
uint8_t moisture_val[] = { 33 , 33 , 33 , 33 } ; //Feuchtigkeitswerte mit 33 initialisieren (Jeder Wert wäre OK). 

//pump state    1:open   0:close
boolean pump_state_flag = 0;



// Hier den Kurzzeittimer definieren
// timerRelais_t timerRelais={valve_kurzzeit[0], 25, 830, 2043}; // Timer an Pin-0 für 25 Sekunden um 0830 und 2043


// Hier die Ventil-Flags definieren mit Ein- und Ausschaltzeiten
// Das Valvetimeprog[4] wird bei defektem Sensor verwendet

// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  Einschalten der Kanäle
//boolean Anlagefreigabe = false;           // Jegliche Ventile/Pumpe inaktiv
boolean Valvefreigabe[] = {1, 1, 1, 1} ;  // Ventil ist gesperrt : 0

// _Setpoints
// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  Sollwerte Feuchtigkeit
uint8_t moisture_min[] = {45, 40, 37, 45} ; // Oleander on #0:45%.  Kann zur Laufzeit überschrieben werden (volatile)
//uint8_t moisture_min[] = {90, 90, 90, 90} ; // Oleander on #0:50%.  Kann zur Laufzeit überschrieben werden (volatile)
const uint8_t moisture_hysteresis = 7;

// Hier die Ventil-Flags definieren mit Ein- und Ausschaltzeiten
// Das Valvetimeprog[4] wird bei defektem Sensor verwendet

#define startzt 1721
//zum testen eingeführt

schaltRelais_t schaltRelais[]=    
{
// gestaffelt alle einschalten zu Testzwecken 
/*
  {0, startzt, startzt+3},  // Ventil-Flag, Einschaltzeit, Ausschaltzeit
  {1, startzt+3, startzt+6},
  {2, startzt+6, startzt+9},
  {3, startzt+9, startzt+12},
//  {3, startzt+3, startzt+4},
  {4, 600, 601},
  
//  {0, startzt+2, startzt+3},  // Ventil-Flag, Einschaltzeit, Ausschaltzeit
//  {1, startzt+1, startzt+2},
//  {1, startzt, startzt},
//  {2, startzt, startzt+1},
//  {2, startzt, startzt},
//  {3, startzt, startzt+1},

// End Testzeiten */

// reguläre Zeiten /*  
  {0, 650, 656},  // Ventil-Flag, Einschaltzeit, Ausschaltzeit
  {0, 710, 716},  // Ventil-Flag, Einschaltzeit, Ausschaltzeit
//  {0, 710, 716},

  
  {1, 656, 702},
  {1, 716, 722},
//  {1, 656, 703},
  
  {2, 722, 728},
  {2, 742, 748},
//  {2, 650, 656},
  
  {3, 644, 650},
  {3, 748, 754},
//  {3, 710, 716},
  {4, 700, 705},
//  */
};




// Falls die Sensorüberwachung aktiv ist, werden die Flags bei einem Fehler auf false gesetzt
// Die Sensoren sollen nach einem Reset korrekt funktionieren
uint8_t Sensorfault[] = {3, 3, 3, 3} ; // 0: Sensor OK, 1: kein R-Teiler, 2: Unterbruch, 3: KS


//valve_kuzzeit    1:open   0:close
//boolean valve_kurzzeit[] = {0, 0, 0, 0} ;   //Valve einige sekunden lang öffnen

//static unsigned long currentMillis_send = 0;
//static unsigned long  Lasttime_send = 0;

int jahre,monate,tage,stunden,minuten,sekunden;
// wochentag bleibt in diesem Test-Sketch unberücksichtigt

char daysOfTheWeek[][9] = {"Sun", "Mon", "Tues", "Wed", "Thur", "Fri", "Sat",};
//unsigned long nowtime;
//unsigned long endtime;
//unsigned long nowtimeNext;
//unsigned long nowtime1;
//unsigned long endtime1;
//unsigned long nowtimeNext1;
//unsigned long nowtime2;
//unsigned long endtime2;
//unsigned long nowtimeNext2;
//unsigned long nowtime3;
//unsigned long endtime3;
//unsigned long nowtimeNext3;

// Forward Deklarationen der Funktionen
//void relaisSchaltenNachZeit(int, int);
//void behandleSerielleBefehle();
//void relaisTimerNachZeit();
//void rtcReadTime(int, int, int, int, int, int );
//void rtcWriteTime(int, int, int, int, int, int );
//byte decToBcd(byte);
//byte bcdToDec(byte);
//int getIntFromString (char, byte);
//void Fuehlerfeuchteausgabe(uint8_t);
//void Pumpensteuerung();

// good flower
unsigned char bitmap_good[] U8G_PROGMEM = {

  0x00, 0x42, 0x4C, 0x00, 0x00, 0xE6, 0x6E, 0x00, 0x00, 0xAE, 0x7B, 0x00, 0x00, 0x3A, 0x51, 0x00,
  0x00, 0x12, 0x40, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x06, 0x40, 0x00, 0x00, 0x06, 0x40, 0x00,
  0x00, 0x04, 0x60, 0x00, 0x00, 0x0C, 0x20, 0x00, 0x00, 0x08, 0x30, 0x00, 0x00, 0x18, 0x18, 0x00,
  0x00, 0xE0, 0x0F, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0xC1, 0x00, 0x00, 0x0E, 0x61, 0x00,
  0x00, 0x1C, 0x79, 0x00, 0x00, 0x34, 0x29, 0x00, 0x00, 0x28, 0x35, 0x00, 0x00, 0x48, 0x17, 0x00,
  0x00, 0xD8, 0x1B, 0x00, 0x00, 0x90, 0x1B, 0x00, 0x00, 0xB0, 0x09, 0x00, 0x00, 0xA0, 0x05, 0x00,
  0x00, 0xE0, 0x07, 0x00, 0x00, 0xC0, 0x03, 0x00
};

// bad flower
unsigned char bitmap_bad[] U8G_PROGMEM = {
  0x00, 0x80, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xE0, 0x0D, 0x00, 0x00, 0xA0, 0x0F, 0x00,
  0x00, 0x20, 0x69, 0x00, 0x00, 0x10, 0x78, 0x02, 0x00, 0x10, 0xC0, 0x03, 0x00, 0x10, 0xC0, 0x03,
  0x00, 0x10, 0x00, 0x01, 0x00, 0x10, 0x80, 0x00, 0x00, 0x10, 0xC0, 0x00, 0x00, 0x30, 0x60, 0x00,
  0x00, 0x60, 0x30, 0x00, 0x00, 0xC0, 0x1F, 0x00, 0x00, 0x60, 0x07, 0x00, 0x00, 0x60, 0x00, 0x00,
  0x00, 0x60, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xC7, 0x1C, 0x00,
  0x80, 0x68, 0x66, 0x00, 0xC0, 0x33, 0x7B, 0x00, 0x40, 0xB6, 0x4D, 0x00, 0x00, 0xE8, 0x06, 0x00,
  0x00, 0xF0, 0x03, 0x00, 0x00, 0xE0, 0x00, 0x00
};

// Elecrow Logo
  static unsigned char bitmap_logo[] U8G_PROGMEM ={
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0xE0,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x04,0xF8,0xFF,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x08,0xFE,0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x10,0x1F,0xE0,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0xB0,0x07,0x80,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0xE0,0x03,0x00,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0xC0,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x80,0x01,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x60,0x23,0x00,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x70,0xC7,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x70,0x9E,0x0F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x70,0x3C,0xFE,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x70,0x78,0xF8,0x7F,0xF0,0x9F,0x07,0xFE,0x83,0x0F,0xFF,0x00,0x77,0x3C,0x18,0x1C,
  0x70,0xF0,0xE1,0x3F,0xF1,0x9F,0x07,0xFE,0xE1,0x1F,0xFF,0xC3,0xF7,0x3C,0x38,0x0C,
  0x70,0xE0,0x87,0x8F,0xF1,0xC0,0x07,0x1E,0x70,0x3C,0xCF,0xE3,0xE1,0x7D,0x3C,0x0E,
  0x70,0xD0,0x1F,0xC0,0xF1,0xC0,0x03,0x1F,0x78,0x3C,0xCF,0xE3,0xE1,0x7D,0x3C,0x06,
  0xF0,0xB0,0xFF,0xF1,0xF0,0xC0,0x03,0x0F,0x78,0x3C,0xCF,0xF3,0xE0,0x7B,0x3E,0x06,
  0xF0,0x60,0xFF,0xFF,0xF0,0xC6,0x03,0xEF,0x3C,0x80,0xEF,0xF1,0xE0,0x7B,0x3E,0x03,
  0xF0,0xE1,0xFC,0xFF,0xF8,0xCF,0x03,0xFF,0x3C,0x80,0xFF,0xF0,0xE0,0x7B,0x7B,0x01,
  0xE0,0xC3,0xF9,0x7F,0x78,0xC0,0x03,0x0F,0x3C,0x80,0xF7,0xF1,0xE0,0xF9,0xF9,0x01,
  0xE0,0x83,0xE3,0x7F,0x78,0xE0,0x03,0x0F,0x3C,0xBC,0xE7,0xF1,0xE0,0xF9,0xF9,0x00,
  0xC0,0x0F,0x8F,0x3F,0x78,0xE0,0x81,0x0F,0x3C,0x9E,0xE7,0xF1,0xE0,0xF1,0xF8,0x00,
  0x80,0x3F,0x1E,0x00,0x78,0xE0,0x81,0x07,0x38,0x9E,0xE7,0xF1,0xF0,0xF0,0x78,0x00,
  0x80,0xFF,0xFF,0x00,0xF8,0xEF,0xBF,0xFF,0xF8,0xCF,0xE7,0xE1,0x7F,0x70,0x70,0x00,
  0x00,0xFF,0xFF,0x0F,0xF8,0xEF,0xBF,0xFF,0xE0,0xC3,0xE3,0x81,0x1F,0x70,0x30,0x00,
  0x00,0xFC,0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0xF8,0xFF,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0xE0,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  };


static unsigned char bitmap_T[] U8G_PROGMEM = {
  0xF7, 0x01, 0x1D, 0x03, 0x0B, 0x02, 0x0C, 0x02, 0x0C, 0x00, 0x0C, 0x00, 0x0C, 0x00, 0x08, 0x02,
  0x18, 0x03, 0xF0, 0x01
};

static unsigned char bitmap_H[] U8G_PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0xC0, 0x03, 0xE0, 0x07, 0xF0, 0x0F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFC, 0x3F,
  0xFC, 0x3F, 0xFE, 0x7F, 0xEE, 0x7F, 0xB3, 0xF7, 0xBB, 0xFB, 0xBB, 0xFD, 0xBB, 0xFD, 0xC7, 0xFE,
  0x7F, 0xC3, 0x3F, 0xDD, 0xBF, 0xFD, 0xDF, 0xDD, 0xEE, 0x5B, 0xFE, 0x7F, 0xFC, 0x3F, 0xF8, 0x1F,
  0xE0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



void setup()
{
//  draw_elecrow();
  delay(2000);
  Wire.begin();
  RTC.begin();
  Serial.begin(38400);
  delay(2000);

 // RTC.adjust(DateTime(__DATE__, __TIME__));  //die methode adjust einsetzen!
  
  Serial.println(F("\r\nSteuerung der Bewässerungsanlage"));
  Serial.println(F("'Serieller Monitor' zeigt Zeit"));
 
  Serial.println();
  Serial.println(F("Die Zeit setzen mit 'set' im 'Serial Monitor'."));
  Serial.println(F("\r\nBeispiel:"));
  Serial.println(F("set 28.08.2021 10:54\r\n"));
  Serial.println(F("Feuchte-Sollwerte setzen mit 'rH' im 'Serial Monitor'."));
  Serial.println(F("\r\nBeispiel:"));
  Serial.println(F("rH 40.40.40.50\r\n"));

  Serial.println("Zeitprogramme:");
  
  for (int i = 0; i<(sizeof(schaltRelais)/sizeof(schaltRelais_t)); i++)  // Alle Timeprogramme anzeigen
  {
   Serial.print(schaltRelais[i].Valveprognr);
   Serial.print(schaltRelais[i].ein_zeit);
   Serial.print(schaltRelais[i].aus_zeit);
   Serial.print("    ");
  /* 
   * Valveprognr;
  int ein_zeit; 
  int aus_zeit 
   */
   
  }
  Serial.println("_");
 
  /*for (int i=0;i<sizeof(schaltRelais)/sizeof(schaltRelais_t);i++)
  {
    digitalWrite(schaltRelais[i].pin,AUS);
    pinMode(schaltRelais[i].pin,OUTPUT);
  }  
  digitalWrite(timerRelais.pin,AUS);
  pinMode(timerRelais.pin,OUTPUT);
  */
  
  // declare relay as output
for (int i=0; i < 4; i++)
{
  // declare valvepin as output
  pinMode(valvepin[i], OUTPUT);
}
  // declare pump as output
  pinMode(pump, OUTPUT);
  // declare switch as input
  pinMode(button, INPUT);
  //pinMode(ROTARY_ANGLE_SENSOR, INPUT);
  // water_flower();
}

void loop()
{

// übergeordnete und Sicherheitsfunktionen
//Anlagefreigabe =true;

// Bei einem Sensorfehler wird mittels Notprogramm gegossen (Einschalten für eine fixe Zeit (zB 1 Minute pro Tag))
// bei einem Sensorfehler wird Zeitprogramm übersteuert


 // alternatives Setup mit verwendung der Library-Funktionen rtclib
 //  now.hour();
  char buffer[30];
  DateTime now;
  now = RTC.now();
  static unsigned long lastMillis;
  static int lastMinute;
  
  if ((millis()-lastMillis)>1000) // nur einmal pro Sekunde
  { 
    lastMillis=millis(); 
      // aktuelle Zeit ausgeben
      now = RTC.now();
      Serial.print(now.hour()); 
      Serial.print(":"); 
      if (now.minute()<10)
      {Serial.print("0");}
      Serial.print(now.minute()); 
      Serial.print(":"); 
      if (now.second()<10)
      {Serial.print("0");}
      Serial.print(now.second()); 
      Serial.println(" Uhr "); 
      
 if (now.minute()!=lastMinute) // die aktuelle Minute hat gewechselt, einmal pro Minute
    {
      lastMinute=now.minute();
           
      relaisSchaltenNachZeit(now.hour(), now.minute());   // Die Valvetimeprog[i] Flags werden entsprechend gesetzt

/*
  char buffer[30];
  static unsigned long lastMillis;
  static uint8_t lastMinute;
  int stunden, minuten, sekunden, dummy;
  
  if ((millis()-lastMillis)>1000) // nur einmal pro Sekunde
  {
    
    lastMillis=millis();
    rtcReadTime(dummy, dummy, dummy, stunden, minuten, sekunden);
    if (minuten!=lastMinute) // die aktuelle Minute hat gewechselt einmal pro Minute
    {
      lastMinute=minuten;
      snprintf(buffer,sizeof(buffer),"%02d:%02d:%02d Uhr",stunden,minuten,sekunden);
      Serial.println(buffer);
      relaisSchaltenNachZeit(stunden,minuten);   // Die Valvetimeprog[i] Flags werden entsprechend gesetzt
 //     relaisTimerNachZeit(stunden,minuten);
 */  
 /*   Hier werden alle Bewässerungsrelevanten inputs verarbeitet (nach dem Debuggen)
  *    
  */   
    
    }
  }
  behandleSerielleBefehle();


// Zeit debuggen
//      snprintf(buffer,sizeof(buffer),"%02d:%02d:02% Uhr debug",now.hour(), now.minute(), now.second());
/*      now = RTC.now();
      Serial.print(now.hour()); 
      Serial.print(":"); 
      if (now.minute()<10)
      {Serial.print("0");}
      Serial.print(now.minute()); 
      Serial.print(":"); 
      if (now.second()<10)
      {Serial.print("0");}
      Serial.print(now.second()); 
      Serial.println(" Uhr "); 
*/
//      Serial.println(buffer); 

// read input
// read the value from the moisture sensors:
for (int i=0; i < 4; i++)
  { 
  moisture_val[i] = CrowtailMoisture(i, true);
  }


// Processing Data


for (int i=0; i < 4; i++)
{
if (Valvefreigabe[i])
{
   if (Sensorfault[i] < 2)
   {
      if (Valvetimeprog[i])
       {
         if (moisture_val[i] < moisture_min[i])   // Bewässerung mit Hysterese Einschalten
         {        
/*       //  Debuggen
         Serial.print("Flag ON");
         Serial.println(i);
*/
         valve_state_flag[i] = true;
         }
      else if (moisture_val[i]  >  (moisture_min[i] + moisture_hysteresis))   // Bewässerung Ausschalten
        {
        //  Debuggen
        //Serial.print("Flag Off");
        //Serial.println(i);
        valve_state_flag[i] = false;
        }
      }
      else // Zeitprogramm ist aus
      {
        valve_state_flag[i] = false;
      }
      
   } 
   else  // Sensor Error -> Notprogramm
   {
    //Serial.print("Not ON ");
    //Serial.println(i);
    valve_state_flag[i] = Valvetimeprog[4];
    } 
}
else
{
 valve_state_flag[i] = false; 
}
}


// Debuggen ermöglichen /*

for (int i=0; i < 4; i++)
{
Serial.print("A");
Serial.print(i);
Serial.print("= ");
Serial.print(moisture_val[i]);
Serial.print("/");
Serial.print(moisture_min[i]);
Serial.print(" % Err:");

switch (Sensorfault[i]) 
 {
  case 0: Serial.print ("0"); break;
  case 1: Serial.print ("1"); break;
  case 2: Serial.print ("2"); break;
  case 3: Serial.print ("3"); break;
 };
Serial.print(F(" Vent_Frei/Time/State:"));
Serial.print(Valvefreigabe[i]);
Serial.print("/");
Serial.print(Valvetimeprog[i]);
Serial.print("/");
Serial.print(valve_state_flag[i]);
Serial.print(" ");
}
Serial.print(F("Pumpe_Flag: "));
Serial.print(pump_state_flag);
Serial.print("  ");
Serial.println();
// End Debug */



// Output

//Ventile öffnen
// Die Pumpe wird von den Valve_state_flag gesteuert, das Pumpenflag wird zum Schluss gesetzt

for (int i=0; i < 4; i++)
{
if (valve_state_flag[i])
{
 digitalWrite(valvepin[i], HIGH);   // turn the LED on (HIGH is the voltage level)
 }
if ((!valve_state_flag[0]) && (!valve_state_flag[1]) && (!valve_state_flag[2]) && (!valve_state_flag[3])) //Alle Ventile sollen zu sein
    {
     if (pump_state_flag == 0) // Sicherstellen, dass Ventil nur ausgeschaltet wird falls Pumpe aus
      {
        digitalWrite(valvepin[i], LOW);   // turn the LED
      }
    }
else 
    {
        digitalWrite(valvepin[i], valve_state_flag[i]);   //Mindestens 1 Ventil ist offen, also kann beliebig geschaltet werden
     }     
}


Pumpensteuerung();

  int button_state = digitalRead(button);
  if (button_state == 1)
  {
    u8g.firstPage();
    do
    {
    for (int i=0; i < 4; i++) // Feuchte und Flowers ausgeben      
      {
      Fuehlerfeuchteausgabe(i); 
      } 
      drawflower();
    } while ( u8g.nextPage() );
  }
  else
  {
    u8g.firstPage();
    do
    {
      drawtime();
      u8g.drawStr(8, 55 , "elecrow");
    } while (u8g.nextPage());
  }
}

//Set moisture value

  void draw_elecrow(void)
  {
  u8g.setFont(u8g_font_gdr9r);
  u8g.drawStr(8,55 , "elecrow");
  u8g.drawXBMP(0, 5,128,32, bitmap_logo);
  }


void drawtime(void)
{
  int x = 5;
  float i = 25.00;
  float j = 54;
  DateTime now = RTC.now();
  //Serial.print(now.year(), DEC);
  if (! RTC.isrunning())
  {
    u8g.setFont(u8g_font_6x10);
    u8g.setPrintPos(5, 20);
    u8g.print("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  else
  {
    u8g.setFont(u8g_font_7x13);
    u8g.setPrintPos(x, 11);
    u8g.print(now.year(), DEC);
    u8g.setPrintPos(x + 80, 11);
    u8g.print(daysOfTheWeek[now.dayOfTheWeek()]);
    u8g.setPrintPos(x + 28, 11);
    u8g.print("/");
    u8g.setPrintPos(x + 33, 11);
    u8g.print(now.month(), DEC);
    if (now.month() < 10)
      x -= 7;
    u8g.setPrintPos(x + 47, 11);
    u8g.print("/");
    u8g.setPrintPos(x + 53, 11);
    u8g.print(now.day(), DEC);
    u8g.setFont(u8g_font_8x13);
    int x = 35;
    u8g.setPrintPos(x, 33);
    u8g.print(now.hour(), DEC);
    if (now.hour() < 10)
      x -= 7;
    u8g.setPrintPos(x + 15, 33);
    u8g.print(":");
    u8g.setPrintPos(x + 21, 33);
    u8g.print(now.minute(), DEC);
    if (now.minute() < 10)
      x -= 7;
    u8g.setPrintPos(x + 36, 33);
    u8g.print(":");
    u8g.setPrintPos(x + 42, 33);
    u8g.print(now.second(), DEC);
  }
}

void drawLogo(uint8_t d)
{
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(8 + d, 30 + d, "S");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(30 + d, 30 + d, "p");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(40 + d, 30 + d, "r");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(55 + d, 30 + d, "i");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(70 + d, 30 + d, "t");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(85 + d, 30 + d, "z");
  u8g.setFont(u8g_font_gdr25r);
  u8g.drawStr(100 + d, 30 + d, "i");
}


//Style the flowers     bitmap_bad: bad flowers     bitmap_good:good  flowers
void drawflower(void)  //Ausgeben der 4 Blumensymbole
{
// Positionen
// #0 :  0, 0
// #1 : 32, 0
// #2 : 64, 0
// #3 : 96, 0

for (int i = 0; i < 4; i++)
{
// Blume #x
  if (moisture_val[i] < moisture_min[i])
  {
    u8g.drawXBMP(i*32, 0, 32, 30, bitmap_bad);
  }
  else
  {
    u8g.drawXBMP(i*32, 0, 32, 30, bitmap_good);
  }
}
}


void drawTH(void)
{
// Koordinaten festlegen
  #define A  0
  #define B 32
  #define C 64
  #define D 96
  
  char moisture1_value_temp[5] = {0};
  char moisture2_value_temp[5] = {0};
  char moisture3_value_temp[5] = {0};
  char moisture4_value_temp[5] = {0};
//  read_value();
  itoa(moisture_val[0], moisture1_value_temp, 10);
  itoa(moisture_val[1], moisture2_value_temp, 10);
  itoa(moisture_val[2], moisture3_value_temp, 10);
  itoa(moisture_val[3], moisture4_value_temp, 10);
  u8g.setFont(u8g_font_7x14);

  // Ausgeben der Fühlerbezeichnung und des Feuchtigkeitswertes inkl. "%" Zeichen 
  u8g.setPrintPos(9, 60);
  u8g.print("A0");
   if (Sensorfault[0] >= 2)   // Fühler defekt
  {
  u8g.setPrintPos(A + 7, 45 );
  u8g.print("XX");
  }
  else
  { 
  if (moisture_val[0] < 10)                // Feuchtigkeitswert hat 1 Stelle
  {
    //u8g.setPrintPos(A + 14, 45 );
    u8g.drawStr(A + 14, 45, moisture1_value_temp);
//    delay(20);
//    u8g.drawStr(A + 14, 45, moisture1_value_temp);
    
  }
  else if (moisture_val[0] < 100)             // Feuchtigkeitswert hat 2 Stellen
  {
    //u8g.setPrintPos(A + 7, 45);
//    u8g.drawStr(A + 7, 45, moisture1_value_temp);
//    delay(20);
    u8g.drawStr(A + 7, 45, moisture1_value_temp);
   
  }
  else                                       // Feuchtigkeitswert 100% hat 3 Stellen
  {
    //u8g.setPrintPos(A + 2, 45 );
    u8g.drawStr(A + 2, 45, moisture1_value_temp);
  }
  }
  //u8g.print(moisture1_value);
  u8g.setPrintPos(A + 23, 45 );
  u8g.print("%");
  
  // Ausgeben der Fühlerbezeichnung und des Feuchtigkeitswertes inkl. "%" Zeichen 
  u8g.setPrintPos(41, 60 );
  u8g.print("A1");
  if (moisture_val[1] < 10)
  {
    //u8g.setPrintPos(B + 46, 45 );
    u8g.drawStr(B + 14, 45, moisture2_value_temp); 
    delay(20);
    u8g.drawStr(B + 14, 45, moisture2_value_temp); 
  }
  else if (moisture_val[1] < 100)
  {
    //u8g.setPrintPos(B + 39, 45);
    u8g.drawStr(B + 7, 45, moisture2_value_temp);
    delay(20);
    u8g.drawStr(B + 7, 45, moisture2_value_temp);
  }
  else
  {
   u8g.drawStr(B + 2, 45, moisture2_value_temp);
  }
  u8g.setPrintPos(B + 23, 45);
  u8g.print("%");
  
   // Ausgeben der Fühlerbezeichnung und des Feuchtigkeitswertes inkl. "%" Zeichen 
  u8g.setPrintPos(73, 60);
  u8g.print("A2");
  if (moisture_val[2] < 10)
  {
    //u8g.setPrintPos(C + 14, 45 );
    u8g.drawStr(C + 14, 45, moisture3_value_temp);
    delay(20);
    u8g.drawStr(C + 14, 45, moisture3_value_temp);
    
  }
  else if (moisture_val[2] < 100)
  {
    // u8g.setPrintPos(C + 7, 45);
   u8g.drawStr(C + 7, 45, moisture3_value_temp);
   delay(20);
   u8g.drawStr(C + 7, 45, moisture3_value_temp);
    
  }
  else
  {
//    moisture_val[2] = 100;
//    itoa(moisture_val[2], moisture3_value_temp, 10);
    u8g.drawStr(C + 2, 45, moisture3_value_temp);
  }
  //u8g.print(moisture3_value);
  u8g.setPrintPos(C + 23, 45);
  u8g.print("%");
  
// Ausgeben der Fühlerbezeichnung und des Feuchtigkeitswertes inkl. "%" Zeichen   
  u8g.setPrintPos(105, 60);
  u8g.print("A3");
  if (moisture_val[3] < 10)
  {
    u8g.drawStr(D + 14, 45, moisture4_value_temp);
    delay(20);
    u8g.drawStr(D + 14, 45, moisture4_value_temp);
   
  }
  else if (moisture_val[3] < 100)
  {
    u8g.drawStr(D + 7, 45, moisture4_value_temp);
    delay(20);
    u8g.drawStr(D + 7, 45, moisture4_value_temp);
  
  }
  else
  {
    moisture_val[3] = 100;
    itoa(moisture_val[3], moisture4_value_temp, 10);
    u8g.drawStr(D + 2, 45, moisture4_value_temp);
  }
  //u8g.print(moisture4_value);
  u8g.setPrintPos(D + 23, 45);
  u8g.print("%");
}


int CrowtailMoisture(uint8_t index, boolean Mappen)
{
// Einlesen der Werte der Feuchtesensoren
// zusätzliches Mapping der Werte auf eine Skala von 0..100
// Mapping with Standard values (without Calibration of Sensors)
// function returns the analog to digital converter value (0 - 1023).
// or the mapped value 0…100
// Results are displayed on the serial monitor for debugging
// @param pin is the analog pin number to be read.
// CrowtailMoisture(2);
// Detection of missing or faulty sensor
//  Konzept:  Wert einlesen
//      internen Pullup einschalten
//      Wert_b einlesen
//    
//      Auswerten:  Wert_b -Wert > Zul_Schwankung -> alles OK
//         
//          (Rechnen mit Ri Sensor und R Pullup)
//          Wert > MaxSensorreading && Wert_B >= (1023-Minimum_Differenz) -> Sensor fehlt
//          Wert_B <= Minimum_Differenz -> Sensor KS
//

//constants for sensor-Mapping
#define rawvalueair   560
#define rawvaluewater 280
#define minpothumidity 5
#define maxpothumidity 100
#define Minimum_Differenz 10    // Guess... (evtl. Rechnen mit Ri Sensor und R Pullup Rpu = dU/dI = 1V/30uA = 33kOhm)
#define MaxPullup 1000           // kleinstes beobachtetes Maximum 1005 ohne Sensor
#define sensorfaultdetection true
 
 int pinValue = analogRead(moisture_pin[index]);
 int moisture_value;
 int pinValueb;

if (sensorfaultdetection)
{
  pinMode(moisture_pin[index],INPUT_PULLUP);
  delay(40);
  pinValueb  = analogRead(moisture_pin[index]);
  Sensorfault[index] = 0;
  
if ((pinValueb - pinValue) < Minimum_Differenz)    // Die Quelle am Eingang ist niederohmig ... Fixe Spannung anstelle R-Spannungsteiler
{
 Sensorfault[index] = 1;
}

if ( (pinValueb >= (MaxPullup)))    // (pinValue > Minimum_Differenz) or..  pinValue ist zu nicht Null (??Schwankend) und Pullup bringt Maximalen Wert -> Sensor fehlt
{
  Sensorfault[index] = 2;
} 

if ((pinValue <= Minimum_Differenz) and (pinValueb <= Minimum_Differenz))    //   Sensor KS, Der Eingangswert ist sowohl ohne, wie auch mit Pullup unterhalb Minimum
{
 Sensorfault[index] = 3;
} 
  digitalWrite(moisture_pin[index],LOW);  // Pullup ausschalten
  delay(20);
}
 
 if (Mappen)
{
  moisture_value = map(pinValue,rawvalueair,rawvaluewater,minpothumidity,maxpothumidity); delay(20);
  if(moisture_value<0)
  {
    moisture_value=0;
  }
  if(moisture_value>100)
  {
    moisture_value=100;
  }
}
else
{
moisture_value = pinValue;
}

  ///////////////
  // Print DATA //
  ///////////////



  // Echo to serial
 // Debuggen der Funktion zum Einlesen der Fühler 
 /* 
  Serial.print(index);
  Serial.print("_"); 
  
  Serial.print(moisture_pin[index]);
 Serial.print(" : ");
 Serial.print(pinValue);
 Serial.print("/");
 Serial.print(pinValueb); 
 Serial.print(" E: ");
 switch (Sensorfault[index]) 
 {
  case 0: Serial.print ("0"); break;
  case 1: Serial.print ("1"); break;
  case 2: Serial.print ("2"); break;
  case 3: Serial.print ("3"); break;
 };
 Serial.print(", ");
// */ 
return moisture_value;
 }


void Pumpensteuerung(void){

// Wenn alle geschlossen werden, muss die Pumpe vorgängig ausgeschaltet werden
    if ((!valve_state_flag[0]) && (!valve_state_flag[1]) && (!valve_state_flag[2]) && (!valve_state_flag[3]))
    {
      if (pump_state_flag == 1)
      {
      digitalWrite(pump, LOW);
      pump_state_flag = 0;
      delay(50);
      }
}

//Pumpe Steuern
// Check if any Valves are open...switch Pump On / Off

 delay(50);
    if ((valve_state_flag[0]) || (valve_state_flag[1] ) || (valve_state_flag[2]) || (valve_state_flag[3]))
    {
    if (pump_state_flag == 0)
    {
      digitalWrite(pump, HIGH);
      pump_state_flag = 1;
      delay(50);
    }
    delay(50);
      }  
}



void behandleSerielleBefehle()
//   Beispiel Serial.println("set 28.08.2021 10:54\r\n");

{
  char linebuf[30];
  byte counter;
  if (Serial.available())
  {
    delay(100); // Warte auf das Eintreffen aller Zeichen vom seriellen Monitor
    memset(linebuf,0,sizeof(linebuf)); // Zeilenpuffer löschen
    counter=0; // Zähler auf Null
    while (Serial.available())
    {
      linebuf[counter]=Serial.read(); // Zeichen in den Zeilenpuffer einfügen
      if (counter<sizeof(linebuf)-1) counter++; // Zeichenzähler erhöhen
    }
    // Ab hier ist die Zeile eingelesen
    //Serial.println(linebuf)
    if (strstr(linebuf,"set")==linebuf) // Prüfe auf Befehl "set" zum Setzen der Zeit
    { // Alle übermittelten Zahlen im String auslesen
      tage=getIntFromString (linebuf,1);
      monate=getIntFromString (linebuf,2);
      jahre=getIntFromString (linebuf,3);
      stunden=getIntFromString (linebuf,4);
      minuten=getIntFromString (linebuf,5);
      sekunden=getIntFromString (linebuf,6);
 // Ausgeben der Werte zum Debuggen
      Serial.println(linebuf);
      Serial.print(tage);
      Serial.print(".");
      Serial.print(monate);
      Serial.print(".");
      Serial.print(jahre);
      Serial.print(" ");
      Serial.print(stunden);
      Serial.print(":");
      Serial.println(minuten);
    
 
    
    // Ausgelesene Werte einer groben Plausibilitätsprüfung unterziehen:
    if (jahre<2000 || monate<1 || monate>12 || tage<1 || tage>31 || (stunden+minuten)==0)
    {
     
      Serial.println(F("\r\nFehlerhafte Zeitangabe im 'set' Befehl"));
      Serial.println(F("\r\nBeispiel:"));
      Serial.println(F("set 28.08.2013 10:54\r\n"));
      return;
    }
     rtcWriteTime(jahre, monate, tage, stunden, minuten, sekunden);   
    }
    else if (strstr(linebuf,"rH")==linebuf) // Prüfe auf Befehl "rH" zum Setzen der Zeit
    { // Alle übermittelten Zahlen im String auslesen
      moisture_min[0] = getIntFromString (linebuf,1);
      moisture_min[1] = getIntFromString (linebuf,2);
      moisture_min[2] = getIntFromString (linebuf,3);
      moisture_min[3] = getIntFromString (linebuf,4);
    if (moisture_min[0]>80 || moisture_min[1]>80 || moisture_min[2]>80 || moisture_min[3]>80 ) // Der fehlerhafte Wert wird gesetzt !
      {
      Serial.println(linebuf);
      Serial.println(F("\r\nFehlerhafte Feuchte im 'rH' Befehl"));
      Serial.println(F("\r\nBeispiel:"));
      Serial.println(F("rH 40.40.40.40 \r\n"));
      return;
      }      
    }
    else
    {
      Serial.println(F("Befehl unbekannt."));
      return;
    } 
    
    
    Serial.println(F("Zeit und Datum oder rH wurden auf neue Werte gesetzt."));
  }
}


/*
void relaisTimerNachZeit(int thishour, int thisminute)
{
  int thisTime= thishour*100+thisminute;
  if (thisTime==timerRelais.timer1 || thisTime==timerRelais.timer2)
  {
    Serial.println("Timer Start");
 //   digitalWrite(timerRelais.pin,EIN);
    valve_kurzzeit[0] = true; // Schaltzustand setzen 
    delay(timerRelais.dauer*1000L);
    valve_kurzzeit[0] =false; // Schaltzustand setzen 
//    digitalWrite(timerRelais.pin,AUS);
    Serial.println("Timer Stopp");
  }
}
*/


void relaisSchaltenNachZeit(int thishour, int thisminute)   

// Zeile der Schaltzeit enthält; Valveprognr, einzeit, auszeit
// Mehrere Zeiten können gesetzt werden
// Die Zeiten können über Mitternacht gesetzt werden
// Zeiten dürfen sich nicht überlappen bzw die zuunterst stehende Zeit wird angewendet

// Schaltet die Zeitschaltuhr ein und aus und setzt den Ausgang entsprechend
{
  for (int k=0; k<5;k++) //Alle Zeitprogramme
{
  Valvetimeprog[k] = 0;
}  
  // Aus der aktuellen Zeit eine Schaltzeit bilden
  int thisTime= thishour*100+thisminute;
  
  // Alle Schaltzeiten durchgehen, falls eine davon EIN sagt, einschalten
  
  Serial.print(F("thisTime: "));
  Serial.println(thisTime);
  
  for (int i=0; i<(sizeof(schaltRelais)/sizeof(schaltRelais_t));i++)   //Alle Zeitprogramme abarbeiten
  {
//     state =Valvetimeprog[schaltRelais[i].Valveprognr];
    if (schaltRelais[i].ein_zeit < schaltRelais[i].aus_zeit)  // normale Schaltzeit von bis
    { 
      if (thisTime>=schaltRelais[i].ein_zeit && thisTime<schaltRelais[i].aus_zeit)
        {
          Valvetimeprog[schaltRelais[i].Valveprognr] = EIN_SCHALTEN;
          Serial.print(F("Schaltprog_Zeile: "));
          Serial.print(i);
          
          Serial.print("Ein");
          Serial.print("");
        }                
    }   
    else if (schaltRelais[i].ein_zeit > schaltRelais[i].aus_zeit) // Schaltzeit über Mitternacht hinweg wenn die Einschaltzeit größer als die Ausschaltzeit ist
    { 
      if (thisTime>=schaltRelais[i].ein_zeit || thisTime<schaltRelais[i].aus_zeit)
        {
          Valvetimeprog[schaltRelais[i].Valveprognr] = EIN_SCHALTEN;
        }      
    }
    
    
    else ; // gleiche Ein- und Ausschaltzeit ignorieren
    {
     /*if (Valvetimeprog[i]!=state) // Falls geschaltet werden soll, ein paar Debug-Ausgaben machen
    { 
       Serial.print("Ventil TimFlag/StateFlag: ");
       Serial.print (i);
       if (Valvetimeprog[i]==EIN_SCHALTEN) 
        {Serial.print (":1"); 
        } 
       else Serial.print(":0");
       if (valve_state_flag[i]==EIN_SCHALTEN) 
       {Serial.print ("/1 "); 
       } 
       else Serial.print("/0 ");    
     }*/        
}

}
Serial.print(F("Zeitprog: "));
for (int k=0; k<5;k++) //Alle Zeitprogramme
{
Serial.print(Valvetimeprog[k]);
}
          
Serial.println(F("Endsub"));
}  

/*
void rtcReadTime(int &jahre, int &monate, int &tage, int &stunden, int &minuten, int &sekunden)
// aktuelle Zeit aus RTC auslesen
{
// Reset the register pointer
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(RTC_I2C_ADDRESS, 7);
  // A few of these need masks because certain bits are control bits
  sekunden    = bcdToDec(Wire.read() & 0x7f);
  minuten     = bcdToDec(Wire.read());
  stunden     = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
//wochentag   = bcdToDec(Wire.read());
  tage        = bcdToDec(Wire.read());
  monate      = bcdToDec(Wire.read());
  jahre       = bcdToDec(Wire.read())+2000;  
}
*/
void rtcWriteTime(int jahre, int monate, int tage, int stunden, int minuten, int sekunden)
// aktuelle Zeit in der RTC speichern
{
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write(0);
  Wire.write(decToBcd(sekunden));    // 0 to bit 7 starts the clock
  Wire.write(decToBcd(minuten));
  Wire.write(decToBcd(stunden));      // If you want 12 hour am/pm you need to set
                                   // bit 6 (also need to change readDateDs1307)
                                  
  Wire.write(decToBcd(0)); // Wochentag unberücksichtigt
  Wire.write(decToBcd(tage));
  Wire.write(decToBcd(monate));
  Wire.write(decToBcd(jahre-2000));
  Wire.endTransmission();  
}

byte decToBcd(byte val) // Hilfsfunktion zum Lesen/Schreiben der RTC
// Convert decimal number to binary coded decimal
// Hilfsfunktion für die Echtzeituhr
{
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val)  // Hilfsfunktion zum Lesen/Schreiben der RTC
// Convert binary coded decimal to decimal number
// Hilfsfunktion für die Echtzeituhr
{
  return ( (val/16*10) + (val%16) );
}


int getIntFromString (char *stringWithInt, byte num)
// input: pointer to a char array
// returns an integer number from the string (positive numbers only!)
// num=1, returns 1st number from the string
// num=2, returns 2nd number from the string, and so on
{
  char *tail; 
  while (num>0)
  {
    num--;
    // skip non-digits
    while ((!isdigit (*stringWithInt))&&(*stringWithInt!=0)) stringWithInt++;
    tail=stringWithInt;
    // find digits
    while ((isdigit(*tail))&&(*tail!=0)) tail++;
    if (num>0) stringWithInt=tail; // new search string is the string after that number
  }  
  return(strtol(stringWithInt, &tail, 10));
}  


void Fuehlerfeuchteausgabe(uint8_t fuehlernr)   //Ausgabe in einer for Schleife... Fuehler 0..3
// Ausgeben der Fühlerbezeichnung und des Feuchtigkeitswertes inkl. "%" Zeichen
{
 // Koordinaten festlegen
//#define A 0
//#define B 32
//#define C 64
//#define D 96
  
  uint8_t moisture_value_temp[5] = {0};
  uint8_t fuehler_str[2] ={0};
//  read_value();

  itoa(fuehlernr, fuehler_str, 2);
  
  itoa(moisture_val[fuehlernr], moisture_value_temp, 10);
    
  u8g.setFont(u8g_font_7x14); 
 
  u8g.setPrintPos((fuehlernr * 32 + 9), 60);  // Initialposition
 
  u8g.print("A");
  u8g.print(fuehlernr); 
 if (Sensorfault[fuehlernr] >= 2)   // Fühler defekt
{
  u8g.setPrintPos((fuehlernr*32) + 7, 45 );
  u8g.print("XX");
  }
  else
  {
  if (moisture_val[fuehlernr] < 10)                   // Feuchtigkeitswert hat 1 Stelle
  {
    //u8g.setPrintPos((fuehlernr*32) + 14, 45 );
    u8g.drawStr((fuehlernr*32) + 14, 45, moisture_value_temp);
    delay(20);
    u8g.drawStr((fuehlernr*32) + 14, 45, moisture_value_temp);
    
  }
  else if (moisture_val[fuehlernr] < 100)             // Feuchtigkeitswert hat 2 Stellen
  {
    //u8g.setPrintPos((fuehlernr*32)A + 7, 45);
    u8g.drawStr((fuehlernr*32) + 7, 45, moisture_value_temp);
    delay(20);
    u8g.drawStr((fuehlernr*32) + 7, 45, moisture_value_temp);
   
  }
  else if (moisture_val[fuehlernr] < 100)                                    // Feuchtigkeitswert 100% hat 3 Stellen
  {
    //u8g.setPrintPos((fuehlernr*32) + 2, 45 );
    moisture_val[fuehlernr] = 100;
    itoa(moisture_val[fuehlernr], moisture_value_temp, 10);
    u8g.drawStr((fuehlernr*32) + 2, 45, moisture_value_temp);
  }
  }
  //u8g.print(moisture_value);
  u8g.setPrintPos((fuehlernr*32) + 23, 45 );
  u8g.print("%");
  
 } 
 




// -----------------Obsolet
/*void Blumengiessen (uint8_t nummer){

// Wenn alle geschlossen werden, muss die Pumpe vorgängig ausgeschaltet werden
    if ((!valve_state_flag[0]) && (!valve_state_flag[1]) && (!valve_state_flag[2]) && (!valve_state_flag[3]))
    {
      if (pump_state_flag == 1)
      {
      digitalWrite(pump, LOW);
      pump_state_flag = 0;
      delay(50);
      }
}

//Ventile öffnen
for (int i=0; i < 4; i++)
{
valvepin[i] = (valve_state_flag[i]);
}
//Pumpe Steuern
// Check if any Valves are open...switch Pump On / Off

 delay(50);
    if ((valve_state_flag[0]) || (valve_state_flag[1] ) || (valve_state_flag[2]) || (valve_state_flag[0]))
    {
    if (pump_state_flag == 0)
    {
      digitalWrite(pump, HIGH);
      pump_state_flag = 1;
      delay(50);
    }
    delay(50);
      }
   
}
*/
