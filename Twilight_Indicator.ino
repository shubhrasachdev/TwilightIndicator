#include <WiFi.h>
#include <Wire.h>
#include "RTClib.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PI 3.1415926
#define zenith 96

//Defining Bluetooth object
BluetoothSerial bt;

//WiFi Configuration
int n;
int id=-1;
String ssid="";
String pwd="";        

//RTC object
RTC_PCF8523 rtc;  

// Defining NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Defining HTTPClient object
HTTPClient http;

//Variables to save latitude, longitude and timezone
//Defaults to the coordinates of Delhi, India
double lat=28.7041;
double lng=77.1025;
String timezone="+05:30";

//Variables to save sunset and sunrise
DateTime sunrise, sunset;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
int relayPin=23;

void connect_WiFi();
void geolocate();
float getOffset(String);
void sync();
void disp_time(DateTime,int,int);
void disp_date(DateTime);
DateTime tw(DateTime, int);

void setup () {
  pinMode(relayPin, OUTPUT);
  connect_WiFi();
  geolocate();
  sync();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  DateTime now = rtc.now();
  sunrise=tw(now, 0);
  sunset=tw(now, 1);
}

void loop () {
    int flag=0, now_minutes, sunrise_minutes, sunset_minutes;
    DateTime now = rtc.now();
    if((now.hour()==0)&&(now.minute()==0)&&(now.second()==0))
    {
      sync();
      sunrise=tw(now, 0);
      sunset=tw(now, 1);
    }
    now_minutes=(now.hour())*60 + now.minute();
    sunrise_minutes=(sunrise.hour())*60 + sunrise.minute();
    sunset_minutes=(sunset.hour())*60 + sunset.minute();
    if((flag==0)&&((now_minutes>=sunset_minutes)||(now_minutes<=sunrise_minutes)))  
    {
      digitalWrite(relayPin, HIGH);
      flag=1;
    }
    if((flag==1)&&(now_minutes>=sunrise_minutes)&&(now_minutes<=sunset_minutes))  
    {
      digitalWrite(relayPin, LOW);
      flag=0;
    }
    display.setCursor(0,0);
    display.print("Date ");
    disp_date(now);
    display.setCursor(0,1);
    display.print("Time ");
    disp_time(now,5,1);
    delay(3000);
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Sunrise ");
    disp_time(sunrise,8,0);
    display.setCursor(0,1);
    display.print("Sunset ");
    disp_time(sunset,8,1);
    delay(3000);
    display.clearDisplay();
}

void connect_WiFi()
{
  bt.begin("Twilight Indicator");
  n = WiFi.scanNetworks();
  while((bt.read()-48)!=1);
  for(int i=0; i<=n;i++) 
  { 
    bt.print(WiFi.SSID(i));
    delay(1000);
  }
  do
  {
    id=bt.read();
  }while(id==-1);
  id=id-1;
  ssid=WiFi.SSID(id);
  if(WiFi.encryptionType(id)==WIFI_AUTH_OPEN)  
  {
    bt.print("1");
    WiFi.begin(ssid.c_str());
  }
  else if(WiFi.encryptionType(id)!=WIFI_AUTH_OPEN)
  {
       bt.print("0");
      do
      {
        pwd=bt.readString();
      }while(pwd=="");
      Serial.println(pwd.c_str());
      WiFi.begin(ssid.c_str(),pwd.c_str());
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }  
  bt.print("Connected");
}

void geolocate()
{
  // ipify.org url with api key to make geolocation request
  //String key = "YOUR_API_KEY";
  http.begin("https://geo.ipify.org/api/v1?apiKey="/*+key*/);
  int httpCode = http.GET();  //Make the request
  if (httpCode > 0)  //Check for the returning code 
  { 
        String response = http.getString();
        //Serial.println(response);
        const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(8) + 190;
        DynamicJsonBuffer jb(capacity);
        JsonObject& root= jb.parseObject(response);
        JsonObject& location = root["location"];
        lat = location["lat"];
        lng = location["lng"];
        String tz = location["timezone"];
        timezone=tz;
  }
  else return;
  http.end(); //Free the resources
}

float getOffset(String tz)
{
    float offset=0, mini;
    offset=(tz[1]-48)*10+(tz[2]-48);
    mini=(tz[4]-48)*10+(tz[5]-48);
    mini=mini/60;
    offset+=mini;
    if(tz[0]=='-') offset=-offset;
    return offset;
}

void sync()
{
  if(WiFi.status() != WL_CONNECTED) return;
  if (! rtc.begin()) {
    //Serial.println("Couldn't find RTC");
    return;
  }
  timeClient.setTimeOffset(getOffset(timezone)*3600);
  while(!timeClient.update()) timeClient.forceUpdate();
  rtc.adjust(DateTime(timeClient.getEpochTime()));
}

void disp_time(DateTime obj, int col, int row)
{
  display.setCursor(col,row);
  if(obj.hour()<10) display.print(0);
  display.print(obj.hour());
  display.setCursor(col+2,row);
  display.print(':');
  display.setCursor(col+3,row);
  if(obj.minute()<10) display.print(0);
  display.print(obj.minute());
}

void disp_date(DateTime obj)
{
  display.setCursor(5,0);
  if(obj.day()<10) display.print(0);
  display.print(obj.day());
  display.setCursor(7,0);
  display.print('/');
  display.setCursor(8,0);
  if(obj.month()<10) display.print(0);
  display.print(obj.month());
  display.setCursor(10,0);
  display.print('/');
  display.setCursor(11,0);
  display.print(obj.year());
}

DateTime tw(DateTime obj, int flag)
{
    float tz=getOffset(timezone);
    double N1 = floor(275 * (obj.month()) / 9);
    double N2 = floor(((obj.month()) + 9) / 12);
    double N3 = (1 + floor(((obj.year()) - 4 * floor((obj.year()) / 4) + 2) / 3));
    double N = N1 - (N2 * N3) + obj.day() - 30;
    double lngHour = lng / 15;
    double t;
    if(flag==1) t= N + ((18 - lngHour) / 24);
    if(flag==0) t= N + ((6 - lngHour) / 24);
    double M = (0.9856 * t) - 3.289;
    double L = M + (1.916 * sin(M*(PI/180))) + (0.020 * sin(2 * M*(PI/180))) + 282.634;
    if (L>360) L-=360;
    if (L<0)   L+=360;
    double RA = (180/PI)*atan(0.91764 * tan((PI/180)*L));
    if (RA>360) RA-=360;
    if (RA<0)   RA+=360;
    double Lquadrant  = (floor( L/90)) * 90;
    double RAquadrant = (floor(RA/90)) * 90;
    RA = RA + (Lquadrant - RAquadrant);
    RA = RA / 15;
    double sinDec = 0.39782 * sin((PI/180)*L);
    double cosDec = cos(asin(sinDec));
    double cosH = (cos((PI/180)*zenith) - (sinDec * sin((PI/180)*lat))) / (cosDec * cos((PI/180)*lat));
    double H;
    if(flag==0) H = 360 - ((180/PI)*acos(cosH));
    if(flag==1) H = (180/PI)*acos(cosH);
    H = H / 15;
    double T = H + RA - (0.06571 * t) - 6.622;
    if(flag==1) T+=24;
    T = T - lngHour + tz;
    int h=T;
    int m=(T-h)*60;
    return DateTime(obj.year(), obj.month(), obj.day(), h, m, 0); 
}
