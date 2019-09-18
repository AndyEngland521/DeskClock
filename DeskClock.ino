//Included in IDE/ESP32 Native Libraries
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Timezone.h>     //https://github.com/JChristensen/Timezone
#include <FastLED.h>

#define CLOCK_PIN 17
#define DATA_PIN 16

const uint8_t NUM_LEDS = 60;

CRGB clockLight[NUM_LEDS];

const char ssid[] = "esp32devnet";
const char pass[] = "password";

//Define Daylight Savings Time Rules
//US Mountain Time Zone
TimeChangeRule myDST = {"MDT", Second, Sun, Mar, 2, -360};    //Daylight time = UTC - 6 hours
TimeChangeRule mySTD = {"MST", First, Sun, Nov, 2, -420};     //Standard time = UTC - 7 hours
Timezone myTZ(myDST, mySTD);

TimeChangeRule *tcr;    //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

// NTP Server:
static const char ntpServerName[] = "us.pool.ntp.org";

WiFiUDP Udp;                    //Initialize UDP library
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t prev = 0;

void setup(void)
{
  Serial.begin(115200);

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(clockLight, NUM_LEDS);
  FastLED.setBrightness(84);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);

  setSyncProvider(getNtpTime);
  setSyncInterval(60);

  prev = now();
}

void fadeall(uint8_t fadeAmount = 230)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    clockLight[i].nscale8(fadeAmount);
  }
}

void loop(void)
{
  Timezone tz = myTZ;
  TimeChangeRule *tcr;
  time_t t = tz.toLocal(now(), &tcr);
  for (int i = (hourFormat12(t) * 5) - 3; i < (hourFormat12(t) * 5) + 2; i++)
  {
    clockLight[59 - i] = CHSV(225, 255, 255);
  }
  clockLight[59 - minute(t)] = CHSV(0, 225, 255);
  clockLight[59 - second(t)] = CHSV(200, 180, 255);
  FastLED.show();
  fadeall(254);
  delay(10);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets

  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);

  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      utc = secsSince1900 - 2208988800UL;
      return utc; //Return local time (with time change rules)
    }
  }
  Serial.println("No NTP Response :(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
