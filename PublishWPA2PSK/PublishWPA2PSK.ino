#include <AWS_IOT.h>
#include <SimpleDHT.h>
#include <WiFi.h>
#include "credentials.h"

AWS_IOT esp32;

int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

extern char WIFI_SSID[];
extern char WIFI_PASSWORD[];

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

void setup() {
    Serial.begin(115200);
    delay(2000);

    while (status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(WIFI_SSID);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        // wait 5 seconds for connection:
        delay(5000);
    }

    Serial.println("Connected to wifi");

    if(esp32.connect(HOST_ADDRESS,CLIENT_ID)== 0)
    {
        Serial.println("Connected to AWS");
        delay(1000);

        if(0==esp32.subscribe(TOPIC_NAME,mySubCallBackHandler))
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
    tick=0;
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
