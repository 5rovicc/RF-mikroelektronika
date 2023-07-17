#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <TinyGPS.h>
#include <elapsedMillis.h>
#include <ESP8266WiFiMulti.h>

#define SIM800_TX_PIN 12
#define SIM800_RX_PIN 13

#define GPS_TX_PIN 5
#define GPS_RX_PIN 4

SoftwareSerial serialGPS(GPS_TX_PIN,GPS_RX_PIN); //GPS PINS
SoftwareSerial serialSIM800(SIM800_TX_PIN,SIM800_RX_PIN); //GSM PINS
TinyGPS gps;
ESP8266WiFiMulti wifiMulti;


int gps_switch = 14;    // select the input pin for the potentiometer     // select the pin for the LED
int gsm_switch = 15;  // variable to store the value coming from the sensor
//int converter_switch = A3;

int short_sleepTimeS = 600; //600
int long_sleepTimeS = 14400; //14400
int sleepTime = 0;
int alarm_flag = 0;

int a,timer;
char incomingByte;
char lat[5][10];
char lon[5][10];
const char* host = "yourhostname.ddns.net";
ADC_MODE(ADC_VCC);

static void smartdelay(unsigned long ms);
static void print_float(float val, float invalid, int len, int prec);
static void print_int(unsigned long val, unsigned long invalid, int len);
static void print_date(TinyGPS &gps);
static void print_str(const char *str, int len);
bool waitForOK();
bool waitFor(String searchString, int waitTimeMS);
void getChars();
static void send_sms(String lat,String lon);
static void send_msg(String text);
static void gsmOn();

void setup(){
  Serial.begin(115200);
  while(!Serial);
  //setup pinmode for hardware controll via transistors
  pinMode(gsm_switch, OUTPUT);//gsm
  pinMode(gps_switch, OUTPUT);//gps
  
  wifiMulti.addAP("wifi1", "passwd");
  wifiMulti.addAP("wifi2", "passwd");
  wifiMulti.addAP("wifi3", "passwd");

  Serial.println("Connecting Wifi...");
  
  Serial.println();
  Serial.println("Sats HDOP Latitude  Longitude  Fix  Date       Time     Date Alt    Course Speed Card  Distance Course Card  Chars Sentences Checksum");
  Serial.println("          (deg)     (deg)      Age                      Age  (m)    --- from GPS ----  ---- to London  ----  RX    RX        Fail");
  Serial.println("-------------------------------------------------------------------------------------------------------------------------------------");
}

