#ifndef LOWLEVEL_H
#define LOWLEVEL_H

// system characteristics
//#define PICO_250_MHZ
#define CPU_FREQUENCY  1000000 //Hz
#define FRAME_RATE     50 //Hz
#define FRAME_TIME_US  (1000000 / FRAME_RATE)
#define SAMPLE_RATE    96000 //Hz

// bank registers
#define PORT_BANK_0           0xFE00
#define PORT_BANK_1           0xFE01
#define PORT_BANK_2           0xFE02
#define PORT_BANK_3           0xFE03
// serial port registers
#define PORT_SERIAL_0_FLAGS   0xFE10 //bit 7: out-buffer full, bit 6: in-data available
#define PORT_SERIAL_0_IN      0xFE11
#define PORT_SERIAL_0_OUT     0xFE12
// SID registers
#define PORT_SID_00           0xFE20
#define PORT_SID_01           0xFE21
#define PORT_SID_02           0xFE22
#define PORT_SID_03           0xFE23
#define PORT_SID_04           0xFE24
#define PORT_SID_05           0xFE25
#define PORT_SID_06           0xFE26
#define PORT_SID_07           0xFE27
#define PORT_SID_08           0xFE28
#define PORT_SID_09           0xFE29
#define PORT_SID_0A           0xFE2A
#define PORT_SID_0B           0xFE2B
#define PORT_SID_0C           0xFE2C
#define PORT_SID_0D           0xFE2D
#define PORT_SID_0E           0xFE2E
#define PORT_SID_0F           0xFE2F
#define PORT_SID_10           0xFE30
#define PORT_SID_11           0xFE31
#define PORT_SID_12           0xFE32
#define PORT_SID_13           0xFE33
#define PORT_SID_14           0xFE34
#define PORT_SID_15           0xFE35
#define PORT_SID_16           0xFE36
#define PORT_SID_17           0xFE37
#define PORT_SID_18           0xFE38
#define PORT_SID_19           0xFE39
#define PORT_SID_1A           0xFE3A
#define PORT_SID_1B           0xFE3B
#define PORT_SID_1C           0xFE3C
// frame counter register
#define PORT_FRAME_COUNTER_LO 0xFE40
#define PORT_FRAME_COUNTER_HI 0xFE41
// file system registers
#define PORT_FILE_CMD         0xFE60
#define PORT_FILE_DATA        0xFE61
#define PORT_FILE_STATUS      0xFE62
// IRQ timer port registers
#define PORT_IRQ_TIMER_LO     0xFE80  //writing the lo-value fills the intermediate register
#define PORT_IRQ_TIMER_HI     0xFE81  //writing the hi-value sets the timer from the completed intermediate register
#define PORT_IRQ_TIMER_RESET  0xFE82  //timer = 0 so it can count a full cycle again
#define PORT_IRQ_TIMER_TRIG   0xFE83  //timer = max timer-value, so it triggers an irq
#define PORT_IRQ_TIMER_PAUSE  0xFE84  //pause the timer indefinitely
#define PORT_IRQ_TIMER_CONT   0xFE85  //continue the timer after a pause, also acks a triggered timer
uint8_t portMem6502[256];

// pin definitions
#define PD0            0
#define PD1            1
#define PD2            2
#define PD3            3
#define PD4            4
#define PD5            5
#define PD6            6
#define PD7            7
#define RW             8
#define MEM_CS_N       9
#define CLK            10
#define A_LO_EN_N      11
#define A_HI_EN_N      12
#define D_EN_N         13
#define BANK_LE        14
#define CTRL_LE        15
#define SPI_RX         16
#define SD_CS_N        17
#define SPI_CLK        18
#define SPI_TX         19
#define LCD_CS_N       20
#define LED_PIN        25
#define AUDIO_PIN      28
// ctrl-port
#define CTRL_IRQ_N     1
#define CTRL_NMI_N     2
#define CTRL_RST_N     4
#define CTRL_BE        8
uint8_t ctrlValue;

int32_t frameTimer;
uint16_t frameCounter;

#define wait1() gpio_get(LED_PIN)

static inline void setBank(uint8_t value) 
{
    gpio_set_dir_out_masked(255);
    gpio_put_masked(255, value);
    gpio_put(BANK_LE, HIGH);
    gpio_put(BANK_LE, LOW);  
}

static inline void setCtrl(uint8_t value) 
{
    gpio_set_dir_out_masked(255);
    gpio_put_masked(255, value);
    gpio_put(CTRL_LE, HIGH);
    gpio_put(CTRL_LE, LOW);  
}

static inline void setCtrlFast(uint8_t value) 
{   //WARNING: only use if the pins are already outputs, to save some time
    //gpio_set_dir_out_masked(255);
    gpio_put_masked(255, value);
    gpio_put(CTRL_LE, HIGH);
    gpio_put(CTRL_LE, LOW);   
}

static inline uint16_t getAddr() 
{
    uint16_t value;
    gpio_set_dir_in_masked(255);
    gpio_put(A_HI_EN_N, LOW);
    wait1();
    wait1();
    wait1();
    #ifdef PICO_250_MHZ
    wait1();
    #endif
    value = (gpio_get_all() & 255) << 8;
    gpio_put(A_HI_EN_N, HIGH);      
    gpio_put(A_LO_EN_N, LOW);
    wait1();
    wait1();
    wait1();
    #ifdef PICO_250_MHZ
    wait1();
    #endif
    value = value | (gpio_get_all() & 255);
    gpio_put(A_LO_EN_N, HIGH);      
    return value;
}

static inline uint8_t getData() 
{
    uint8_t value;    
    gpio_set_dir_in_masked(255);
    gpio_put(D_EN_N, LOW);
    wait1();
    wait1();
    wait1();
    #ifdef PICO_250_MHZ
    wait1();
    #endif
    value = gpio_get_all() & 255;
    gpio_put(D_EN_N, HIGH);  
    return value;
}

static inline void setDataAndClk(uint8_t value) 
{
    gpio_set_dir_out_masked(255);
    gpio_put_masked(255, value);
    gpio_put(D_EN_N, LOW);
    #ifdef PICO_250_MHZ
    wait1();
    #endif
    gpio_put(CLK, LOW);    
    gpio_put(D_EN_N, HIGH);
}

void reset6502() 
{
    ctrlValue = 255 & ~CTRL_RST_N;
    setCtrl(ctrlValue);
    busy_wait_us_32(1);
    for (int i = 0; i < 16; i++)
    {
        gpio_put(CLK, HIGH);
        busy_wait_us_32(1);
        gpio_put(CLK, LOW);
        busy_wait_us_32(1);
    }
    ctrlValue = ctrlValue | CTRL_RST_N;
    setCtrl(ctrlValue);
}

#endif //LOWLEVEL_H
