#ifndef SerialDataLink_h
#define SerialDataLink_h

#ifdef _WIN32
	#include <windows.h>
#else
	#define DWORD int
	#define HANDLE int
	#include <unistd.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <termios.h>
#endif
#include <stdio.h>
#include <math.h>


#include "DataLink.h"


#ifdef _WIN32
class __declspec( dllexport ) SerialDataLink : public DataLink {
#else
class SerialDataLink : public DataLink {
#endif

public:
	SerialDataLink(const char * comPort, int baudRate);
	bool connect();
	bool send(char * message, int bytes);
	int receive(int bytes, char * message);
	bool disconnect();
	void setByteSize(int bits);
	void setNumberOfStopBits(int stopBits);
private:
	DWORD checkBaudRate(int baudRate);
	const char * comPort;
	DWORD baudRate;
	DWORD stopBits;
	DWORD parity;
	HANDLE hSerial;
	int byteSize;
};
#endif
