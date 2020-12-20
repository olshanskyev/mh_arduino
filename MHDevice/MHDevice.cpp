#include "MHDevice.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

MHDevice::MHDevice(DeviceModel deviceModel){		
	this->deviceModel = deviceModel;
	this->callback = NULL;
}

MHDevice::MHDevice(DeviceModel deviceModel, MQTT_CALLBACK_SIGNATURE){	
	this->deviceModel = deviceModel;
	this->callback = callback;
}

/*
 * Flashes an LED in an error state.
 * */
void MHDevice::ledFlash (int led_pin, int num) {
  pinMode(led_pin, OUTPUT);
  for (int i = 0; i < num; i++) {
    digitalWrite(led_pin, LOW);
    delay(200);
    digitalWrite(led_pin, HIGH);
    delay(200);
  }
}

bool MHDevice::isConnected(){
	return (WiFi.status() == WL_CONNECTED && mqttClient.connected());
}

void MHDevice::setupWIFI() {
    // starting by connecting to a WiFi network    
    int waitingTime = 0;
	#ifdef DEBUG 
    Serial.print("Attempting WiFi connection to ");
	Serial.print(SSID);
	#endif
	WiFi.enableAP(0); //hide ssid
    WiFi.begin(SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED && waitingTime < WIFI_MAX_WAITING_TIME) {
      delay(100);
      waitingTime += 100;
	  #ifdef DEBUG 
      Serial.print(".");
	  #endif
    }                    
}

void MHDevice::setupMQTT() {
  int connectionAttempts = 0;
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);  
  // Loop until we're connected
  while (!mqttClient.connected()) {   
	#ifdef DEBUG   
    Serial.println("Attempting MQTT connection...");
	#endif
    // Create a random client ID
    String clientId = "MHDevice-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
	  #ifdef DEBUG 
      Serial.println("MQTT connected");  
	  #endif
	  if(callback != NULL){ //register callback for mh.DeviceName.CONTROL		  
		  if (strcmp(deviceModel.controlInterface.type, "MQTT_TOPIC") == 0){
			mqttClient.setCallback(callback);			
			mqttClient.subscribe(deviceModel.controlInterface.connectionString);
		  }
	  }
    } else {
	  #ifdef DEBUG 
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
	  #endif
      // Wait 5 seconds before retrying
      connectionAttempts++;
      if (connectionAttempts < MQTT_CONNECTION_ATTEMPTS){
        delay(5000);
      } else {
        break;     
      }
    }
  } 
}


void MHDevice::mqttPublishToStatusTopic(char* data){
	if (strcmp(deviceModel.statusInterface.type, "MQTT_TOPIC") == 0){		
		mqttClient.publish(deviceModel.statusInterface.connectionString, data);  
	} else {
		#ifdef DEBUG 
		Serial.println("Wrong statusInterface type");
		#endif
	}
}


boolean MHDevice::connect(){
  if (WiFi.status() != WL_CONNECTED){
	setupWIFI();  
  }
  if (WiFi.status() == WL_CONNECTED){   
	#ifdef DEBUG   
	Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
	#endif
	if ((strcmp(deviceModel.controlInterface.type, "MQTT_TOPIC") == 0) || (strcmp(deviceModel.statusInterface.type, "MQTT_TOPIC") == 0)){
		setupMQTT();
		if (!mqttClient.connected()){          
		  ledFlash(BUILTIN_LED, 2);
		  #ifdef DEBUG 
		  Serial.println("Failed MQTT connection!");
		  #endif
		  return false;
		}   
	}
  } else {
    ledFlash(BUILTIN_LED, 3);
	#ifdef DEBUG 
    Serial.println("Failed WiFi connection!");
	#endif
	return false;
  }
  delay(100);
  return true;
}

void MHDevice::loop(){
	mqttClient.loop();
	if (!isConnected()){
		connect();
	}	
}

void MHDevice::publishStatus(String id){
	char* statusMessage = buildStatusMessage(id);
	mqttPublishToStatusTopic(statusMessage);	
}

void MHDevice::publishCurrentValues(String id){
	char* statusMessage = buildCurrentValuesMessage(id);
	mqttPublishToStatusTopic(statusMessage);	
}	

void MHDevice::publishError(String id, String errorString){
	char* statusMessage = buildErrorMessage(id, errorString);
	mqttPublishToStatusTopic(statusMessage);
}

