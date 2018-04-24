// capasitor_read.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <string>
#include "comPort.hpp"


enum
{
	TEST_IDLE = 0,		/* простаиваем, горит зеленый светодиод */
	TEST_PREPARE,       /* готовимся к измерению, идет заряд конденсатора, горит красный светодиод */
	TEST_MESURE,        /* идет измерение, конденсатор разряжается, горит красный светодиод */
	TEST_STATE_COUNT
};

/* magic с которого начинаются данные */
static const unsigned char magic0 = 0xbe;
static const unsigned char magic1 = 0xde;


static long double milli = 1e-3;
static long double micro = 1e-6;
static long double nano =  1e-9;
static long double piko =  1e-12;


/* размер пакета с данными */
static const unsigned int packSize = 8;

/* паузы различной длительности */
static const DWORD	waitConnectSleep = 100;					/*ms*/
static const DWORD	waitDataSleep = 40;					/*ms*/
static const DWORD  shortWaitSleep = 2;					/*ms*/


static const char waitSign[] = { '-', '\\', '|', '/' };

static const unsigned int freqMultiplier = 1000000;

/* константы для распарсивания данных в пакете */
static const unsigned int stateShift = 2;
static const unsigned int freqShift = 3;
static const unsigned int ticksShift = 4;

/* количество "пустых" чтений, после которого детектируется разрыв связи */
static const unsigned int voidReadMax = 100;		/* при паузе в ~40 ms между чтениями, отключение MSP430 будет обнаружено примерно через 4 секунды */

/* буферы для чтения данных */
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

	/* попросим ввести имя порта, из которого нужно читать
	и сопротивление резистора, с которым производилось измерение (см. в проекте CCS для MSP430 Launchpad)*/
	cout << "Please, enter com port name: ";
	cin >> comPortName;
	cout << "Please, enter resistance (Ohms): ";
	cin >> resist;

	ComPort comPort(comPortName, 8, 9600, NOPARITY, ONESTOPBIT);

	/* основной цикл - вычитываем пакет данных и парсим его */
	while (true)
	{
		/* если нет подключения */
		if (noConnection)
		{
			/* ждем подключения */
			waitConnection(comPort);

			cout << endl << "Connection success" << endl;
			noConnection = false;

			/* вычитаем все старые данные, которые есть в буфере последовательного порта */
			while ((nBytes = comPort.read(dummyData, sizeof(dummyData))) == sizeof(dummyData));

			currentState = TEST_STATE_COUNT;
			currentTicks = 0;
			voidRead = 0;
		}

		nRead = readData(comPort, dataPack, sizeof(dataPack));

		if (nRead == sizeof(dataPack))
		{
			/* разбираем полученные данные */
			voidRead = 0;
			state = dataPack[stateShift];
			freq = dataPack[freqShift];
			ticks = *(unsigned int*)&dataPack[ticksShift];

			/* если данные изменились */
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
		else /* ничего не прочитали */
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


/* вычисление емкости и вывод в консоль */
static void printData(char state, unsigned int ticks, unsigned int freq, unsigned int res)
{
	cout << "\r                                                                            \r";

	/* смотрим в каком режиме сейчас MSP430 и выводим информацию */
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


/* вычитать пакет с данными */
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

	/* пытаемся прочитать первый байт magic */
	int reallyRead = port.read((char*)&mag0, sizeof(mag0));

	if (sizeof(mag0) == reallyRead)
	{
		if (mag0 == magic0)
		{
			/* пытаемся прочитать второй байт magic */
			reallyRead = port.read((char*)&mag1, sizeof(mag1));
			if (sizeof(mag1) == reallyRead)
			{
				if (mag1 == magic1)
				{
					needData = size - 2;  /* размер данных = размер пакета - размер magic*/
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

	/* magic вычитан, нужно вычитать остальное */
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
		nBytes += 2;    /* + 2 байта magic*/
	}

	return nBytes;
}

/* функция ожидает подключение устройства к последовательному порту */
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


/* функция вычисляет емкость */
static long double calcCapacity(unsigned int freq, unsigned int ticks, unsigned int resistance)
{
	long double timeInSec = (1.0L / (freq * freqMultiplier)) * ticks;

	return (timeInSec / (resistance * log(2)));
}

