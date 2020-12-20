#include "MHDevice.h"
#include "MHCommandReader.h"

#define RELAY_CONTROL_PIN D7
#define STATE_PARAM_INDEX 0 //must be the parameter index in the model definition

void gotControlMessage(char* topic, byte* payload, unsigned int length);

DeviceModel deviceModel = {
  "Relay000001",      //name
  "MHRelayDevice",    //typ
  "1.0.0",            //firmware
  0,                  //wakeUpInterval
  { //controlInterface
    "MQTT_TOPIC",                 //type
    "mh.Relay000001.CONTROL"      //connectionString
  },
  { //statusInterface
    "MQTT_TOPIC",                 //type
    "mh.Relay000001.STATUS"       //connectionString
  },
  { //parameters
    {           
       "STATE",   //name
       "ON_OFF"  //type     
    }
  },
  { //commands
    {
      "TOGGLE"  //name
    },
    {
      "UPDATE", //name
      { //data
        {
          "link",   //elementName
          "string"  //type
        }        
      }
    },
    {
      "IA_ALIVE",    
    }
  }
  
};
MHDevice mhDevice(deviceModel, gotControlMessage);

/**
 * callback for all control messages
 */
void gotControlMessage(char* topic, byte* payload, unsigned int length) {
  #ifdef DEBUG 
  Serial.print("Message arrived at topic: ");
  Serial.println(topic);
  #endif
  MHCommandReader mhCommandReader(payload);
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
  if (command.equals("TOGGLE")){            
    //swtich relay
    int newPinValue = !digitalRead(RELAY_CONTROL_PIN);
    digitalWrite(RELAY_CONTROL_PIN, newPinValue);    
    mhDevice.setCurrentValue(STATE_PARAM_INDEX, (newPinValue == HIGH)?"OFF\0":"ON\0");
    //publish DONE        
    mhDevice.publishCurrentValues(id);
  } else if (command.equals("IS_ALIVE")){
    mhDevice.publishCurrentValues(id);
  } else if(command.equals("DISCOVER")){
    mhDevice.publishStatus(id);
  } else if (command.equals("UPDATE")){
        
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
   
  }
  else {
    String error = "wrong command: " + command;
    mhDevice.publishError(id, error);
    #ifdef DEBUG 
    Serial.println(error);
    #endif
  }
  
}


void setup()
{
  #ifdef DEBUG 
  Serial.begin(9600);   
  delay(200);  
  #endif
  pinMode(RELAY_CONTROL_PIN, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(RELAY_CONTROL_PIN, HIGH); // relay is opened
  mhDevice.setCurrentValue(STATE_PARAM_INDEX, "OFF\0");  
  digitalWrite(BUILTIN_LED, HIGH); // led is off    
  mhDevice.connect();
  mhDevice.publishCurrentValues("");
  delay(200);
   
}

void loop() {  
  mhDevice.loop();    
  delay(100);
}