/**
 * installs previously downloaded update
 * returns -1 - SPIFFS problems
 *         -2 - Update problems
 *         -3 - storage space problem
 */
int MHDevice::installUpdate(){
	digitalWrite(BUILTIN_LED, LOW);
	if (SPIFFS.begin()) {
	  #ifdef DEBUG 
	  Serial.println(F("SPIFFS done."));
	  #endif
	} else {
	  #ifdef DEBUG 
	  Serial.println(F("SPIFFS fail."));
	  #endif
	  return -1;
	}

	File f = SPIFFS.open("/Update.bin\0", "r");
	if (!f) {
	 #ifdef DEBUG 
	 Serial.println("Cannot open file Update.bin");
	 #endif
	 SPIFFS.end();
	 return -1;
	}  
	int total = f.size();
	uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
	if (total > maxSketchSpace){
	#ifdef DEBUG 
	Serial.println("Not enough storage");
	#endif
	f.close();
	SPIFFS.end();
	return -3;
	}
	if (!Update.begin(maxSketchSpace, U_FLASH)) { //start with max available size
	#ifdef DEBUG 
	Update.printError(Serial);
	Serial.println("ERROR while updating");
	#endif
	f.close();
	SPIFFS.end();
	return -2;
	}  

	int allBytes = 0;  
	while (f.available()) {
	uint8_t ibuffer[2048];
	allBytes += f.read((uint8_t *)ibuffer, 2048);
	#ifdef DEBUG 
	Serial.print("Reaad bytes: ");
	Serial.print(allBytes);
	Serial.print(" of ");
	Serial.println(total);
	#endif
	Update.write(ibuffer, sizeof(ibuffer));    
	}
	#ifdef DEBUG 
	Serial.println("Update finished!");
	#endif
	Update.end(true);
	digitalWrite(BUILTIN_LED, HIGH);
	f.close();
	//remove update file
	if (SPIFFS.remove("/Update.bin\0")){
		#ifdef DEBUG 
		Serial.println("Update file removed");  
		#endif
	}
	SPIFFS.end();
	return 0;
}

/**
 * downloads file from link to /Update.bin
 * returns 0 for OK 
 *         -1 - SPIFFS problems
 *         httpCode for http errors
 *         
 */
int MHDevice::downloadUpdate(String link){
	HTTPClient http;  
	if (SPIFFS.begin()) {
	  #ifdef DEBUG 
	  Serial.println(F("SPIFFS done."));
	  #endif
	} else {
	  #ifdef DEBUG 
	  Serial.println(F("SPIFFS fail."));
	  #endif
	  return -1;
	}           
	// remove if exists and open file
	SPIFFS.remove("/Update.bin\0");    
	File f = SPIFFS.open("/Update.bin\0", "w+");
	if (!f) {
	 #ifdef DEBUG 
	 Serial.println("Cannot open file Update.bin");
	 #endif
	 SPIFFS.end();
	 return -1;
	}

	http.begin(link);
	int httpCode = http.GET();        
		  
	if (httpCode == HTTP_CODE_OK) {
		#ifdef DEBUG 
		Serial.print("got OK from ");
		Serial.println(link);        
		#endif
		int total = http.getSize();    
		http.writeToStream(&f);   
		#ifdef DEBUG 
		Serial.print("Download complete. File size: ");
		Serial.println(total);        
		#endif
	} else {
		#ifdef DEBUG 
		Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());     
		#endif
	}    
	http.end();    
	f.close();   
	SPIFFS.end(); 
	return (httpCode == HTTP_CODE_OK)?0:httpCode;    
}

void MHDevice::setCurrentValue(char index, String value){
	if (index < MAX_PARAMETERS){
		currentValues[index] = value;		
	}
}

String MHDevice::getCurrentValue(char index){
	if (index < MAX_PARAMETERS){
		return currentValues[index];
	} else {
		return "";
	}
}

