#include <reg52.h>

/* =========================
   KHAI BAO CHAN
   ========================= */

/* PORT 0: 7 doan */
#define SEG_PORT P0

/* PORT 1: nut goi tang */
sbit BTN_0_FLU = P1^7;

sbit BTN_1_FLD = P1^6;
sbit BTN_1_FLU = P1^5;

sbit BTN_2_FLD = P1^4;
sbit BTN_2_FLU = P1^3;

sbit BTN_3_FLD = P1^2;
sbit BTN_3_FLU = P1^1;

sbit BTN_4_FLD = P1^0;

/* PORT 2: nut cabin */
sbit BTN_CABIN_0 = P2^0;
sbit BTN_CABIN_1 = P2^1;
sbit BTN_CABIN_2 = P2^2;
sbit BTN_CABIN_3 = P2^3;
sbit BTN_CABIN_4 = P2^4;

/* LED WAIT doi sang P2.7 */
sbit LED_WAIT = P2^7;

/* PORT 3 */
sbit LED_UP   = P3^0;
sbit LED_DOWN = P3^4;

/* Motor thang */
sbit IN1 = P3^5;
sbit IN2 = P3^6;
sbit EN  = P3^7;

/* Motor cua */
sbit doorOPIN1 = P3^1;
sbit doorCLIN2 = P3^2;
sbit DoorEN    = P3^3;


/* =========================
   MA LED 7 DOAN
   COMMON ANODE
   ========================= */

unsigned char code seg_code[5] =
{
    0xC0,   /* 0 */
    0xF9,   /* 1 */
    0xA4,   /* 2 */
    0xB0,   /* 3 */
    0x99    /* 4 */
};


/* =========================
   BIEN HE THONG
   ========================= */

unsigned char requests[5] =
{
    0, 0, 0, 0, 0
};

/* Khoa nut de tranh nhan giu gay lap lenh */
unsigned char buttonLock[5] =
{
    0, 0, 0, 0, 0
};

char currentFloor = 0;
char targetFloor = -1;

bit isMoving = 0;

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


/* =========================
   THOI GIAN CUA
   Timer 1 = 50ms
   ========================= */

#define DOOR_OPEN_TIME  20
#define DOOR_WAIT_TIME  20
#define DOOR_CLOSE_TIME 20


/* =========================
   HIEN THI TANG
   ========================= */

void display_floor(unsigned char floor)
{
    if(floor > 4)
        floor = 0;

    P0 = seg_code[floor];
}


/* =========================
   LED WAIT
   P2.7 dung logic muc cao
   ========================= */

void wait_led_on(void)
{
    LED_WAIT = 1;
}

void wait_led_off(void)
{
    LED_WAIT = 0;
}


/* =========================
   MOTOR THANG
   ========================= */

void stop_motor(void)
{
    IN1 = 0;
    IN2 = 0;
    EN = 0;

    LED_UP = 0;
    LED_DOWN = 0;

    isMoving = 0;
}

void start_up(void)
{
    IN1 = 1;
    IN2 = 0;
    EN = 1;

    LED_UP = 1;
    LED_DOWN = 0;

    isMoving = 1;
    timer_count = 0;
}

void start_down(void)
{
    IN1 = 0;
    IN2 = 1;
    EN = 1;

    LED_UP = 0;
    LED_DOWN = 1;

    isMoving = 1;
    timer_count = 0;
}


/* =========================
   MOTOR CUA
   ========================= */

void stop_door_motor(void)
{
    doorOPIN1 = 0;
    doorCLIN2 = 0;
    DoorEN = 0;
}

void start_door_open(void)
{
    if(doorState != 0)
        return;

    /* Bat LED WAIT khi cua mo */
    wait_led_on();

    /* Motor cua quay chieu mo */
    doorOPIN1 = 1;
    doorCLIN2 = 0;
    DoorEN = 1;

    door_timer = DOOR_OPEN_TIME;
    doorState = 1;
}

void hold_door_open(void)
{
    /* Dung motor */
    stop_door_motor();

    /* Van giu LED WAIT sang */
    wait_led_on();

    door_timer = DOOR_WAIT_TIME;
    doorState = 2;
}

void start_door_close(void)
{
    /* Bat LED WAIT trong luc cua dong */
    wait_led_on();

    /* Motor cua quay chieu dong */
    doorOPIN1 = 0;
    doorCLIN2 = 1;
    DoorEN = 1;

    door_timer = DOOR_CLOSE_TIME;
    doorState = 3;
}

