/* =====================================================
   U1 - TRUNG TAM DIEU KHIEN GOI TANG
   AT89C52, thach anh 11.0592MHz

   - Doc nut goi tang (P1) + nut cabin (P2)
   - Xu ly logic thang may + thoi gian cua
   - Hien thi thong bao tren LCD 16x2 (che do 4 bit)
       D4..D7 = P0.0..P0.3 (BIT1..BIT4, co dien tro keo len)
       RS = P3.2, RW = P3.3, E = P3.4
   - Gui lenh + so tang sang U4 qua UART
     (U1 P3.1 TXD  --->  U4 P3.0 RXD)
   ===================================================== */

#include <reg52.h>
#include <intrins.h>

/* =========================
   KHAI BAO CHAN
   ========================= */

/* LCD 16x2 che do 4 bit
   Du lieu: P0.0..P0.3 = BIT1..BIT4 = D4..D7 (ghi bang P0)
   Dieu khien: RS1, RW1, E1 noi vao P3.2, P3.3, P3.4 */
sbit LCD_RS = P3^2;
sbit LCD_RW = P3^3;
sbit LCD_E  = P3^4;

/* PORT 1: nut goi tang */
sbit BTN_0_FLU = P1^7;   /* BTN0U */

sbit BTN_1_FLD = P1^6;   /* BTN1D */
sbit BTN_1_FLU = P1^5;   /* BTN1U */

sbit BTN_2_FLD = P1^4;   /* BTN2D */
sbit BTN_2_FLU = P1^3;   /* BTN2U */

sbit BTN_3_FLD = P1^2;   /* BTN3D */
sbit BTN_3_FLU = P1^1;   /* BTN3U */

sbit BTN_4_FLD = P1^0;   /* BTN4  */

/* PORT 3.7: nut STOP khan cap, active-low (can tro keo len 10k khi lam mach that) */
sbit BTN_STOP = P3^7;

/* PORT 2: LED bao da goi tang, sang toi khi thang phuc vu xong
   Moi LED ung voi dung 1 nut o P1 (xem so do U1: BTNFL4D..BTNFL0U).
   Gia su LED noi qua tro xuong GND, muc 1 = sang.
   Neu LED sang nguoc (LED dau kia len VCC), doi P2 = ~ledLatch. */
#define LED_4D  0x01   /* P2.0 - sang khi nhan BTN_4_FLD (P1.0) */
#define LED_3D  0x02   /* P2.1 - sang khi nhan BTN_3_FLD (P1.2) */
#define LED_2D  0x04   /* P2.2 - sang khi nhan BTN_2_FLD (P1.4) */
#define LED_1D  0x08   /* P2.3 - sang khi nhan BTN_1_FLD (P1.6) */
#define LED_3U  0x10   /* P2.4 - sang khi nhan BTN_3_FLU (P1.1) */
#define LED_2U  0x20   /* P2.5 - sang khi nhan BTN_2_FLU (P1.3) */
#define LED_1U  0x40   /* P2.6 - sang khi nhan BTN_1_FLU (P1.5) */
#define LED_0U  0x80   /* P2.7 - sang khi nhan BTN_0_FLU (P1.7) */

unsigned char ledLatch = 0x00;   /* anh xa truc tiep 8 bit cua P2 */

/* PORT 3: P3.0 = RXD, P3.1 = TXD (UART noi voi U4) */


/* =========================
   GIAO THUC UART U1 -> U4
   Khung 4 byte:
   [0xA5] [CMD] [FLOOR] [~(CMD ^ FLOOR)]
   ========================= */

#define FRAME_HEAD      0xA5

#define CMD_UP          0x01   /* motor thang len  + LED UP   */
#define CMD_DOWN        0x02   /* motor thang xuong + LED DOWN */
#define CMD_DOOR_OPEN   0x04   /* motor cua chieu mo   */
#define CMD_DOOR_CLOSE  0x08   /* motor cua chieu dong */
#define CMD_WAIT        0x10   /* LED WAIT */
#define CMD_BUZZ        0x20   /* coi */

unsigned char outCmd = 0;

/* Keu coi 6 x 50ms = 300ms */
#define BUZZ_TIME 6
unsigned char buzz_timer = 0;


/* =========================
   BIEN HE THONG
   ========================= */

unsigned char requests[5]   = { 0, 0, 0, 0, 0 };
unsigned char buttonLock[5] = { 0, 0, 0, 0, 0 };