void loop(){
  Serial.println("WIFI LOOP STARTS");
  for(int i = 0;i<2;i++){
    if(wifiMulti.run() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        alarm_flag = 0;
        break;
        
    }
    else if(wifiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected! Checking next..");
        alarm_flag = 1;
        delay(1000);
    }

    delay(2000);
  }
  Serial.println("FLAG:");
  Serial.print(alarm_flag);
  
  //if wifi status not ok 
  if(alarm_flag){
    digitalWrite(gsm_switch, HIGH);//turn on GSM shield
    digitalWrite(gps_switch, HIGH);//turn on GPS
    delay(2000);
    serialGPS.begin(38400); //begin conmmunciation with GPS
    serialSIM800.begin(9600);
    float flat, flon, last_flat, last_flon;
    unsigned long age, date, time, chars = 0;
    unsigned short sentences = 0, failed = 0;
    static const double LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
    a=0;
    timer=0;
    do{
      Serial.println("\nEntering do-while loop.");
      serialGPS.listen();
      if(serialGPS.isListening()){
        print_int(gps.satellites(), TinyGPS::GPS_INVALID_SATELLITES, 5);//number of satelites
        print_int(gps.hdop(), TinyGPS::GPS_INVALID_HDOP, 5);//hdop
        gps.f_get_position(&flat, &flon, &age);//get position
        print_float(flat, TinyGPS::GPS_INVALID_F_ANGLE, 10, 6);//lat
        print_float(flon, TinyGPS::GPS_INVALID_F_ANGLE, 11, 6);//lon
        print_int(age, TinyGPS::GPS_INVALID_AGE, 5);//age
        print_date(gps);//date
        print_float(gps.f_altitude(), TinyGPS::GPS_INVALID_F_ALTITUDE, 7, 2);//alt
        print_float(gps.f_course(), TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);//
        print_float(gps.f_speed_kmph(), TinyGPS::GPS_INVALID_F_SPEED, 6, 2);//speed
        print_str(gps.f_course() == TinyGPS::GPS_INVALID_F_ANGLE ? "*** " : TinyGPS::cardinal(gps.f_course()), 6);
        print_int(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0xFFFFFFFF : (unsigned long)TinyGPS::distance_between(flat, flon, LONDON_LAT, LONDON_LON) / 1000, 0xFFFFFFFF, 9);
        print_float(flat == TinyGPS::GPS_INVALID_F_ANGLE ? TinyGPS::GPS_INVALID_F_ANGLE : TinyGPS::course_to(flat, flon, LONDON_LAT, LONDON_LON), TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);
        print_str(flat == TinyGPS::GPS_INVALID_F_ANGLE ? "*** " : TinyGPS::cardinal(TinyGPS::course_to(flat, flon, LONDON_LAT, LONDON_LON)), 6);
        gps.stats(&chars, &sentences, &failed);
        print_int(chars, 0xFFFFFFFF, 6);
        print_int(sentences, 0xFFFFFFFF, 10);
        print_int(failed, 0xFFFFFFFF, 9);
        //get 10 valid reads from gps and save them in an array. 
        if(!(int(flat)== 1000)){
          //we need lat, lon, date, time, alt, speed 
          //then send one message with 10 links or 10 messages 
          // if task took longer than one minute (or 20/30 secs) - skip it and send sms with surrounding wifi networks, try connect to open wifi(?) and push geolocation
          last_flat = flat;
          last_flon = flon;
    
          dtostrf(flat, 5, 6, lat[a]);
          dtostrf(flon, 5, 6, lon[a]);
          Serial.println("\nCorrect location");
          Serial.println(lat[a]);
          Serial.println(lon[a]);
          gsmOn();
          send_sms(lat[a],lon[a]);
          Serial.println("\nMessage sent!");
          Serial.println(a);
          a=a+1;
          Serial.println();
          smartdelay(1000);
        }
        else{
          Serial.println("\n no correct location");
          if(timer == 100){
            gsmOn();
            Serial.println("\n Here sms is send.");
            //send_msg("no correct location");
            Serial.println("\n 10 seconds attempt finished with failure.");
            sleepTime = short_sleepTimeS;
            break;
          }
          else{
            timer=timer+1;
            delay(1000);
          }
        }
      }
    }while(a<100);
    //sleep it for shorter time to make sure dog came back to the home range 
    //power off all the hardware and converter
    //serial command to gps and gsm to power off?
    Serial.println("\n Devices should power off and arduino go to selected sleep mode.");
    delay(1500);
    digitalWrite(gps_switch, LOW);//turn off GPS
    digitalWrite(gsm_switch, LOW);//turn off GPS
    delay(1000);
    Serial.println("All devices powered down.");
    Serial.println("short sleep mode");
    // Sleep
    ESP.deepSleep(sleepTime * 1000000);
  }
  else{
    //status ok - push update to db / push notification on phone
    Serial.println("Device should push status");
    
    Serial.print("Connecting to "); Serial.print(host);
    WiFiClient client;
    int retries = 5;
    while (!client.connect(host, 4567) && (retries-- > 0)) {
      Serial.print(".");
    }
    Serial.println();
    if (!client.connected()) {
      Serial.println("Failed to connect, going back to sleep");
    }

    float vdd = ESP.getVcc() / 1000.0;
    String vdd_value = String(vdd);
    String url = "/pettracker";

    Serial.println();

    String valueStr =   "{\"battery\":\"" + vdd_value + "\",\"location\":\"" + WiFi.SSID() +"\",\"id\":\"0\"}";

    String urlLoad = String("POST ") + url +
                     " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "Content-Type: application/json\r\n" +
                     "Content-Length: "  + valueStr.length() + "\r\n\r\n" +
                     valueStr + "\r\n\r\n";

    Serial.print(urlLoad);
    client.print(urlLoad);
    int timeout = 5 * 10; // 5 seconds
    while (!client.available() && (timeout-- > 0)) {
      delay(100);
    }
    WiFi.disconnect();
    digitalWrite(gps_switch, LOW);//turn off GPS
    Serial.println("long sleep mode");
    delay(2000);
    sleepTime = long_sleepTimeS;
    ESP.deepSleep(sleepTime * 1000000);
  }
}

