#include <AWS_IOT.h>
#include <SimpleDHT.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include "credentials.h"

#include <HardwareSerial.h>
#include <PMserial.h>  // Arduino library for PM sensors with serial interface
#include <TinyGPS++.h> // Need TinyGPSPlus library

extern const char* ssid;
extern const char* ca_pem;
extern char HOST_ADDRESS[];
extern char CLIENT_ID[];
extern char TOPIC_NAME[];

AWS_IOT esp32;

int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

// GPS variables
static const int RXPin = 13, TXPin = 12;
static const uint32_t GPSBaud = 9600;

// The TinyGPS++ object
TinyGPSPlus gps;
// The serial connection to the GPS device
HardwareSerial ss(1);

SerialPM pms(PMSA003); // aka G10

// Serial1 and Serial2 are not instantiated by default, so do it here
HardwareSerial Serialfu(2); // UART2 on GPIO16(RX),GPIO17(TX)

int status = WL_IDLE_STATUS;
int tick=0, msgCount=0, msgReceived = 0;
char payload[512];
char rcvdPayload[512];

byte temperature = 0;
byte humidity = 0;

// Default is UCF
double lat = 28.602427;
double lng = -81.200058;

short day = 19; 
short month = 11;
int year = 2018;

short hour = 0, minute = 0, second = 0;
    
int err = SimpleDHTErrSuccess;

void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
    strncpy(rcvdPayload,payLoad,payloadLen);
    rcvdPayload[payloadLen] = 0;
    msgReceived = 1;
}

void setupWifi()
{
  byte error = 0;
  
  Serial.println("Connecting to: ");
  Serial.println(ssid);

  //disconnect from wifi to set new wifi connection
  WiFi.disconnect(true);  
  
  WiFi.mode(WIFI_STA);
  
  error += esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  
  error += esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
    
  error += esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)ca_pem, strlen(ca_pem));
  
  error += esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); 
  
  if (error != 0) {
    Serial.println("Error setting WPA properties.");
  }
  
  WiFi.enableSTA(true);
  
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  
  if (esp_wifi_sta_wpa2_ent_enable(&config) != ESP_OK) {
    Serial.println("WPA2 Settings Not OK");
  }

  WiFi.begin(ssid);
  
  WiFi.setHostname("RandomHostname"); //set Hostname for your device - not neccesary
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP()); //print LAN IP
}

/*****************************************************************************************/

void setup() {
  Serial.begin(9600);
  delay(2000);

  setupWifi();

  if(esp32.connect(HOST_ADDRESS,CLIENT_ID)== 0)
  {
      Serial.println("Connected to AWS");
      delay(1000);

      if(0 == esp32.subscribe(TOPIC_NAME,mySubCallBackHandler))
      {
          Serial.println("Subscribe Successful");
      }
      else
      {
          Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
          while(1);
      }
  }
  else
  {
      Serial.println("AWS connection failed, Check the HOST Address");
      while(1);
  }

   Serial.println(F("Booted"));
  

   ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin); // UART 1 maps to unused pins
   Serial.println(F("GPS STARTED"));
   pms.begin(Serialfu);
   pms.init();
   Serial.println(F("PMS sensor on HardwareSerial"));
 
  delay(2000);
}

/****************************************************************************************/

void loop() {

  if(tick >= 20)   // publish to topic every 20 seconds
  {
    if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) 
    {
      Serial.print("Read DHT11 failed, err=");
      Serial.println(err);
      delay(2000);
      return;
    }

    pms.read();
  
    // This sketch displays information every time a new sentence from GPS is correctly encoded.
    while (ss.available() > 0) {
      
      gps.encode(ss.read());
      
      if (gps.location.isUpdated()) {
        lat = gps.location.lat();
        lng = gps.location.lng();
        Serial.print("Latitude : "); Serial.print(lat , 6);
        Serial.print("Longitude : "); Serial.println(lng, 6);
        
        Serial.print(F("Date/Time: "));
        
        if (gps.date.isValid())  {
          day = gps.date.day();
          month = gps.date.month();
          year = gps.date.year();
          Serial.print(month); Serial.print(F("/"));
          Serial.print(day); Serial.print(F("/")); Serial.print(year);
        }
        else {
          Serial.print(F("INVALID"));
        }
        
        Serial.print(F(" "));
        
        if (gps.time.isValid()) {
          hour = gps.time.hour();
          if (hour < 10) 
              Serial.print(F("0"));
          Serial.print(hour); 
          Serial.print(F(":"));
          
          minute = gps.time.minute();
          if (minute < 10) 
              Serial.print(F("0"));
          Serial.print(minute);
          Serial.print(F(":"));
          
          second = gps.time.second();
          if (second < 10) 
              Serial.print(F("0"));
          Serial.print(second);
          break;
        }
        else {
          Serial.print(F("INVALID"));
        }
        Serial.println();
      }
    }
    
    tick = 0;
    
    if(IS_SHADOW_UPDATE)
      sprintf(payload,"{\"state\": {\"reported\" : {\"date\": \"%d/%d/%d\",\"time\": \"%d:%d:%d\",\"lat\": \"%f\",\"long\": \"%f\",\"temp\": \"%d\",\"humidity\": \"%d\",\"PMSA003\": \"%d\"}}}",
              month, day, year, hour, minute, second, lat, lng, temperature, humidity,pms.pm[1]);
    else
      sprintf(payload,"{\"date\": \"%d/%d/%d\",\"time\": \"%d:%d:%d\",\"lat\": \"%f\",\"long\": \"%f\",\"temp\": \"%d\",\"humidity\": \"%d\",\"PMSA003\": \"%d\"}",
              month, day, year, hour, minute, second, lat, lng, temperature, humidity,pms.pm[1]);
    
    if(esp32.publish(TOPIC_NAME,payload) == 0)
    { 
      Serial.println("");
      Serial.print("Publish Message:");
      Serial.println(payload);
    }
    else
    {
      Serial.println("Publish failed");
    }
  }  
  
  vTaskDelay(1000 / portTICK_RATE_MS); 
  tick++;
}