char currentFloor = 0;
char targetFloor = -1;

bit isMoving = 0;

/* Dung khan cap: 1 = dang giu nut STOP */
bit emergency = 0;

unsigned int timer_count = 0;

/*
   doorState:
   0 = cua dong
   1 = cua dang mo
   2 = cua dang giu
   3 = cua dang dong
*/
unsigned char doorState = 0;
unsigned char door_timer = 0;

/* Timer 0 = 50ms */
#define DOOR_OPEN_TIME  20
#define DOOR_WAIT_TIME  20
#define DOOR_CLOSE_TIME 20


/* =========================
   HAM DELAY
   ========================= */

void delay_ms(unsigned int ms)
{
    unsigned int i;
    unsigned int j;

    for(i = 0; i < ms; i++)
    {
        for(j = 0; j < 120; j++)
        {
            ;
        }
    }
}


/* =========================
   LCD 16x2 - 4 BIT
   Chi ghi (RW luon = 0), khong doc busy
   ========================= */

void lcd_delay(void)
{
    unsigned char i;

    for(i = 0; i < 40; i++)
    {
        _nop_();
    }
}

void lcd_write_nibble(unsigned char nib, unsigned char rs)
{
    LCD_E  = 0;
    LCD_RW = 0;
    LCD_RS = (rs != 0);

    /* Chi dung P0.0..P0.3, cac bit cao de muc 1 */
    P0 = 0xF0 | (nib & 0x0F);

    _nop_();
    _nop_();

    LCD_E = 1;

    _nop_();
    _nop_();
    _nop_();
    _nop_();

    LCD_E = 0;

    lcd_delay();
}

void lcd_cmd(unsigned char c)
{
    lcd_write_nibble(c >> 4, 0);
    lcd_write_nibble(c & 0x0F, 0);

    /* Lenh clear / home can cho lau hon */
    if(c <= 0x03)
        delay_ms(3);
}

void lcd_data(unsigned char c)
{
    lcd_write_nibble(c >> 4, 1);
    lcd_write_nibble(c & 0x0F, 1);
}

void lcd_goto(unsigned char row)
{
    if(row == 0)
        lcd_cmd(0x80);
    else
        lcd_cmd(0xC0);
}

void lcd_print(char *s)
{
    while(*s)
    {
        lcd_data(*s);
        s++;
    }
}

void lcd_init(void)
{
    delay_ms(30);

    /* Trinh tu dua LCD ve che do 4 bit */
    lcd_write_nibble(0x03, 0);
    delay_ms(5);
    lcd_write_nibble(0x03, 0);
    delay_ms(1);
    lcd_write_nibble(0x03, 0);
    delay_ms(1);
    lcd_write_nibble(0x02, 0);
    delay_ms(1);

    lcd_cmd(0x28);      /* 4 bit, 2 dong, 5x8 */
    lcd_cmd(0x0C);      /* bat hien thi, tat con tro */
    lcd_cmd(0x06);      /* tu dong tang con tro */
    lcd_cmd(0x01);      /* xoa man hinh */
}


/* =========================
   THONG BAO LCD
   Dong 1: "Tang hien tai: X"
   Dong 2: trang thai thang may
   Chi ve lai khi co thay doi
   ========================= */

char lcdFloor = -1;
unsigned char lcdState = 0xFF;
char lcdTarget = -2;

unsigned char digit_char(char n)
{
    if(n >= 0 && n <= 4)
        return (unsigned char)('0' + n);

    return '-';
}

