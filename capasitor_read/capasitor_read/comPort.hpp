#pragma once

#include <Windows.h>
#include <string>
#include <vector>



class ComPort
{
public:
	/*
	parity - one of NOPARITY
					ODDPARITY
					EVENPARITY
					MARKPARITY
					SPACEPARITY
	stopBitNum - one of	ONESTOPBIT
						ONE5STOPBITS
						TWOSTOPBITS
	*/
	ComPort(std::string name, int byteSize, int baudRate, int parity, int stopBitNum):
		name(name), hComPort(INVALID_HANDLE_VALUE)
	{
		comParam = { 0 };
		comParam.BaudRate = baudRate;
		comParam.Parity = parity;
		comParam.StopBits = stopBitNum;
		comParam.ByteSize = byteSize;

		comTimeouts = { 0 };
		comTimeouts.ReadIntervalTimeout = readSignTimeout;
		comTimeouts.ReadTotalTimeoutConstant = readComPortTimeout;
	}

	int connect();
	void disconnect();

	int read(char* readBuf, unsigned int bufLen);
	int write(char* writeBuf, unsigned int bufLen);

	int setTimeouts(int interval, int readTimeout, int writeTimeout);

	std::string getName() { return name; }
	
private:
	std::string name;
	HANDLE hComPort;
	COMMTIMEOUTS comTimeouts;
	DCB comParam;

	const DWORD readComPortTimeout = 10;	/*ms*/
	const DWORD readSignTimeout = 2;		/*ms*/
};
