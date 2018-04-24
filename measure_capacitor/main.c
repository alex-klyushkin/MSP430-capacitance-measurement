#include <msp430.h> 


/**
 * main.c
 */

/*
 * ��������� ������� ������������ � ������� MSP430.
 * ������� ������ - 16 MHz.
 * ��������� ���������� � ������� ������.
 * ����� ������� ���� ����� ������������ �� 0.5 vcc (������������� �� ������������ �����������).
 * ����� ����������������� ���������� � � ������� TIMERA_0 ���� ��������� ������� ����� ���������� ��
 * ������������ �� 0.5 vcc �� 0.25 vcc.
 * 10 ��� � ������� (���������� �� TIMERA_1) ��������� ��������� ������������ � UART (������ ������ ������ ����).
 * �� ����� ��������� ����� ������ ���������, ��� ������� - �������.
 * UART ����������, �������� RXD � TXD ����� ���������� � ��������������� ���������.
*/


//                    MSP430F2553
//                 -----------------
//                 |           P1.0|--> red LED
//                 |           P1.1|--> UART RX
//                 |           P1.2|--> UART TX
//    green LED <--|P1.6       P1.3|<-- button S2
//                 |               |
//                 |               |     ______    C1
//                 |           P1.4|-->-|__R1__|-+-||----|GND
//                 |               |             |
//                 |       CA4/P1.5|<------------+
//
// C1 - ���������� �����������
// R1 - ������������� ��� ����������� ����, ����� ����� ����� 100 ���


/*
 * �1.0 - ����� �� ������� ���������
 * P1.1 - UART RX
 * P1.2 - UART TX
 * �1.3 - ���� �� ������
 * �1.4 - ����� ��� ������ ������������
 * �1.5 - ���� ����������� ��4 (-/+) ��� ��������� ���������� �� ������������
 * �1.6 - ����� �� ������� ���������
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
    TEST_IDLE = 0,      /* �����������, ����� ������� ��������� */
    TEST_PREPARE,       /* ��������� � ���������, ���� ����� ������������, ����� ������� ��������� */
    TEST_MESURE         /* ���� ���������, ����������� �����������, ����� ������� ��������� */
};


/* ��������� ���������� */
static short testState = TEST_IDLE;                 /* ������� ����� ��������� */
static unsigned short testResultHigh = 0;           /* ��� �������� ���������� ������������ �������� */
static unsigned short testResultLow = 0;            /* ��� �������� �������� � ������ ��������� ��������� */

/* ������ ��� ������� �� uart:
 * 2 ����� - magic 0xbede
 * 1 ����  - ������� ��������� �����
 * 1 ����  - ������� � ������� �������� ���������� ������, MHz
 * 2 ����� - testResultLow
 * 2 ����� - testResultHigh */
static unsigned char  uartTxData[8] = {0xbe, 0xde, 0x0, 0x10, 0x0, 0x0, 0x0, 0x0};
static unsigned short uartTxCounter = 0;

/* ��������� ������� */
static void initClock(void);
static void setupRegisters(void);




/***********************************************************************************************************************/
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	                //stop watchdog timer
	
	initClock();                                //16 MHz
	setupRegisters();                       //��������� ���������� � ������, ���� ��� �����������, UART

	for (;;)
	{
	    __bis_SR_register(LPM1_bits + GIE);     //������ � ����������������� ����� �1 � ������������ ������������
	}                                           //� ���� ���������� �� ������

	return 0;
}


/* ��������� ������� */
/********************************************************************************************************************/
/* MCLK=16MHz */
static void initClock(void)
{
    /* 16 MHz, ACLK = VLO */
    BCSCTL1 = CALBC1_16MHZ;
    DCOCTL = CALDCO_16MHZ;
    BCSCTL3 |= LFXT1S_2;
}


/* ����������� ����� � ������ */
static void setupRegisters(void)
{
    /* P1.3 �� ����, ��������� ���������� �� ������
     * �1.0 � �1.6 �� ����� �� ����������, ������� ������� - ������ � ���������.
     * ��� ���� �1.4 ������ ��� ����� ��� ������ ������������ */
    P1DIR = 0x51;
    P1OUT = 0x48;
    P1REN = 0x8;
    P1IES = 0x8;
    P1IFG = 0;
    P1IE  = 0x8;

    /* P1.1 - UART RX, P1.2 - UART TX,
     * ���������� UART ��������� SMCLK,
     * �������� UART 9600,
     * ��������� UCBRSx = 6,
     * ��������� ���������� �� UART RX.
     * */
    /* ����������� uart */
    P1SEL  = RXD + TXD;
    P1SEL2 = RXD + TXD;
    UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 1666 % 256;         //16 MHz, 9600 boudrate
    UCA0BR1 = 1666 / 256;
    UCA0MCTL = UCBRS_6;
    UCA0CTL1 &= ~UCSWRST;  // �������������� ������ USCI
    IE2 |= UCA0RXIE;

    /* ���������� ��5 (�1.4) � ���������������� ����� �����������,
     * ���������� ����������� ���������� ����� ���������� � �������������� ����� */
    CACTL1 = CAEX;
    CACTL2 = P2CA3 + P2CA1;

    /* �������� TIMERA_1 ��� ������������� ������� ������ �� uart */
    TA1CTL |= TACLR;                        /* ������� ������ */
    TA1CCR0 = 1200;                         /* ACLK = VLOCLK = 12 KHz, ������� �� 1200 - ���������� 10 ��� � ������� */
    TA1CCTL0 |= CCIE;                       /* ��������� ���������� �� ��������� � TA1CCR0 */
    TA1CTL |= TASSEL_1 + MC_1;              /* ACLK, Up mode */
}


