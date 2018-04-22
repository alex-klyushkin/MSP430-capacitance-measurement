#include <msp430.h> 


/**
 * main.c
 */

/*
 * Измерение емкости конденсатора с помощью MSP430.
 * Частота работы - 16 MHz.
 * Измерение начиниется с нажатия кнопки.
 * После нажатия идет заряд конденсатора до 0.5 vcc (детектируется по срабатыванию компаратора).
 * Затем перенастраивается компаратор и с помощью TIMERA_0 идет измерение времени спада напряжения на
 * конденсаторе от 0.5 vcc до 0.25 vcc.
 * 10 раз в секунду (прерывания по TIMERA_1) результат измерения выпихивается в UART (формат данных описан ниже).
 * Во время измерения горит красны светодиод, при простое - зеленый.
 * UART аппаратный, джамперы RXD и TXD нужно установить в соответствующее положение.
*/


//                    MSP430F2553
//                 -----------------
//                 |           P1.0|--> red LED
//                 |           P1.1|--> UART RX
//                 |           P1.2|--> UART TX
//    green LED <--|P1.6       P1.3|<-- button S2
//                 |               |
//                 |               |      C1
//                 |           P1.4|-->-+-||----|GND
//                 |               |    |
//                 |       CA4/P1.5|<---+

/*
 * Р1.0 - выход на красный светодиод
 * P1.1 - UART RX
 * P1.2 - UART TX
 * Р1.3 - вход от кнопки
 * Р1.4 - выход для заряда конденсатора
 * Р1.5 - вход компаратора СА4 (-/+) для измерения напряжения на конденсаторе
 * Р1.6 - выход на зеленый светодиод
 * */
#define RED_LED     BIT0
#define RXD         BIT1
#define TXD         BIT2
#define BUTTON_IN   BIT3
#define CHARGE_OUT  BIT4
#define COMPAR_IN   BIT5
#define GREEN_LED   BIT6



enum
{
    TEST_IDLE = 0,      /* простаиваем, горит зеленый светодиод */
    TEST_PREPARE,       /* готовимся к измерению, идет заряд конденсатора, горит красный светодиод */
    TEST_MESURE         /* идет измерение, конденсатор разряжается, горит красный светодиод */
};


/* локальные переменные */
static short testState = TEST_IDLE;                 /* текущий режим измерения */
static unsigned short testResultHigh = 0;           /* для хранения количества переполнений счетчика */
static unsigned short testResultLow = 0;            /* для значения счетчика в момент окончания измерения */

/* массив для отсылки по uart:
 * 2 байта - magic 0xbede
 * 1 байт  - текущее состояние теста
 * 1 байт  - частота с которой работает измеряющий таймер, MHz
 * 2 байта - testResultLow
 * 2 байта - testResultHigh */
static unsigned char  uartTxData[8] = {0xbe, 0xde, 0x0, 0x10, 0x0, 0x0, 0x0, 0x0};
static unsigned short uartTxCounter = 0;

/* локальные функции */
static void initClock(void);
static void setupRegisters(void);




/***********************************************************************************************************************/
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	                //stop watchdog timer
	
	initClock();                                //16 MHz
	setupRegisters();                       //настроили светодиоды и кнопку, вход для компаратора, UART

	for (;;)
	{
	    __bis_SR_register(LPM1_bits + GIE);     //уходим в энергосберегающий режим №1 с разрешенными прерываниями
	}                                           //и ждем прерывания от кнопки

	return 0;
}


/* локальные функции */
/********************************************************************************************************************/
/* MCLK=16MHz */
static void initClock(void)
{
    /* 16 MHz, ACLK = VLO */
    BCSCTL1 = CALBC1_16MHZ;
    DCOCTL = CALDCO_16MHZ;
    BCSCTL3 |= LFXT1S_2;
}


/* настраиваем порты и кнопку */
static void setupRegisters(void)
{
    /* P1.3 на вход, разрешаем прерывание от кнопки
     * Р1.0 и Р1.6 на выход на светодиоды, включем зеленый - готовы к измерению.
     * Еще порт Р1.4 займем под выход для заряда конденсатора */
    P1DIR = 0x51;
    P1OUT = 0x48;
    P1REN = 0x8;
    P1IES = 0x8;
    P1IFG = 0;
    P1IE  = 0x8;

    /* P1.1 - UART RX, P1.2 - UART TX,
     * аппаратный UART тактируем SMCLK,
     * скорость UART 9600,
     * модуляция UCBRSx = 6,
     * разрешаем прерывания от UART RX.
     * */
    /* настраиваем uart */
    P1SEL  = RXD + TXD;
    P1SEL2 = RXD + TXD;
    UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 1666 % 256;         //16 MHz, 9600 boudrate
    UCA0BR1 = 1666 / 256;
    UCA0MCTL = UCBRS_6;
    UCA0CTL1 &= ~UCSWRST;  // Инициализируем модуль USCI
    IE2 |= UCA0RXIE;

    /* Подключаем СА5 (Р1.4) к неинвентирующему входу компаратора,
     * внутреннее референсное напряжение будет подключено к инвертирующему входу */
    CACTL1 = CAEX;
    CACTL2 = P2CA3 + P2CA1;

    /* настроим TIMERA_1 для периодической отсылки данных по uart */
    TA1CTL |= TACLR;                        /* сбросим таймер */
    TA1CCR0 = 1200;                         /* ACLK = VLOCLK = 12 KHz, считаем до 1200 - прерывание 10 раз в секунду */
    TA1CCTL0 |= CCIE;                       /* разрешаем прерывание по сравнению с TA1CCR0 */
    TA1CTL |= TASSEL_1 + MC_1;              /* ACLK, Up mode */
}


