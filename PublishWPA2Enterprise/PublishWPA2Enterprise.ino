#include <AWS_IOT.h>
#include <SimpleDHT.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include "credentials.h"

AWS_IOT esp32;

int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

extern const char* ssid;
extern const char* ca_pem;
extern char HOST_ADDRESS[];
extern char CLIENT_ID[];
extern char TOPIC_NAME[];

int status = WL_IDLE_STATUS;
int tick=0,msgCount=0,msgReceived = 0;
char payload[512];
char rcvdPayload[512];

byte temperature = 0;
byte humidity = 0;
    
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

void setup() {
  Serial.begin(115200);
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

  delay(2000);
}

void loop() {

  if(tick >= 10)   // publish to topic every 10 seconds
  {
    if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) 
    {
      Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(2000);
      return;
    }
    
    tick = 0;
    
    if(IS_SHADOW_UPDATE)
      sprintf(payload,"{\"state\": {\"reported\" : {\"temperature\" : \"%d\"}}}",(int)temperature);
    else
      sprintf(payload,"{\"temperature\": \"%d\"}",(int)temperature);
    
    if(esp32.publish(TOPIC_NAME,payload) == 0)
    {        
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