void lcd_update(void)
{
    char floor_now;
    char target_now;
    unsigned char st;

    /* Dung khan cap: ghi de man hinh, bo qua trang thai khac */
    if(emergency)
    {
        if(lcdState != 6)
        {
            lcd_goto(0);
            lcd_print("!! DUNG KHAN CAP");
            lcd_goto(1);
            lcd_print("Nha nut de chay ");

            lcdState = 6;
            lcdFloor = -1;    /* buoc ve lai dong 1 khi het khan cap */
        }

        return;
    }

    /* Chup lai trang thai hien tai */
    floor_now  = currentFloor;
    target_now = targetFloor;

    /*
       st:
       0 = san sang
       1 = dang len
       2 = dang xuong
       3 = cua dang mo
       4 = cua dang giu
       5 = cua dang dong
       (6 = dung khan cap, xu ly rieng o tren)
    */
    if(doorState == 1)
        st = 3;
    else if(doorState == 2)
        st = 4;
    else if(doorState == 3)
        st = 5;
    else if(isMoving)
    {
        if(outCmd & CMD_UP)
            st = 1;
        else
            st = 2;
    }
    else
        st = 0;

    /* Khong doi gi thi khong ve lai (tranh nhap nhay) */
    if(floor_now == lcdFloor && st == lcdState && target_now == lcdTarget)
        return;

    /* Dong 1: tang hien tai */
    if(floor_now != lcdFloor)
    {
        lcd_goto(0);
        lcd_print("Tang hien tai: ");
        lcd_data(digit_char(floor_now));
    }

    /* Dong 2: trang thai */
    if(st != lcdState || target_now != lcdTarget)
    {
        lcd_goto(1);

        switch(st)
        {
            case 0:
                lcd_print("San sang        ");
                break;

            case 1:
                lcd_print("Di len -> tang ");
                lcd_data(digit_char(target_now));
                break;

            case 2:
                lcd_print("Di xuong->tang ");
                lcd_data(digit_char(target_now));
                break;

            case 3:
                lcd_print("Da den - Mo cua ");
                break;

            case 4:
                lcd_print("Moi vao thang   ");
                break;

            default:
                lcd_print("Cua dang dong   ");
                break;
        }
    }

    lcdFloor  = floor_now;
    lcdState  = st;
    lcdTarget = target_now;
}


/* =========================
   LED WAIT (dieu khien qua U4)
   ========================= */

void wait_led_on(void)
{
    outCmd |= CMD_WAIT;
}

void wait_led_off(void)
{
    outCmd &= ~CMD_WAIT;
}


/* =========================
   MOTOR THANG (dieu khien qua U4)
   ========================= */

void stop_motor(void)
{
    outCmd &= ~(CMD_UP | CMD_DOWN);

    isMoving = 0;
}

void start_up(void)
{
    outCmd &= ~CMD_DOWN;
    outCmd |= CMD_UP;

    isMoving = 1;
    timer_count = 0;
}

void start_down(void)
{
    outCmd &= ~CMD_UP;
    outCmd |= CMD_DOWN;

    isMoving = 1;
    timer_count = 0;
}


/* =========================
   MOTOR CUA (dieu khien qua U4)
   ========================= */

void stop_door_motor(void)
{
    outCmd &= ~(CMD_DOOR_OPEN | CMD_DOOR_CLOSE);
}

void start_door_open(void)
{
    if(doorState != 0)
        return;

    wait_led_on();

    outCmd &= ~CMD_DOOR_CLOSE;
    outCmd |= CMD_DOOR_OPEN;

    /* Keu coi bao den tang */
    buzz_timer = BUZZ_TIME;
    outCmd |= CMD_BUZZ;

    door_timer = DOOR_OPEN_TIME;
    doorState = 1;
}

void hold_door_open(void)
{
    stop_door_motor();

    wait_led_on();

    door_timer = DOOR_WAIT_TIME;
    doorState = 2;
}

void start_door_close(void)
{
    wait_led_on();

    outCmd &= ~CMD_DOOR_OPEN;
    outCmd |= CMD_DOOR_CLOSE;

    door_timer = DOOR_CLOSE_TIME;
    doorState = 3;
}

void finish_door_close(void)
{
    stop_door_motor();

    wait_led_off();

    doorState = 0;
    targetFloor = -1;
}


/* =========================
   CAP NHAT YEU CAU TANG
   ========================= */

void update_floor_request(unsigned char floor,
                          unsigned char pressed)
{
    if(pressed)
    {
        /* Chi tao request o lan nhan dau tien */
        if(buttonLock[floor] == 0)
        {
            requests[floor] = 1;
            buttonLock[floor] = 1;
        }
    }
    else
    {
        buttonLock[floor] = 0;
    }
}


/* =========================
   TAT LED CUA 1 TANG KHI DA PHUC VU XONG
   Tang 1,2,3 co 2 nut (len/xuong) nen tat ca 2 LED cung luc,
   vi requests[] hien gop chung 1 bit cho ca tang.
   ========================= */

void clear_floor_leds(unsigned char floor)
{
    switch(floor)
    {
        case 0: ledLatch &= (unsigned char)~LED_0U; break;
        case 1: ledLatch &= (unsigned char)~(LED_1D | LED_1U); break;
        case 2: ledLatch &= (unsigned char)~(LED_2D | LED_2U); break;
        case 3: ledLatch &= (unsigned char)~(LED_3D | LED_3U); break;
        case 4: ledLatch &= (unsigned char)~LED_4D; break;
        default: break;
    }

    P2 = ledLatch;
}