static void smartdelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (serialGPS.available())
      gps.encode(serialGPS.read());
  } while (millis() - start < ms);
}

static void print_float(float val, float invalid, int len, int prec)
{
  if (val == invalid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartdelay(0);
}

static void print_int(unsigned long val, unsigned long invalid, int len)
{
  char sz[32];
  if (val == invalid)
    strcpy(sz, "*******");
  else
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartdelay(0);
}

static void print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print("********** ******** ");
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d ",
        month, day, year, hour, minute, second);
    Serial.print(sz);
  }
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);
  smartdelay(0);
}

static void print_str(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartdelay(0);
}

elapsedMillis waitTime;
bool waitFor(String searchString, int waitTimeMS) {
    waitTime = 0;
    String foundText;
    while (waitTime < waitTimeMS) {
        if (!serialSIM800.available()) {
            // Nothing in the buffer, wait a bit
            delay(5);
            continue;
        }
        
        // Get the next character in the buffer
        incomingByte = serialSIM800.read();
        if (incomingByte == 0) {
            // Ignore NULL character
            continue;
        }
        foundText += incomingByte;
        
        if (foundText.lastIndexOf(searchString) != -1) {
            // Found the search string
            return true;
        }
    }
    
    // Timed out before finding it
    return false;
}

/**
 * Wait for "OK" from the Sim800l
 */
bool waitForOK() {
    return waitFor("\nOK\r\n", 4000);
}

static void send_sms(String lat,String lon){
  serialSIM800.listen();
  if(serialSIM800.isListening()){
    serialSIM800.println("AT");
    //wait for ok from sim800l
    if(waitForOK()){
      Serial.println("Setup Complete!");
      Serial.println("Sending SMS...");
      serialSIM800.write("AT+CMGF=1");
      delay(1000);
      serialSIM800.write("AT+CMGS=\"0123123123\"\r\n");
      delay(1000);
      char charBuf[30];
      String meserialGPSage = "http://maps.apple.com/?q="+lat+","+lon;
      Serial.println(meserialGPSage);
      meserialGPSage.toCharArray(charBuf, 30);
      serialSIM800.write(charBuf);
      delay(1000);
      serialSIM800.write((char)26);
      delay(1000);
      Serial.println("SMS Sent!");
    }
    else{
      Serial.println("NO OK STATUS");
    }
    
  }
}

static void send_msg(String text){
  Serial.println("Setup Complete!");
  Serial.println("Sending SMS...");
   
  //Set SMS format to ASCII
  serialSIM800.write("AT+CMGF=1\r\n");
  waitForOK();
  delay(1000);
 
  //Send new SMS command and message number
  serialSIM800.write("AT+CMGS=\"0123123123\"\r\n");
  waitForOK();
  delay(1000);
   
  //Send SMS content
  char charBuf[30];
  String meserialGPSage = text;
  Serial.println(meserialGPSage);
  meserialGPSage.toCharArray(charBuf, 30);
  serialSIM800.write(charBuf);
  waitForOK();
  delay(1000);
   
  //Send Ctrl+Z / ESC to denote SMS message is complete
  serialSIM800.write((char)26);
  waitForOK();
  delay(1000);  
  Serial.println("SMS Sent!");
  serialSIM800.end();
}
void gsmOn() {
  serialSIM800.listen();
  serialSIM800.println("AT");
  if (waitForOK()) {
    return;
  }
  // For at least one second
  delay(1100);

  // Send data to the the Sim800 so it auto bauds
  delay(4000);
  // Wait for the Sim800l to become ready
  waitFor("SMS Ready", 15000);
  getChars();
}
void getChars() {
  while (serialSIM800.available() > 0) {
          incomingByte = serialSIM800.read();
  //Serial.print(incomingByte);
  delay(5);
      }  
}


