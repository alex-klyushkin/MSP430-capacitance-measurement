// capasitor_read.cpp: ���������� ����� ����� ��� ����������� ����������.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <string>
#include "comPort.hpp"


enum
{
	TEST_IDLE = 0,		/* �����������, ����� ������� ��������� */
	TEST_PREPARE,       /* ��������� � ���������, ���� ����� ������������, ����� ������� ��������� */
	TEST_MESURE,        /* ���� ���������, ����������� �����������, ����� ������� ��������� */
	TEST_STATE_COUNT
};

/* magic � �������� ���������� ������ */
static const unsigned char magic0 = 0xbe;
static const unsigned char magic1 = 0xde;


static long double milli = 1e-3;
static long double micro = 1e-6;
static long double nano =  1e-9;
static long double piko =  1e-12;


/* ������ ������ � ������� */
static const unsigned int packSize = 8;

/* ����� ��������� ������������ */
static const DWORD	waitConnectSleep = 100;					/*ms*/
static const DWORD	waitDataSleep = 40;					/*ms*/
static const DWORD  shortWaitSleep = 2;					/*ms*/


static const char waitSign[] = { '-', '\\', '|', '/' };

static const unsigned int freqMultiplier = 1000000;

/* ��������� ��� ������������� ������ � ������ */
static const unsigned int stateShift = 2;
static const unsigned int freqShift = 3;
static const unsigned int ticksShift = 4;

/* ���������� "������" ������, ����� �������� ������������� ������ ����� */
static const unsigned int voidReadMax = 100;		/* ��� ����� � ~40 ms ����� ��������, ���������� MSP430 ����� ���������� �������� ����� 4 ������� */

/* ������ ��� ������ ������ */
static char dataPack[packSize];
static char dummyData[1024];


static long double calcCapacity(unsigned int freq, unsigned int ticks, unsigned int resistance);
static void waitConnection(ComPort &port);
static int readData(ComPort &port, char* buf, size_t size);
static void printData(char state, unsigned int ticks, unsigned int freq, unsigned int res);

using namespace std;

int main()
{	
	string comPortName;
	int nBytes = 0;
	unsigned int freq = 0;
	char state = TEST_IDLE;
	unsigned int ticks = 0;
	unsigned int resist = 0;
	int nRead = 0;
	char currentState = TEST_STATE_COUNT;
	unsigned int currentTicks = 0;
	unsigned int voidRead = 0;
	bool noConnection = true;

	/* �������� ������ ��� �����, �� �������� ����� ������
	� ������������� ���������, � ������� ������������� ��������� (��. � ������� CCS ��� MSP430 Launchpad)*/
	cout << "Please, enter com port name: ";
	cin >> comPortName;
	cout << "Please, enter resistance (Ohms): ";
	cin >> resist;

	ComPort comPort(comPortName, 8, 9600, NOPARITY, ONESTOPBIT);

	/* �������� ���� - ���������� ����� ������ � ������ ��� */
	while (true)
	{
		/* ���� ��� ����������� */
		if (noConnection)
		{
			/* ���� ����������� */
			waitConnection(comPort);

			cout << endl << "Connection success" << endl;
			noConnection = false;

			/* �������� ��� ������ ������, ������� ���� � ������ ����������������� ����� */
			while ((nBytes = comPort.read(dummyData, sizeof(dummyData))) == sizeof(dummyData));

			currentState = TEST_STATE_COUNT;
			currentTicks = 0;
			voidRead = 0;
		}

		nRead = readData(comPort, dataPack, sizeof(dataPack));

		if (nRead == sizeof(dataPack))
		{
			/* ��������� ���������� ������ */
			voidRead = 0;
			state = dataPack[stateShift];
			freq = dataPack[freqShift];
			ticks = *(unsigned int*)&dataPack[ticksShift];

			/* ���� ������ ���������� */
			if ((currentState != state) || (currentTicks != ticks))
			{
				printData(state, ticks, freq, resist);
				currentState = state;
				currentTicks = ticks;
			}
		}
		else if (nRead < 0)
		{
			std::cout << "Some error occured while reading form " << comPort.getName() << std::endl;
			return -1;
		}
		else /* ������ �� ��������� */
		{
			if (voidRead < voidReadMax)
			{
				voidRead++;
			}
			else
			{
				std::cout << "Connection with " << comPort.getName() << " is lost" << std::endl;
				noConnection = true;
				comPort.disconnect();
			}
		}

		Sleep(waitDataSleep);
	}

    return 0;
}