/* =========================
   QUET CAC NUT
   ========================= */

void scan_buttons(void)
{
    unsigned char pressed0;
    unsigned char pressed1;
    unsigned char pressed2;
    unsigned char pressed3;
    unsigned char pressed4;

    /* Tang 0 */
    pressed0 = 0;
    if(BTN_0_FLU == 0) { pressed0 = 1; ledLatch |= LED_0U; }

    /* Tang 1 */
    pressed1 = 0;
    if(BTN_1_FLD == 0) { pressed1 = 1; ledLatch |= LED_1D; }
    if(BTN_1_FLU == 0) { pressed1 = 1; ledLatch |= LED_1U; }

    /* Tang 2 */
    pressed2 = 0;
    if(BTN_2_FLD == 0) { pressed2 = 1; ledLatch |= LED_2D; }
    if(BTN_2_FLU == 0) { pressed2 = 1; ledLatch |= LED_2U; }

    /* Tang 3 */
    pressed3 = 0;
    if(BTN_3_FLD == 0) { pressed3 = 1; ledLatch |= LED_3D; }
    if(BTN_3_FLU == 0) { pressed3 = 1; ledLatch |= LED_3U; }

    /* Tang 4 */
    pressed4 = 0;
    if(BTN_4_FLD == 0) { pressed4 = 1; ledLatch |= LED_4D; }

    update_floor_request(0, pressed0);
    update_floor_request(1, pressed1);
    update_floor_request(2, pressed2);
    update_floor_request(3, pressed3);
    update_floor_request(4, pressed4);

    P2 = ledLatch;   /* xuat het 8 LED cung 1 lan */
}


/* =========================
   TIM TANG GAN NHAT
   ========================= */

char find_nearest_request(void)
{
    char i;
    char distance;
    char bestFloor = -1;
    char bestDistance = 100;

    for(i = 0; i < 5; i++)
    {
        if(requests[i])
        {
            if(i >= currentFloor)
                distance = i - currentFloor;
            else
                distance = currentFloor - i;

            if(distance < bestDistance)
            {
                bestDistance = distance;
                bestFloor = i;
            }
        }
    }

    return bestFloor;
}


/* =========================
   XU LY THANG MAY
   ========================= */

void process_elevator(void)
{
    char nearest;

    /* Dang dung khan cap: khong tao lenh moi, cho ISR xu ly rieng */
    if(emergency)
        return;

    /* Khong xu ly khi cua dang hoat dong */
    if(doorState != 0)
        return;

    /* Thang dang chay thi khong tao lenh moi */
    if(isMoving)
        return;

    /* Co yeu cau ngay tai tang hien tai: xoa request, mo cua */
    if(requests[currentFloor])
    {
        requests[currentFloor] = 0;
        clear_floor_leds((unsigned char)currentFloor);
        targetFloor = -1;

        start_door_open();

        return;
    }

    nearest = find_nearest_request();

    if(nearest >= 0)
    {
        targetFloor = nearest;

        if(targetFloor > currentFloor)
        {
            start_up();
        }
        else if(targetFloor < currentFloor)
        {
            start_down();
        }
    }
}


/* =========================
   UART (Timer 1 mode 2, 9600 baud)
   ========================= */

void uart_init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x20;       /* Timer 1 mode 2 - tao baud */

    TH1 = 0xFD;         /* 9600 baud @ 11.0592MHz */
    TL1 = 0xFD;

    SCON = 0x40;        /* mode 1, 8 bit */
    TI = 0;

    TR1 = 1;
}

void uart_send(unsigned char b)
{
    SBUF = b;
    while(!TI);
    TI = 0;
}

void send_state(void)
{
    unsigned char c;
    unsigned char f;

    c = outCmd;
    f = (unsigned char)currentFloor;

    uart_send(FRAME_HEAD);
    uart_send(c);
    uart_send(f);
    uart_send((unsigned char)(~(c ^ f)));
}


/* =========================
   TIMER 0 - 50ms @ 11.0592MHz
   65536 - 46080 = 19456 = 0x4C00
   ========================= */

void timer0_init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;       /* Timer 0 mode 1 (16 bit) */

    TH0 = 0x4C;
    TL0 = 0x00;

    ET0 = 1;
    EA = 1;

    TR0 = 1;
}