void finish_door_close(void)
{
    /* Dung motor cua */
    stop_door_motor();

    /* Cua dong xong thi tat WAIT */
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
        /*
           Chi tao request tai lan nhan dau tien.
           Neu giu nut thi khong tao request moi.
        */
        if(buttonLock[floor] == 0)
        {
            requests[floor] = 1;
            buttonLock[floor] = 1;
        }
    }
    else
    {
        /*
           Khi nha nut thi mo khoa.
           Lan nhan tiep theo moi duoc ghi nhan.
        */
        buttonLock[floor] = 0;
    }
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

    /*
       Tang 0:
       - Nut cabin 0
       - Nut goi tang 0 len
    */
    pressed0 = 0;

    if(BTN_CABIN_0 == 0)
        pressed0 = 1;

    if(BTN_0_FLU == 0)
        pressed0 = 1;


    /*
       Tang 1:
       - Nut cabin 1
       - Nut goi xuong
       - Nut goi len
    */
    pressed1 = 0;

    if(BTN_CABIN_1 == 0)
        pressed1 = 1;

    if(BTN_1_FLD == 0)
        pressed1 = 1;

    if(BTN_1_FLU == 0)
        pressed1 = 1;


    /*
       Tang 2
    */
    pressed2 = 0;

    if(BTN_CABIN_2 == 0)
        pressed2 = 1;

    if(BTN_2_FLD == 0)
        pressed2 = 1;

    if(BTN_2_FLU == 0)
        pressed2 = 1;


    /*
       Tang 3
    */
    pressed3 = 0;

    if(BTN_CABIN_3 == 0)
        pressed3 = 1;

    if(BTN_3_FLD == 0)
        pressed3 = 1;

    if(BTN_3_FLU == 0)
        pressed3 = 1;


    /*
       Tang 4
       - Nut cabin 4
       - Nut goi xuong
    */
    pressed4 = 0;

    if(BTN_CABIN_4 == 0)
        pressed4 = 1;

    if(BTN_4_FLD == 0)
        pressed4 = 1;


    /* Cap nhat request */
    update_floor_request(0, pressed0);
    update_floor_request(1, pressed1);
    update_floor_request(2, pressed2);
    update_floor_request(3, pressed3);
    update_floor_request(4, pressed4);
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

    /* Khong xu ly khi cua dang hoat dong */
    if(doorState != 0)
        return;

    /* Neu thang dang chay thi khong tao lenh moi */
    if(isMoving)
        return;

    /*
       Neu co yeu cau ngay tai tang hien tai:
       - Xoa request truoc
       - Mo cua mot lan
    */
    if(requests[currentFloor])
    {
        requests[currentFloor] = 0;
        targetFloor = -1;

        start_door_open();

        return;
    }


    /* Tim tang co yeu cau gan nhat */
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
   TIMER 1
   50ms
   11.0592MHz
   ========================= */

void timer1_init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x10;

    /*
       50ms voi thach anh 11.0592MHz

       TH1 = 3C
       TL1 = B0
    */
    TH1 = 0x3C;
    TL1 = 0xB0;

    ET1 = 1;
    EA = 1;

    TR1 = 1;
}


/* =========================
   NGAT TIMER 1
   ========================= */

void timer1_ISR(void) interrupt 3
{
    TH1 = 0x3C;
    TL1 = 0xB0;


    /* =====================
       XU LY CUA
       ===================== */

    if(doorState != 0)
    {
        if(door_timer > 0)
        {
            door_timer--;

            if(door_timer == 0)
            {
                /*
                   Cua mo xong
                */
                if(doorState == 1)
                {
                    hold_door_open();
                }

                /*
                   Het thoi gian giu cua
                */
                else if(doorState == 2)
                {
                    start_door_close();
                }

                /*
                   Cua dong xong
                */
                else if(doorState == 3)
                {
                    finish_door_close();
                }
            }
        }

        return;
    }


    /* =====================
       XU LY THANG
       ===================== */

    if(isMoving)
    {
        timer_count++;

        /*
           40 x 50ms = 2 giay
           Moi 2 giay di 1 tang
        */
        if(timer_count >= 40)
        {
            timer_count = 0;


            /*
               Dang di len
            */
            if(IN1 == 1 && IN2 == 0)
            {
                if(currentFloor < 4)
                    currentFloor++;
            }


            /*
               Dang di xuong
            */
            else if(IN1 == 0 && IN2 == 1)
            {
                if(currentFloor > 0)
                    currentFloor--;
            }


            /* Cap nhat hien thi */
            display_floor(currentFloor);


            /*
               Da den tang dich
            */
            if(currentFloor == targetFloor)
            {
                stop_motor();

                requests[currentFloor] = 0;

                start_door_open();
            }


            /*
               Neu dang di qua va tang hien tai
               cung dang co yeu cau thi dung lai
            */
            else if(requests[currentFloor])
            {
                stop_motor();

                requests[currentFloor] = 0;

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
    /* P0 dung cho 7 doan */
    P0 = 0x00;

    /* P1 la nut nhan */
    P1 = 0xFF;

    /*
       P2:
       P2.0-P2.4: nut cabin
       P2.7: LED WAIT
    */
    P2 = 0xFF;

    /* P3 */
    P3 = 0x00;


    /* LED WAIT tat */
    wait_led_off();


    /* LED huong tat */
    LED_UP = 0;
    LED_DOWN = 0;


    /* Motor thang dung */
    IN1 = 0;
    IN2 = 0;
    EN = 0;


    /* Motor cua dung */
    doorOPIN1 = 0;
    doorCLIN2 = 0;
    DoorEN = 0;


    /* Bien he thong */
    isMoving = 0;

    currentFloor = 0;
    targetFloor = -1;

    timer_count = 0;

    doorState = 0;
    door_timer = 0;


    /* Xoa request */
    requests[0] = 0;
    requests[1] = 0;
    requests[2] = 0;
    requests[3] = 0;
    requests[4] = 0;


    /* Mo khoa cac nut */
    buttonLock[0] = 0;
    buttonLock[1] = 0;
    buttonLock[2] = 0;
    buttonLock[3] = 0;
    buttonLock[4] = 0;


    /* Hien thi tang 0 */
    display_floor(0);


    /* Khoi dong Timer 1 */
    timer1_init();
}


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
   HAM MAIN
   ========================= */
void main(void)
{
    system_init();

    while(1)
    {
        /*
           Quet nut moi 20ms.
           ButtonLock dam bao giu nut
           khong lam cua mo lap lai.
        */
        scan_buttons();

        process_elevator();

        delay_ms(20);
    }
}