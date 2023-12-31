#include <Arduino.h>
#include <Rfid134.h>
#include <pico/stdlib.h>
#include <Adafruit_TinyUSB.h>

char testchip[] = {"900215007537249"};
volatile Rfid134Reading microchip;


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

    Serial.print("TAG: ");

    // since print doesn't support leading zero's, use sprintf


    //Serial.print(" ");

    // since print doesn't support leading zero's, use sprintf
    // since sprintf with AVR doesn't support uint64_t (llu/lli), use /% trick to
    // break it up into equal sized leading zero pieces
    sprintf(temp, "%03u%06lu%06lu\0", reading.country, static_cast<uint32_t>(reading.id / 1000000), static_cast<uint32_t>(reading.id % 1000000));
    memcpy((void*)&microchip, (void*)&reading, sizeof(Rfid134Reading));
    memcpy((char*)&microchip.idString, &temp, 16);
    Serial.println(temp);
  }
};

Rfid134<HardwareSerial, RfidNotify> rfid(Serial1);


void setup()
{
  Serial.begin(115200);
  while(!Serial) yield();
  Serial.println("initializing...");
  Serial.flush();
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
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

  rfid.begin();
  
  Serial.println("starting...");
}

void loop()
{
  rfid.loop();
  if(strcmp(testchip, (const char*)&microchip.idString) == 0) digitalWrite(LED_BUILTIN, HIGH);
  else digitalWrite(LED_BUILTIN, LOW);
}