/* =========================
   NGAT TIMER 0
   ========================= */

void timer0_ISR(void) interrupt 1
{
    TH0 = 0x4C;
    TL0 = 0x00;

    /* ---------- Nut STOP khan cap ---------- */
    if(BTN_STOP == 0)
    {
        emergency = 1;

        /* Dung ngay dong co thang va dong co cua, giu nguyen vi tri/tien trinh */
        outCmd &= (unsigned char)~(CMD_UP | CMD_DOWN | CMD_DOOR_OPEN | CMD_DOOR_CLOSE);
        isMoving = 0;

        /* Coi keu lien tuc trong luc dung khan cap */
        outCmd |= CMD_BUZZ;

        return;   /* khong xu ly cua/thang trong luc dang dung */
    }
    else if(emergency)
    {
        /* Vua nha nut STOP: cho chay/mo/dong cua tiep dung trang thai dang do dang */
        emergency = 0;

        if(doorState == 1)
            outCmd |= CMD_DOOR_OPEN;
        else if(doorState == 3)
            outCmd |= CMD_DOOR_CLOSE;

        outCmd &= (unsigned char)~CMD_BUZZ;
        buzz_timer = 0;
    }

    /* Coi */
    if(buzz_timer > 0)
    {
        buzz_timer--;

        if(buzz_timer == 0)
            outCmd &= ~CMD_BUZZ;
    }

    /* Xu ly cua */
    if(doorState != 0)
    {
        if(door_timer > 0)
        {
            door_timer--;

            if(door_timer == 0)
            {
                if(doorState == 1)
                {
                    hold_door_open();
                }
                else if(doorState == 2)
                {
                    start_door_close();
                }
                else if(doorState == 3)
                {
                    finish_door_close();
                }
            }
        }

        return;
    }

    /* Xu ly thang */
    if(isMoving)
    {
        timer_count++;

        /* 40 x 50ms = 2 giay / tang */
        if(timer_count >= 40)
        {
            timer_count = 0;

            if(outCmd & CMD_UP)
            {
                if(currentFloor < 4)
                    currentFloor++;
            }
            else if(outCmd & CMD_DOWN)
            {
                if(currentFloor > 0)
                    currentFloor--;
            }

            /* Da den tang dich */
            if(currentFloor == targetFloor)
            {
                stop_motor();

                requests[currentFloor] = 0;
                clear_floor_leds((unsigned char)currentFloor);

                start_door_open();
            }

            /* Di ngang qua tang co yeu cau thi dung lai */
            else if(requests[currentFloor])
            {
                stop_motor();

                requests[currentFloor] = 0;
                clear_floor_leds((unsigned char)currentFloor);

                targetFloor = -1;

                start_door_open();
            }
        }
    }
}


/* =========================
   KHOI TAO HE THONG
   ========================= */

void system_init(void)
{
    P0 = 0xFF;      /* LCD data (P0.0..P0.3) */
    P1 = 0xFF;      /* nut nhan */
    P2 = 0x00;      /* LED bao goi tang: tat het luc khoi dong */
    P3 = 0xFF;      /* RXD/TXD + RS/RW/E muc cao */

    ledLatch = 0;

    outCmd = 0;
    buzz_timer = 0;

    isMoving = 0;
    emergency = 0;

    currentFloor = 0;
    targetFloor = -1;

    timer_count = 0;

    doorState = 0;
    door_timer = 0;

    requests[0] = 0;
    requests[1] = 0;
    requests[2] = 0;
    requests[3] = 0;
    requests[4] = 0;

    buttonLock[0] = 0;
    buttonLock[1] = 0;
    buttonLock[2] = 0;
    buttonLock[3] = 0;
    buttonLock[4] = 0;

    /* LCD + man hinh chao */
    lcd_init();

    lcd_goto(0);
    lcd_print("THANG MAY 4 TANG");
    lcd_goto(1);
    lcd_print("Dang khoi dong..");
    delay_ms(800);

    /* UART truoc, Timer 0 sau */
    uart_init();
    timer0_init();
}


/* =========================
   HAM MAIN
   ========================= */

void main(void)
{
    system_init();

    while(1)
    {
        scan_buttons();

        process_elevator();

        /* Gui trang thai + so tang sang U4 */
        send_state();

        /* Cap nhat thong bao LCD */
        lcd_update();

        delay_ms(20);
    }
}