/************************************����������� ����������************************************************************************/

/* ���������� �� ������ - �������� ��������� */
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    if ((P1IFG & 0x8) && TEST_IDLE == testState)
    {
        testResultHigh = 0;
        testResultLow = 0;

        /* ���������� ���������� ����������� ���������� 0.5 Vcc �� ������������� ����,
         * ��������� ���������� �� ������, �������� ���������� */
        CACTL1 = CAEX + CAIE + CAREF1 + CAON;
        CACTL2 |= CAF;              /* ��������� ����� ����������� */
        P1OUT = 0x19;               /* ������ 1 �� �1.4, �������� �����������, ������ ������� ��������� */

        P1IFG = 0;

        testState = TEST_PREPARE;
    }

    /* �� ����� 1 ������� ����������, ����� ������, �� ����� */
    P1IFG = 0;
}


/* ���������� �� ����������� - ���� � ������ ��������� ��������������� ����������,
 * ���� ��������� ��������� - ��������� TIMERA_0 � ���������� */
#pragma vector=COMPARATORA_VECTOR
__interrupt void COMPARATORA_ISR(void)
{
    if (TEST_MESURE == testState)
    {
        /* �������� � �������� ��������� - ��������� ��������� */
        testResultLow = TAR;                /* ��������� ������� ������� ������� */
        TACTL &= ~(MC_3 + TAIE + TAIFG);    /* ��������� ������ */
        CACTL1 &= ~(CAON + CAIE);           /* ��������� ���������� */
        P1OUT = 0x48;                       /* �������� �������, ������ ������� ��������� */

        testState = TEST_IDLE;
    }
    else if (TEST_PREPARE == testState)
    {
        /* ���������� � ��������� � �������� ���������� - ������ �������� */
        CACTL1 = CAIE + CAREF0 + CAEX;       /* ����������� ���������� �� ������������� ����� �� 0.25 Vcc, ���������� �� ����� */
        CACTL1 |= CAON;                      /* �������� ���������� */

        /* ����������� ������ */
        TACTL &= ~TAIFG;                    /* �� ������ ������ ������� ���� ���������� */
        TACTL |= TACLR;                     /* ���������� ������ */
        TACTL |= MC_2 + TASSEL_2 + TAIE;    /* continious mode, SMCLK, ��������� ���������� �� ������������ */

        P1OUT &= ~0x10;                     /* ������ 0 �� �1.4, ��������� ����������� */

        testState = TEST_MESURE;
    }
}


/* ���������� �� ������������ ������� TIMERA_0 TAIFG */
#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta0_isr(void)
{
    ++testResultHigh;
    TA0CTL &= ~TAIFG;
}


/* ���������� UART TX */
#pragma vector=USCIAB0TX_VECTOR
__interrupt void uart_tx(void)
{
    if (++uartTxCounter >= sizeof(uartTxData))
    {
        uartTxCounter = 0;      /* ��� ���������, �������� ������� */
        IE2 &= ~UCA0TXIE;       /* ��������� ���������� �� ���������� ���������� �� ������� */
    }
    else
    {
        UCA0TXBUF = uartTxData[uartTxCounter];
    }

    IFG2 &= ~UCA0TXIFG;
}


/* ���������� UART RX */
#pragma vector=USCIAB0RX_VECTOR
__interrupt void uart_rx(void)
{
    IFG2 &= ~UCA0RXIFG;
}



/* ���������� �� ������������ ������� 1 TACCR0 CCIFG */
#pragma vector=TIMER1_A0_VECTOR
__interrupt void ta1_isr(void)
{
    if (TEST_IDLE == testState)
    {
        *(unsigned short*)&uartTxData[4] = testResultLow;
        *(unsigned short*)&uartTxData[6] = testResultHigh;
    }

    uartTxData[2] = testState;

    IE2 |= UCA0TXIE;                            /* ��������� ���������� uart tx */
    UCA0TXBUF = uartTxData[uartTxCounter];      /* ���������� ������ ���� */

    TA1CCTL0 &= ~CCIFG;
}
