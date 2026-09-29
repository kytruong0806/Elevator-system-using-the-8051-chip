/* =====================================================
   U4 - TRUNG TAM DIEU KHIEN DONG CO
   AT89C52, thach anh 11.0592MHz

   - Nhan khung lenh tu U1 qua UART (P3.0 RXD)
   - Dieu khien: motor thang, motor cua, LED UP/DOWN/WAIT, coi
   - Hien thi tang hien tai tren LED 7 doan (P0)
   - Mat lien lac voi U1 > 1 giay -> dung tat ca,
     LED 7 doan hien dau '-'
   ===================================================== */

#include <reg52.h>

/* =========================
   KHAI BAO CHAN (theo so do)
   ========================= */

/* PORT 0: LED 7 doan common anode
   P0.0..P0.6 = SEG1..SEG7 = a..g */
#define SEG_PORT P0

/* PORT 2: LED + coi */
sbit LED_UP   = P2^0;   /* LEDUP   */
sbit LED_WAIT = P2^1;   /* LEDWAIT */
sbit LED_DOWN = P2^2;   /* LEDDOWN */
sbit BUZZ     = P2^3;   /* BUZZ    */

/* PORT 3: P3.0 RXD, P3.1 TXD */
sbit doorOPIN1 = P3^2;
sbit doorCLIN2 = P3^3;
sbit DoorEN    = P3^4;

sbit IN1 = P3^5;
sbit IN2 = P3^6;
sbit EN  = P3^7;


/* =========================
   GIAO THUC (phai giong U1)
   Khung 4 byte:
   [0xA5] [CMD] [FLOOR] [~(CMD ^ FLOOR)]
   ========================= */

#define FRAME_HEAD      0xA5

#define CMD_UP          0x01
#define CMD_DOWN        0x02
#define CMD_DOOR_OPEN   0x04
#define CMD_DOOR_CLOSE  0x08
#define CMD_WAIT        0x10
#define CMD_BUZZ        0x20

/* 20 x 50ms = 1 giay khong co khung hop le -> dung */
#define COMM_TIMEOUT    20

unsigned char rxState = 0;
unsigned char rxCmdTemp = 0;
unsigned char rxFloorTemp = 0;

unsigned char rxCmd = 0;
unsigned char rxFloor = 0;
bit rxNew = 0;

unsigned char commTimeout = COMM_TIMEOUT;


/* =========================
   MA LED 7 DOAN - COMMON ANODE
   ========================= */

unsigned char code seg_code[5] =
{
    0xC0,   /* 0 */
    0xF9,   /* 1 */
    0xA4,   /* 2 */
    0xB0,   /* 3 */
    0x99    /* 4 */
};

#define SEG_DASH 0xBF   /* chi sang doan g -> dau '-' */

void display_floor(unsigned char floor)
{
    if(floor > 4)
        return;

    SEG_PORT = seg_code[floor];
}


/* =========================
   XUAT RA CHAN THEO BYTE LENH
   ========================= */

void apply_cmd(unsigned char c)
{
    /* ----- Motor thang + LED huong ----- */
    if((c & CMD_UP) && !(c & CMD_DOWN))
    {
        IN1 = 1;
        IN2 = 0;
        EN  = 1;

        LED_UP   = 1;
        LED_DOWN = 0;
    }
    else if((c & CMD_DOWN) && !(c & CMD_UP))
    {
        IN1 = 0;
        IN2 = 1;
        EN  = 1;

        LED_UP   = 0;
        LED_DOWN = 1;
    }
    else
    {
        IN1 = 0;
        IN2 = 0;
        EN  = 0;

        LED_UP   = 0;
        LED_DOWN = 0;
    }

    /* ----- Motor cua ----- */
    if((c & CMD_DOOR_OPEN) && !(c & CMD_DOOR_CLOSE))
    {
        doorOPIN1 = 1;
        doorCLIN2 = 0;
        DoorEN    = 1;
    }
    else if((c & CMD_DOOR_CLOSE) && !(c & CMD_DOOR_OPEN))
    {
        doorOPIN1 = 0;
        doorCLIN2 = 1;
        DoorEN    = 1;
    }
    else
    {
        doorOPIN1 = 0;
        doorCLIN2 = 0;
        DoorEN    = 0;
    }

    /* ----- LED WAIT + coi ----- */
    LED_WAIT = (c & CMD_WAIT) ? 1 : 0;
    BUZZ     = (c & CMD_BUZZ) ? 1 : 0;
}


/* =========================
   UART - Timer 1 mode 2, 9600 baud
   ========================= */

void uart_init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x20;       /* Timer 1 mode 2 */

    TH1 = 0xFD;         /* 9600 baud @ 11.0592MHz */
    TL1 = 0xFD;

    SCON = 0x50;        /* mode 1, cho phep nhan */

    TR1 = 1;

    ES = 1;
}


/* =========================
   TIMER 0 - 50ms (dem timeout)
   ========================= */

void timer0_init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;

    TH0 = 0x4C;
    TL0 = 0x00;

    ET0 = 1;
    TR0 = 1;
}

void timer0_ISR(void) interrupt 1
{
    TH0 = 0x4C;
    TL0 = 0x00;

    if(commTimeout < 255)
        commTimeout++;
}


/* =========================
   NGAT UART - GHEP KHUNG
   ========================= */

void serial_ISR(void) interrupt 4
{
    unsigned char b;

    if(RI)
    {
        RI = 0;
        b = SBUF;

        if(rxState == 0)
        {
            if(b == FRAME_HEAD)
                rxState = 1;
        }
        else if(rxState == 1)
        {
            rxCmdTemp = b;
            rxState = 2;
        }
        else if(rxState == 2)
        {
            rxFloorTemp = b;
            rxState = 3;
        }
        else
        {
            /* Kiem tra byte kiem tra */
            if(b == (unsigned char)(~(rxCmdTemp ^ rxFloorTemp)))
            {
                rxCmd = rxCmdTemp;
                rxFloor = rxFloorTemp;
                rxNew = 1;
            }

            rxState = 0;
        }
    }

    if(TI)
        TI = 0;
}


/* =========================
   KHOI TAO
   ========================= */

void io_init(void)
{
    /* P0: hien dau '-' cho den khi nhan duoc khung dau tien */
    SEG_PORT = SEG_DASH;

    /* P2: LED + coi = 0 */
    P2 = 0x00;

    /* P3: RXD/TXD = 1, con lai = 0 */
    P3 = 0x03;
}


/* =========================
   MAIN
   ========================= */

void main(void)
{
    io_init();
    apply_cmd(0);

    uart_init();
    timer0_init();

    EA = 1;

    while(1)
    {
        if(rxNew)
        {
            rxNew = 0;
            commTimeout = 0;

            apply_cmd(rxCmd);
            display_floor(rxFloor);
        }

        /* Mat lien lac voi U1 -> dung an toan */
        if(commTimeout >= COMM_TIMEOUT)
        {
            apply_cmd(0);
            SEG_PORT = SEG_DASH;
        }
    }
}