char* MHDevice::buildStatusMessage(String id){
	DynamicJsonDocument doc(2048);
	char message[2048] = "0";
	doc["device"]["name"] = deviceModel.name;
	doc["device"]["type"] = deviceModel.type;
	doc["device"]["firmware"] = deviceModel.firmware;
	doc["device"]["wakeUpInterval"] = deviceModel.wakeUpInterval;	
	doc["device"]["ip"] = WiFi.localIP().toString();
	doc["device"]["controlInterface"]["type"] = deviceModel.controlInterface.type;
	doc["device"]["controlInterface"]["connectionString"] = deviceModel.controlInterface.connectionString;
	doc["device"]["statusInterface"]["type"] = deviceModel.statusInterface.type;
	doc["device"]["statusInterface"]["connectionString"] = deviceModel.statusInterface.connectionString;
	
	JsonArray parameters = doc["device"].createNestedArray("parameters");		
	for(int i = 0; i < MAX_PARAMETERS; i++){
		if (strlen(deviceModel.parameters[i].name) > 0) {			
			JsonObject parameterItem = parameters.createNestedObject();
			parameterItem["name"] = deviceModel.parameters[i].name;	
			parameterItem["type"] = deviceModel.parameters[i].type;			
		} else {
			break;
		}		
	}
	
	JsonArray commands = doc["device"].createNestedArray("commands");	
	for(int i = 0; i < MAX_COMMANDS; i++){
		if (strlen(deviceModel.commands[i].command) > 0) {			
			JsonObject commandItem = commands.createNestedObject();
			commandItem["command"] = deviceModel.commands[i].command;			
			JsonArray dataItems = commandItem.createNestedArray("data");
			for (int j = 0; j < MAX_DATA_ITEMS; j++){
				if (strlen(deviceModel.commands[i].dataItems[j].elementName) > 0){
					JsonObject dataItem = dataItems.createNestedObject();
					dataItem["elementName"] = deviceModel.commands[i].dataItems[j].elementName;	
					dataItem["type"] = deviceModel.commands[i].dataItems[j].type;
				} else {
					break;
				}
			}
		} else {
			break;
		}		
	}
	if (!id.equals("")){
		doc["requestId"] = id;
	}
	
	for(int i = 0; i < MAX_PARAMETERS; i++){
		if (strlen(deviceModel.parameters[i].name) > 0){			
			doc["currentValues"][deviceModel.parameters[i].name] = currentValues[i];
		} else {
			break;
		}
	}

	serializeJson(doc, message);	
	return message;	
	
}

char* MHDevice::buildErrorMessage(String id, String error){
	StaticJsonDocument<512> doc;
	char message[512] = "0";
	if (!id.equals("")){
		doc["requestId"] = id;
	}
	doc["error"] = error;	
	
	serializeJson(doc, message);	
	return message;		
}

char* MHDevice::buildCurrentValuesMessage(String id){
	StaticJsonDocument<512> doc;
	char message[512] = "0";
	doc["device"]["name"] = deviceModel.name;
	doc["device"]["controlInterface"]["type"] = deviceModel.controlInterface.type;
	doc["device"]["controlInterface"]["connectionString"] = deviceModel.controlInterface.connectionString;	
	
	if (!id.equals("")){
		doc["requestId"] = id;
	}
	
	for(int i = 0; i < MAX_PARAMETERS; i++){
		if (strlen(deviceModel.parameters[i].name) > 0){			
			doc["currentValues"][deviceModel.parameters[i].name] = currentValues[i];
		} else {
			break;
		}
	}
	
	serializeJson(doc, message);	
	return message;	
	
}

String MHDevice::getControlMessageFromQueue(){
	String message = "";	
	if (strcmp(deviceModel.controlInterface.type, "MQTT_QUEUE") == 0){			
		HTTPClient http;  		        			
		http.setAuthorization(MQTT_WEBSERVER_USER, MQTT_WEBSERVER_PASSWORD);				
		
		String link = "http://" + String(MQTT_SERVER) + ":" + String(MQTT_WEBSERVER_PORT) + "/api/message/" + String(deviceModel.controlInterface.connectionString) + "?type=queue&oneShot=true&readTimeout=200"; //&clientId=" + String(deviceModel.name);
		#ifdef DEBUG 
		Serial.println(link);
		#endif
		http.begin(link);
				
		int httpCode = http.GET();				
		if (httpCode == HTTP_CODE_OK) {
			#ifdef DEBUG 
			Serial.print("got OK from ");
			Serial.println(link);
			#endif
			
			message = http.getString();
				
		} else {	
			#ifdef DEBUG 
			Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());            	 
			#endif
		}			
		http.end();			
		
	}
	return message;
}
	
	
	
	