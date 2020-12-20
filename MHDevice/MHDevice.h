#ifndef MHDEVICE_H
#define MHDEVICE_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <ArduinoJson.h>

#define MQTT_PORT 1883
#define MQTT_WEBSERVER_PORT 8161
#define MQTT_WEBSERVER_USER "admin"
#define MQTT_WEBSERVER_PASSWORD "admin"

#define MQTT_SERVER "192.168.0.111"
#define MQTT_USER "username"
#define MQTT_PASSWORD "password"

#define SSID "mhAccessPoint"
#define WIFI_PASSWORD "mh_access"

#define WIFI_MAX_WAITING_TIME 20000
#define MQTT_CONNECTION_ATTEMPTS 1

#define MAX_PARAMETERS 8
#define MAX_COMMANDS 8
#define MAX_DATA_ITEMS 4

//#define DEBUG

typedef char DEVICE_NAME_t[32];
typedef char DEVICE_TYPE_t[16];
typedef char FIRMWARE_VERSION_t[8];

typedef char INTERFACE_TYPE_t[16];
typedef char CONNECTION_STRING_t[128];

typedef char PARAMETER_NAME_t[16];
typedef char TYPE_IN_STRING_t[16];

typedef char COMMAND_NAME_t[16];

typedef char DATA_ELEMENT_NAME_t[16];

struct Interface {
	INTERFACE_TYPE_t type;
	CONNECTION_STRING_t connectionString;
};

struct Parameter {	
	PARAMETER_NAME_t name;
	TYPE_IN_STRING_t type;
};

struct DataItem {		
	DATA_ELEMENT_NAME_t elementName;
	TYPE_IN_STRING_t type;
};

struct Command {
	COMMAND_NAME_t command;
	DataItem dataItems[MAX_DATA_ITEMS];
};



struct DeviceModel {
	DEVICE_NAME_t name;
	DEVICE_TYPE_t type;
	FIRMWARE_VERSION_t firmware;
	unsigned int wakeUpInterval;
	Interface controlInterface;
	Interface statusInterface;
	Parameter parameters[MAX_PARAMETERS];
	Command commands[MAX_COMMANDS];
};

class MHDevice{
	public:
		MHDevice(DeviceModel deviceModel);	
		/**
		* constructor with setting callback for control messages
		*/
		MHDevice(DeviceModel deviceModel, MQTT_CALLBACK_SIGNATURE);
		boolean connect();
		bool isConnected();		
		/**
		*	sends the whole status message (f.e. while descovering)
		*/
		void publishStatus(String id);
		/**
		*	sends only current values
		*/
		void publishCurrentValues(String id);
		
		/**
		*	sends error message
		*/
		void publishError(String id, String errorString);		
		void ledFlash (int led_pin, int num);
		void loop();
		/**
		 * downloads file from link to /Update.bin
		 * returns 0 for OK 
		 *         -1 - SPIFFS problems
		 *         httpCode for http errors
		 *         
		 */
		static int downloadUpdate(String link);		
		/**
		 * installs previously downloaded update
		 * returns -1 - SPIFFS problems
		 *         -2 - Update problems
		 *         -3 - storage space problem
		 */
		static int installUpdate();		
		
		void setCurrentValue(char index, String value);
		String getCurrentValue(char index);
		
		/**
		 * returns control message 
		 */
		String getControlMessageFromQueue();
	private:
		void mqttPublishToStatusTopic(char* data);
		DeviceModel deviceModel;
		String currentValues[MAX_PARAMETERS]={""};
		MQTT_CALLBACK_SIGNATURE;
		void setupMQTT();
		void setupWIFI();
		char* buildStatusMessage(String id);
		char* buildCurrentValuesMessage(String id);
		char* buildErrorMessage(String id, String error);
};

#endif