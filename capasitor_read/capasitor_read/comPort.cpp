#include "stdafx.h"
#include "comPort.hpp"


int ComPort::connect()
{
	/* откроем com порт */
	hComPort = ::CreateFile((LPCSTR)name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == hComPort)
	{
		return GetLastError();
	}

	DCB tmpParam = { 0 };

	/* получим параметры и установим свои */
	tmpParam.DCBlength = sizeof(tmpParam);
	if (!GetCommState(hComPort, &tmpParam))
	{
		DWORD err = GetLastError();
		disconnect();
		return err;
	}

	tmpParam.BaudRate = comParam.BaudRate;
	tmpParam.ByteSize = comParam.ByteSize;
	tmpParam.Parity = comParam.Parity;
	tmpParam.StopBits = comParam.StopBits;

	memcpy(&comParam, &tmpParam, sizeof(comParam));

	if (!SetCommState(hComPort, &comParam))
	{
		DWORD err = GetLastError();
		disconnect();
		return err;
	}

	/* выставим таймауты */
	if (!SetCommTimeouts(hComPort, &comTimeouts)) 
	{
		DWORD err = GetLastError();
		disconnect();
		return err;
	}

	return 0;
}



void ComPort::disconnect()
{
	if (INVALID_HANDLE_VALUE != hComPort)
	{
		CloseHandle(hComPort);
		hComPort = INVALID_HANDLE_VALUE;
	}
}



int ComPort::setTimeouts(int interval, int readTimeout, int writeTimeout)
{
	if (INVALID_HANDLE_VALUE != hComPort)
	{
		comTimeouts = { 0 };
		comTimeouts.ReadIntervalTimeout = interval;
		comTimeouts.ReadTotalTimeoutConstant = readTimeout;
		comTimeouts.WriteTotalTimeoutConstant = writeTimeout;

		SetCommTimeouts(hComPort, &comTimeouts);

		return  GetLastError();
	}
	
	return -1;
}


int ComPort::read(char* readBuf, unsigned int bufLen)
{
	DWORD reallyRead = 0;
	
	if ((INVALID_HANDLE_VALUE != hComPort) && (0 != bufLen))
	{
		if (!ReadFile(hComPort, readBuf, bufLen, &reallyRead, 0))
		{
			reallyRead = -1;
		}
	}

	return reallyRead;
}


int ComPort::write(char* writeBuf, unsigned int bufLen)
{
	DWORD reallyWrite = 0;

	if ((INVALID_HANDLE_VALUE != hComPort) && (0 != bufLen))
	{
		if (!ReadFile(hComPort, writeBuf, bufLen, &reallyWrite, 0))
		{
			reallyWrite = -1;
		}
	}

	return reallyWrite;
}

