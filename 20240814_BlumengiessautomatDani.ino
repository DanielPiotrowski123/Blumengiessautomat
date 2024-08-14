// #define NETZWERKNAME "WN-8B6CB0"
#define NETZWERKNAME "TobiiGuest"
// #define PASSWORT "28d8b36f74"
#define PASSWORT "UseyourEyes2001"
#define MQTT_BROKER "raspberrypi.local"
#define MQTT_TOPIC_OUT "ESP/Pflanze1/Output/"
#define MQTT_TOPIC_IN "ESP/Pflanze1/Input/#"
#define MQTT_CLIENT "ESP_Pflanze1"

#define PUMPE 5 //original #define PUMPE D7 → mit D7 geht es nicht. Digital Ausgang z.B. "5" entsprechend dem Setup anpassen.
#define PAUSENZEIT 300  //min. 5 Minuten (300 s) Pause
//                         //zwischen Pumpvorgängen
#include "ESP8266WiFi.h"
#include "PubSubClient.h"

WiFiClient WLAN_Client;
PubSubClient MQTT_Client(WLAN_Client);

unsigned int PumpDauer = 5;
unsigned long LoopTimer = 0, TagesSumme = 0, PumpTimer = 0, PumpSumme = 0, MaximalSumme = 0, letzterPumpvorgang = 0, Tagesbeginn = 0;
unsigned int Grenzwert = 0;
boolean PumpeEin = false;

void setup() {

  Serial.begin(115200); // original 115200
  
  pinMode(PUMPE, OUTPUT);
  delay(100);

  WiFi.begin(NETZWERKNAME, PASSWORT);

//-------------------------------------------------------------
Serial.println("Connecting to WiFi");

  // Wait for the connection to complete
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
        // Optionally, add a timeout to avoid infinite loops
    if (attempts > 40) { // 40 attempts = 20 seconds
      Serial.println("Failed to connect to WiFi");
      return;
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  // Print the local IP address assigned by the router

//-------------------------------------------------------------



  //while (WiFi.status() != WL_CONNECTED)
    //delay(500);

  MQTT_Client.setServer(MQTT_BROKER, 1883);
  
}



void loop() {
  //Serial.println("Test 2 20240814"); // zum testen

  WLAN_reconnect();
  MQTT_reconnect();
  MQTT_Client.loop();

  if (millis() > LoopTimer)  //Übertragungender
                             //aktuellen Daten alle 10 s
  {
    int Feuchte = analogRead(A0);
    MQTT_publish("Feuchte", String(Feuchte));
    // (2)
    String Letzter = "letzter Pumpgang vor "
                     + String((millis() - letzterPumpvorgang) / 60000ul)  // (3)
                     + "Minuten";
    MQTT_publish("Letzter", Letzter);

    String Summe = "insgesamt "
                   + String(TagesSumme)
                   + " Sekunden in den letzten "
                   + String((millis() - Tagesbeginn) / 3600000ul)
                   + " Stunden gepumt";
    MQTT_publish("TagesSumme", Summe);

    MQTT_publish("Pumpdauer", String(PumpDauer));
    MQTT_publish("Grenzwert", String(Grenzwert));
    MQTT_publish("Maximalsumme", String(MaximalSumme));

    if (Feuchte > Grenzwert
        && (millis() - letzterPumpvorgang) > PAUSENZEIT * 1000
        && (TagesSumme + PumpDauer) < MaximalSumme) {
      PumpTimer = millis() + 10 * 1000;  // 10 gleich Pumpzeit
                                         // (4)
    }
    if ((millis() - Tagesbeginn) / (24 * 60 * 60 * 1000) > 0)
    // Nach 24 Stunden
    {                          // wird der
      Tagesbeginn = millis(),  //Tageszähler

        TagesSumme = 0;  // zurückgesetzt.
    }

    LoopTimer = millis() + 10000;  // alle 10 Sekunden
                                   // erfolgt die
  }                                // erneute
                                   // Messwertübertragung

  if (millis() <= PumpTimer && !PumpeEin)  // Wenn der Wert von
                                           // PumpTimer in der
                                           // Zukunft liegt und die PumpeEin aktuell
                                           // ausgeschaltet ist,
  {
    digitalWrite(PUMPE, HIGH);  // Pumpe einschalten
    PumpeEin = true;
    MQTT_publish("Pumpe", "1");  // und entsprechend
                                 // Meldung abschicken.
    letzterPumpvorgang = millis();
  }
  if (millis() > PumpTimer && PumpeEin)  // ist der Zeitpunkt
                                         // von PumpTimer
  {                                      //erreichte und die
                                         // Pumpe noch aktiv,
    digitalWrite(PUMPE, LOW);            // Pumpe ausschalten
    PumpeEin = false;
    MQTT_publish("Pumpe", "0");  // und entsprechende
                                 // Meldung abschicken.
    TagesSumme += (millis() - letzterPumpvorgang) / 1000;
    letzterPumpvorgang = millis();
  }
}

void WLAN_reconnect() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(NETZWERKNAME, PASSWORT);

    while (WiFi.status() == WL_IDLE_STATUS)
      delay(100);
  }
}

void MQTT_publish(String Topic, String Message)
// (6)
{
  char ZwischenspeicherA[100], ZwischenspeicherB[100];
  Topic = String(MQTT_TOPIC_OUT) + Topic;
  Topic.toCharArray(ZwischenspeicherA, 100);
  Message.toCharArray(ZwischenspeicherB, 100);
  MQTT_Client.publish(ZwischenspeicherA, ZwischenspeicherB);
  delay(50);
}  // (7)

void MQTT_reconnect()
{
  if(MQTT_Client.connected())
    return;
    
  while (!MQTT_Client.connected())
  {
    Serial.println("Verbinde mit MQTT...");
    if(!MQTT_Client.connect(MQTT_CLIENT))
    {
      Serial.print("Fehler: ");
      Serial.print(MQTT_Client.state());
      Serial.println(" Neuer Versuch in 5 Sekunden.");   
      delay(2000);
    }
  }

MQTT_Client.setCallback(MQTT_receive);
MQTT_Client.subscribe(MQTT_TOPIC_IN);
}

void MQTT_receive(char* Topic, byte* Nutzerdaten, unsigned int Laenge) 
{
  String STopic = Topic;  // in String wandeln

  Serial.println("### MQTT Message wurde empfangen ###");
  Serial.print("Topic: ");
  Serial.println(STopic);
  Serial.print("Nutzdaten: ");

  int Zahlenwert = 0;
  for (int i = 0; i < Laenge; i++)
   {
    Serial.print(char(Nutzerdaten[i]));
    Zahlenwert = Zahlenwert * 10 + (Nutzerdaten[i] - '0');
  }


  if (STopic.endsWith("Pumpdauer"))
    
  {
    PumpDauer = Zahlenwert;
    MQTT_publish("Pumpdauer", String(PumpDauer));
  }

  if (STopic.endsWith("Pumpe")) 
  {
    if (Zahlenwert == 1) 
    {
      PumpTimer = millis() + PumpDauer * 1000;
      TagesSumme += PumpDauer;
    }
    if (Zahlenwert == 0)
      PumpTimer = 0;
  }

  if (STopic.endsWith("Grenzwert"))
   {
    Grenzwert = Zahlenwert;
    MQTT_publish("Grenzwert", String(Grenzwert));
  }

  if (STopic.endsWith("Maximalsumme"))
   {
    MaximalSumme = Zahlenwert;
    MQTT_publish("Maximalsumme", String(MaximalSumme));
  }
}