/************************************обработчики прерываний************************************************************************/

/* прерывание от кнопки - начинаем измерение */
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    if ((P1IFG & 0x8) && TEST_IDLE == testState)
    {
        testResultHigh = 0;
        testResultLow = 0;

        /* выставляем внутреннее референсное напряжение 0.5 Vcc на инвертирующий вход,
         * разрешаем прерывание по фронту, включаем компаратор */
        CACTL1 = CAEX + CAIE + CAREF1 + CAON;
        CACTL2 |= CAF;              /* фильтруем выход компаратора */
        P1OUT = 0x19;               /* подаем 1 на Р1.4, заряжаем конденсатор, зажгли красный светодиод */

        P1IFG = 0;

        testState = TEST_PREPARE;
    }

    /* на порту 1 никакие прерывания, кроме кнопки, не нужны */
    P1IFG = 0;
}


/* прерывание от компаратора - если в режиме измерение перенастраиваем компаратор,
 * если закончили измерение - выключаем TIMERA_0 и компаратор */
#pragma vector=COMPARATORA_VECTOR
__interrupt void COMPARATORA_ISR(void)
{
    if (TEST_MESURE == testState)
    {
        /* измеряли и получили прерывние - закончили измерение */
        testResultLow = TAR;                /* запомнили текущий счетчик таймера */
        TACTL &= ~(MC_3 + TAIE + TAIFG);    /* остановим таймер */
        CACTL1 &= ~(CAON + CAIE);           /* выключили компаратор */
        P1OUT = 0x48;                       /* погасили красный, зажгли зеленый светодиод */

        testState = TEST_IDLE;
    }
    else if (TEST_PREPARE == testState)
    {
        /* готовились к измерению и получили прерывание - начнем измерять */
        CACTL1 = CAIE + CAREF0 + CAEX;       /* референсное напряжение на инвертирующем входе на 0.25 Vcc, прерывание по спаду */
        CACTL1 |= CAON;                      /* включаем компаратор */

        /* настраиваем таймер */
        TACTL &= ~TAIFG;                    /* на всякий случай сбросим флаг прерывания */
        TACTL |= TACLR;                     /* сбрасываем таймер */
        TACTL |= MC_2 + TASSEL_2 + TAIE;    /* continious mode, SMCLK, разрешаем прерывания по переполнению */

        P1OUT &= ~0x10;                     /* подаем 0 на Р1.4, разряжаем конденсатор */

        testState = TEST_MESURE;
    }
}


/* прерывание по переполнению таймера TIMERA_0 TAIFG */
#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta0_isr(void)
{
    ++testResultHigh;
    TA0CTL &= ~TAIFG;
}


/* прерывание UART TX */
#pragma vector=USCIAB0TX_VECTOR
__interrupt void uart_tx(void)
{
    if (++uartTxCounter >= sizeof(uartTxData))
    {
        uartTxCounter = 0;      /* все отправили, обнуляем счетчик */
        IE2 &= ~UCA0TXIE;       /* запрещаем прерывания до следующего прерывания по таймеру */
    }
    else
    {
        UCA0TXBUF = uartTxData[uartTxCounter];
    }

    IFG2 &= ~UCA0TXIFG;
}


/* прерывание UART RX */
#pragma vector=USCIAB0RX_VECTOR
__interrupt void uart_rx(void)
{
    IFG2 &= ~UCA0RXIFG;
}



/* прерывание по переполнению таймера 1 TACCR0 CCIFG */
#pragma vector=TIMER1_A0_VECTOR
__interrupt void ta1_isr(void)
{
    if (TEST_IDLE == testState)
    {
        *(unsigned short*)&uartTxData[4] = testResultLow;
        *(unsigned short*)&uartTxData[6] = testResultHigh;
    }

    uartTxData[2] = testState;

    IE2 |= UCA0TXIE;                            /* разрешаем прерывание uart tx */
    UCA0TXBUF = uartTxData[uartTxCounter];      /* отправляем первый байт */

    TA1CCTL0 &= ~CCIFG;
}