/* ���������� ������� � ����� � ������� */
static void printData(char state, unsigned int ticks, unsigned int freq, unsigned int res)
{
	cout << "\r                                                                            \r";

	/* ������� � ����� ������ ������ MSP430 � ������� ���������� */
	switch (state)
	{
		case TEST_IDLE:
			if (0 == ticks)
			{
				cout << "Frequency " << freq << " MHz, measurements have not yet been made";
			}
			else
			{
				std::string units;
				long double capacity = calcCapacity(freq, ticks, res);

				if (capacity > milli)
				{
					units = "mF";
					capacity /= milli;
				}
				else if (capacity > micro)
				{
					units = "uF";
					capacity /= micro;
				}
				else if (capacity > nano)
				{
					units = "nF";
					capacity /= nano;
				}
				else if (capacity > piko)
				{
					units = "pF";
					capacity /= piko;
				}
				else
				{
					capacity = 0;
					cout << "Capacity too small!";
				}

				if (capacity)
				{
					cout << "Frequency " << freq << " MHz, capacity = " << capacity << " " << units;
				}

			}
			break;

		case TEST_MESURE:
		case TEST_PREPARE:
			cout << "Frequency " << freq << " MHz. Measurement in process";
			break;

		default:
			cout << "Frequency " << freq << ". Unknown state";
	}
}


/* �������� ����� � ������� */
static int readData(ComPort &port, char* buf, size_t size)
{
	unsigned char mag0 = 0, mag1 = 0;
	size_t needData = 0;
	int nBytes = 0;

	if (size < 2)
	{
		std::cout << "Too small buffer" << std::endl;
		return 0;
	}

	if (buf == nullptr)
	{
		std::cout << "Buffer is NULL" << std::endl;
		return 0;
	}

	/* �������� ��������� ������ ���� magic */
	int reallyRead = port.read((char*)&mag0, sizeof(mag0));

	if (sizeof(mag0) == reallyRead)
	{
		if (mag0 == magic0)
		{
			/* �������� ��������� ������ ���� magic */
			reallyRead = port.read((char*)&mag1, sizeof(mag1));
			if (sizeof(mag1) == reallyRead)
			{
				if (mag1 == magic1)
				{
					needData = size - 2;  /* ������ ������ = ������ ������ - ������ magic*/
				}
			}
			else if (reallyRead < 0)
			{
				cout << "can't read magic1 byte from " << port.getName() << endl;
			}
		}
	}
	else if (reallyRead < 0)
	{
		cout << "can't read magic0 byte from " << port.getName() << ". RealyRead " << reallyRead << endl;
	}

	/* magic �������, ����� �������� ��������� */
	while (needData)
	{
		if ((reallyRead = port.read(&dataPack[packSize - needData], needData)) < 0)
		{
			cout << "ERROR: can't read from " << port.getName() << endl;
			port.disconnect();
			return -1;
		}

		needData -= reallyRead;
		nBytes += reallyRead;
	}

	if (nBytes != 0)
	{
		nBytes += 2;    /* + 2 ����� magic*/
	}

	return nBytes;
}

/* ������� ������� ����������� ���������� � ����������������� ����� */
static void waitConnection(ComPort &port)
{
	int res = -1;
	int i = 0;

	cout << "\r                                                           \r";

	while (ERROR_SUCCESS != res)
	{
		res = port.connect();

		if (ERROR_FILE_NOT_FOUND == res)
		{
			cout << "                   \r";
			cout << "wait connection to " << port.getName() << "..." << waitSign[i++];
			i %= sizeof(waitSign);
		}
		else if (ERROR_SUCCESS != res)
		{
			cout << "some other error occurred: error = " << (int)res << endl;
			system("pause");
			exit(1);
		}

		Sleep(waitConnectSleep);
	}

}


/* ������� ��������� ������� */
static long double calcCapacity(unsigned int freq, unsigned int ticks, unsigned int resistance)
{
	long double timeInSec = (1.0L / (freq * freqMultiplier)) * ticks;

	return (timeInSec / (resistance * log(2)));
}

