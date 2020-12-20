#ifndef MHCOMMANDREADER_H
#define MHCOMMANDREADER_H

#include <ArduinoJson.h>

class MHCommandReader{
	public:
		MHCommandReader(byte* payload);		
		int checkDeserialization();
		String getCommandName();
		String getOperationId();
		String getDataElement(String element);
	private:
	     
};

#endif