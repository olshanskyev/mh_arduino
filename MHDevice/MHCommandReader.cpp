#include "MHCommandReader.h"

DynamicJsonDocument doc(1024); 
DeserializationError error;

MHCommandReader::MHCommandReader(byte* payload){
	error = deserializeJson(doc, payload);		
}
	
int MHCommandReader::checkDeserialization(){
	return (error)? error.code(): 0;
}

String MHCommandReader::getCommandName(){
	return doc["command"];
}

String MHCommandReader::getOperationId(){
	return doc["id"];
}

 String MHCommandReader::getDataElement(String element){
	return doc["data"][element];
}
	
	
	
	