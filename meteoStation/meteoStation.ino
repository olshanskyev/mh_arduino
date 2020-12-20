/**
 * specification
 * used sensors:
 * - BPM280 Pressure sensor (I2C, Address 0x45):
 * SDA - D2
 * SCL - D1
 * CSB and SDO - 3.3V
 * - SHT30 Temp/Humidity sensor (I2C, Address 0x77):
 * SDA - D2
 * SCL - D1
 * - Light Sensor (Analog)
 * A0 - A0
 * VCC - D7 //to turn off while sleeping
 */

#include "MHDevice.h"
#include "MHCommandReader.h"
#include <WEMOS_SHT3X.h>
#include <Adafruit_BMP280.h>

#define LIGHT_INPUT_PIN A0
#define LIGHT_CONTROL_PIN D7

#define TEMP_PARAM_INDEX 0 //must be the parameter index in the model definition
#define HUM_PARAM_INDEX 1 
#define PRES_PARAM_INDEX 2
#define LIGHT_PARAM_INDEX 3

#define WAKE_UP_INTERVAL 30

DeviceModel deviceModel = {
  "MeteoStation000001",       //name
  "MHMeteoStation",           //typ
  "1.0.0",                    //firmware
  WAKE_UP_INTERVAL,           //wakeUpInterval
  { //controlInterface
    "MQTT_QUEUE",                             //type
    "mh.MeteoStation000001.CONTROL"     //connectionString
  },
  { //statusInterface
    "MQTT_TOPIC",                            //type
    "mh.MeteoStation000001.STATUS"      //connectionString
  },
  { //parameters
    {           
       "TEMPERATURE",   //name
       "NUMBER"  //type     
    },
    {           
       "HUMIDITY",   //name
       "NUMBER"  //type     
    },
    {           
       "PRESSURE",   //name
       "NUMBER"  //type     
    },    
    {           
       "LIGHT",   //name
       "NUMBER"    //type     
    }
  },
  { //commands    
    {
      "UPDATE", //command
      { //data
        {
          "link",   //elementName
          "string"  //type
        }        
      }
    },
    {
      "DISCOVER"
    }
  }
  
};
MHDevice mhDevice(deviceModel);

SHT3X sht30(0x45);
Adafruit_BMP280 bmp; // I2C

/**
 * callback for all control messages
 */
void gotControlMessage(String payload) {
  #ifdef DEBUG
  Serial.println("Got control message: "); 
  #endif 
  byte bytes[payload.length() + 1];
  payload.getBytes(bytes, payload.length()+1);  
    
  MHCommandReader mhCommandReader(bytes);
  int result = mhCommandReader.checkDeserialization();
    
  if (result != 0){
    String error = "Cannot deserialize json. Wrong format. Error: " ;
    mhDevice.publishError("", error + result);
    #ifdef DEBUG
    Serial.print(error);
    Serial.println(result);
    #endif
    return;
  }
  
  String command = mhCommandReader.getCommandName();
  String id = mhCommandReader.getOperationId();
  
  #ifdef DEBUG
  Serial.println("id: " + id);
  Serial.println("command: " + command);  
  #endif
  
  // possible commands: TOGGLE, UPDATE ...  
  if (command.equals("UPDATE")){
        
    String link = mhCommandReader.getDataElement("link");
    int downloadResult = MHDevice::downloadUpdate(link);
    if (downloadResult == 0){ //downloaded
      int installResult = MHDevice::installUpdate();  
      if (installResult == 0){                
        mhDevice.publishStatus(id);
        #ifdef DEBUG
        Serial.println("Reset...");
        #endif
        delay(5000);
        ESP.restart();   
        delay(1000);
      } else {        
        String error = "update installation failed with code: ";
        mhDevice.publishError(id, error + installResult);
        #ifdef DEBUG
        Serial.print(error);        
        Serial.println(installResult);
        #endif
      }
    } else {
      String error = "download failed with code: ";
      mhDevice.publishError(id, error + downloadResult);
      #ifdef DEBUG
      Serial.print(error);
      Serial.println(downloadResult);      
      #endif
    }         
   
  } else if(command.equals("DISCOVER")){
    mhDevice.publishStatus(id);
  }
  else {
    String error = "wrong command: " + command;
    mhDevice.publishError(id, error);
    #ifdef DEBUG
    Serial.println(error);
    #endif
  }
  
}

void readAndPublishData(){  
  String error = "";
  if(sht30.get()==0) {    
    mhDevice.setCurrentValue(TEMP_PARAM_INDEX, String(sht30.cTemp));   //temp in celsius
    mhDevice.setCurrentValue(HUM_PARAM_INDEX, String(sht30.humidity));    
  } else {   
    error = "Error SHT reading!";
    #ifdef DEBUG
    Serial.println(error);   
    #endif
  }

  if (bmp.begin()) {//default 0x77      
      mhDevice.setCurrentValue(PRES_PARAM_INDEX, String(bmp.readPressure() / 133.322)); //mm  
  } else {    
    error += "Error BMP reading!";
    #ifdef DEBUG
    Serial.println(error);
    #endif
  }

  float light = analogRead(LIGHT_INPUT_PIN);
  mhDevice.setCurrentValue(LIGHT_PARAM_INDEX, String(light));
  
  //publish Values
  mhDevice.publishCurrentValues("");

  if (error.length() > 0){
    mhDevice.publishError("", error);
  }
    
}

void setup()
{
  #ifdef DEBUG
  Serial.begin(9600);   
  delay(200); 
  #endif 
  
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH); // led is off    
  pinMode(LIGHT_INPUT_PIN, INPUT); 
  pinMode(LIGHT_CONTROL_PIN, OUTPUT);
  digitalWrite(LIGHT_CONTROL_PIN, HIGH);
  
  mhDevice.setCurrentValue(TEMP_PARAM_INDEX, "0.0");
  mhDevice.setCurrentValue(HUM_PARAM_INDEX, "0.0");
  mhDevice.setCurrentValue(PRES_PARAM_INDEX, "0.0");
  mhDevice.setCurrentValue(LIGHT_PARAM_INDEX, "0");
  if (mhDevice.connect()){
    String payload = mhDevice.getControlMessageFromQueue();
    if (!payload.equals("")){      
      gotControlMessage(payload);
    }
    readAndPublishData();    
  }
  delay(200); //needed to publish data    
  #ifdef DEBUG
  Serial.println("going to deep sleep...");  
  #endif  
  //deep sleep for WAKE_UP_INTERVAL seconds
  ESP.deepSleep(WAKE_UP_INTERVAL * 1000000); 
  delay(100);    
}

void loop() {
}
