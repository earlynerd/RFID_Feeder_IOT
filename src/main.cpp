#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "secrets.h"
#include "ThingSpeak.h"
#include "Rfid134.h"
#include <pico/stdlib.h>
#include <Adafruit_TinyUSB.h>

#define RESET_INTERVAL 400
#define CAT_TIMEOUT 3000
char testchip[] = {"900215007537252"};

char GnocchiTag[] = {"981020037133435"};
char ZitiTag[] = {"981020037140619"};

int readerResetPin = 2;

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password

unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;

WiFiClient client;
uint32_t wifiStartTime = 0;

enum Cat
{
  GNOCCHI,
  ZITI,
  OTHER
};

struct RFIDReaderState
{
  enum ReaderState
  {
    NO_TAG,
    TAG_PRESENT,
  } readerState;
  Rfid134Reading tag;
  uint32_t tagMillis;
  bool newTag;
};

volatile RFIDReaderState reader_state;

class RfidNotify
{
public:
  static void OnError(Rfid134_Error errorCode)
  {
    // see Rfid134_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }

  static void OnPacketRead(const Rfid134Reading &reading)
  {
    char temp[16];

    // Serial.print("TAG: ");

    // since print doesn't support leading zero's, use sprintf

    // Serial.print(" ");

    // since print doesn't support leading zero's, use sprintf
    // since sprintf with AVR doesn't support uint64_t (llu/lli), use /% trick to
    // break it up into equal sized leading zero pieces
    sprintf(temp, "%03u%06lu%06lu\0", reading.country, static_cast<uint32_t>(reading.id / 1000000), static_cast<uint32_t>(reading.id % 1000000));
    reader_state.tag;
    memcpy((void *)&reader_state.tag, (void *)&reading, sizeof(Rfid134Reading));
    memcpy((char *)&reader_state.tag.idString, &temp, 16);
    reader_state.newTag = true;
    reader_state.tagMillis = millis();
    reader_state.readerState = RFIDReaderState::TAG_PRESENT;
    // Serial.println(temp);
  }
};

void resetReader();
void tagUpdate();
void catDetection();
void blinkSuccess();
int logCatVisit(int field, float seconds);

Rfid134<HardwareSerial, RfidNotify> rfid(Serial1);

void setup()
{
  Serial.begin(115200);
  //while (!Serial)
  //  yield();
  Serial.println("initializing...");
  Serial.flush();
  pinMode(readerResetPin, INPUT);
  // digitalWrite(readerResetPin, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // due to design differences in (Software)SerialConfig that make the serial.begin
  // method inconsistent between implemenations, it is required that the sketch
  // call serial.begin() that is specific to the platform
  //
  // hardware
  Serial1.setRX(1);
  Serial1.setTX(0);
  Serial1.begin(9600, SERIAL_8N2);
  // software ESP
  // secondarySerial.begin(9600, SWSERIAL_8N2);
  // software AVR
  // secondarySerial.begin(9600);
  reader_state.readerState = RFIDReaderState::NO_TAG;
  rfid.begin();
  
  Serial.println("starting...");

  Serial.println("wifi begin");
  wifiStartTime = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - wifiStartTime < 10000))
    ;
  digitalWrite(LED_BUILTIN, LOW);
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("wifi connected");
  delay(200);
  digitalWrite(LED_BUILTIN, HIGH);
  ThingSpeak.begin(client); // Initialize ThingSpeak
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
  rfid.loop();
  tagUpdate();
  catDetection();
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to reconnect to SSID: ");
    Serial.println(SECRET_SSID);
    WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
    wifiStartTime = millis();
    while ((WiFi.status() != WL_CONNECTED) && (millis() - wifiStartTime < 8000))
    {
      Serial.print(".");
      delay(250);
    }
    if (WiFi.status() == WL_CONNECTED)
      Serial.println("\nConnected.");
    else
      Serial.println("Reconnect fail");
  }
}

void tagUpdate()
{
  static uint32_t lastReset = millis();
  if (reader_state.readerState == RFIDReaderState::NO_TAG) // if theres no tag, turn led off anf go home
  {
    digitalWrite(LED_BUILTIN, LOW);
    return;
  }
  // if a tag has been detected, reset the reader and see if its detected again. over and over until it isnt detected
  if (reader_state.readerState == RFIDReaderState::TAG_PRESENT) // redundant
  {
    digitalWrite(LED_BUILTIN, HIGH);
    if (millis() - lastReset > RESET_INTERVAL)
    {
      resetReader();
      lastReset = millis();
    }
    if (millis() - reader_state.tagMillis > CAT_TIMEOUT)
    {
      reader_state.readerState = RFIDReaderState::NO_TAG;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  // if its been too long since last detect, set the state to no tag so we can end this game
  // if
}

void catDetection()
{
  static Cat lastCat;
  static bool catPresent = false;
  static uint32_t arrivalTime;
  if ((reader_state.readerState == RFIDReaderState::TAG_PRESENT) && (catPresent == false)) // cat arrival
  {
    catPresent = true;
    arrivalTime = millis();
    Serial.print("Cat detected! it is");
    if (strcmp(testchip, (const char *)&reader_state.tag.idString) == 0)
    {
      Serial.println(" Unknown Cat");
      lastCat = Cat::OTHER;
    }
    else if (strcmp(GnocchiTag, (const char *)&reader_state.tag.idString) == 0)
    {
      Serial.println(" Gnocchi.");
      lastCat = Cat::GNOCCHI;
    }
    else if (strcmp(ZitiTag, (const char *)&reader_state.tag.idString) == 0)
    {
      Serial.println(" Ziti.");
      lastCat = Cat::ZITI;
    }
  }
  if ((reader_state.readerState == RFIDReaderState::NO_TAG) && (catPresent == true)) // cat departure
  {
    catPresent = false;
    uint32_t departureTime = millis();
    uint32_t dinnerTime = departureTime - arrivalTime;
    float dinnerTimeSeconds = (float)dinnerTime / 1000.0;
    switch (lastCat)
    {
    case Cat::GNOCCHI:
    {
      Serial.print("Gnocchi has departed, after ");
      Serial.print(dinnerTimeSeconds, 2);
      Serial.println(" seconds.");
      if(logCatVisit(1, dinnerTimeSeconds) == 200) blinkSuccess();
    }
    break;
    case Cat::ZITI:
    {
      Serial.print("Ziti has departed after ");
      Serial.print(dinnerTimeSeconds, 2);
      Serial.println(" seconds.");
      if(logCatVisit(2, dinnerTimeSeconds) == 200) blinkSuccess();
    }
    break;
    case Cat::OTHER:
    {
      Serial.print("Unknown Cat has departed after ");
      Serial.print(dinnerTimeSeconds, 2);
      Serial.println(" seconds.");
      if(logCatVisit(3, dinnerTimeSeconds) == 200) blinkSuccess();
    }
    break;
    };
  }
}

void blinkSuccess()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

int logCatVisit(int field, float seconds)
{
  //ThingSpeak.setField(field, seconds);
  String duration = String(seconds, 3);
 
  int x = ThingSpeak.writeField(myChannelNumber, field, duration, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Thingspeak Log entry Sucess.");
  }
  else
  {
    Serial.println("Problem updating thingspeak channel. HTTP error code " + String(x));
  }
  return x;
}

void resetReader()
{
  pinMode(readerResetPin, OUTPUT);
  digitalWrite(readerResetPin, LOW);
  delay(10);
  digitalWrite(readerResetPin, HIGH);
  pinMode(readerResetPin, INPUT);
}
