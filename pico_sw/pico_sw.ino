#define PD0            0
#define PD1            1
#define PD2            2
#define PD3            3
#define PD4            4
#define PD5            5
#define PD6            6
#define PD7            7
#define RW             8
#define SYNC           9
#define CLK            10
#define A_LO_EN_N      11
#define A_HI_EN_N      12
#define D_EN_N         13
#define BANK_LE        14
#define CTRL_LE        15
#define LED_PIN        25

#define CTRL_IRQ_N     1
#define CTRL_NMI_N     2
#define CTRL_RST_N     4
#define CTRL_BE        8
#define CTRL_RAM_R_N   16
#define CTRL_RAM_W_N   32
uint8_t ctrlValue;

#include "portdefs.h"
uint8_t ports6502[256];

#include "6502-src/ehbasic.h"
#include "6502-src/toppage.h"

static inline void waitShort() 
{
    gpio_put(LED_PIN, HIGH);
    gpio_put(LED_PIN, HIGH);
    gpio_put(LED_PIN, LOW);
}

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

static inline uint16_t getAddr() 
{
    uint16_t value;
    gpio_set_dir_in_masked(255);
    gpio_put(A_HI_EN_N, LOW);
    waitShort();
    value = (gpio_get_all() & 255) << 8;
    gpio_put(A_HI_EN_N, HIGH);      
    gpio_put(A_LO_EN_N, LOW);
    waitShort();
    value = value | (gpio_get_all() & 255);
    gpio_put(A_LO_EN_N, HIGH);      
    return value;
}

static inline uint8_t getData() 
{
    uint8_t value;
    gpio_set_dir_in_masked(255);
    gpio_put(D_EN_N, LOW);
    waitShort();
    value = gpio_get_all() & 255;
    gpio_put(D_EN_N, HIGH);  
    return value;
}

static inline void setDataAndClk(uint8_t value) 
{
    gpio_set_dir_out_masked(255);
    gpio_put_masked(255, value);
    gpio_put(D_EN_N, LOW);
    gpio_put(CLK, LOW);    
    waitShort();
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

void setup() 
{
    pinMode(PD0,       OUTPUT);
    pinMode(PD1,       OUTPUT);
    pinMode(PD2,       OUTPUT);
    pinMode(PD3,       OUTPUT);
    pinMode(PD4,       OUTPUT);
    pinMode(PD5,       OUTPUT);
    pinMode(PD6,       OUTPUT);
    pinMode(PD7,       OUTPUT);
    pinMode(RW,        INPUT);
    pinMode(SYNC,      INPUT);
    pinMode(CLK,       OUTPUT);
    pinMode(A_LO_EN_N, OUTPUT);
    pinMode(A_HI_EN_N, OUTPUT);
    pinMode(D_EN_N,    OUTPUT);
    pinMode(BANK_LE,   OUTPUT);
    pinMode(CTRL_LE,   OUTPUT);
    pinMode(LED_PIN,   OUTPUT);  
    
    digitalWrite(CLK,       HIGH);
    digitalWrite(A_LO_EN_N, HIGH);
    digitalWrite(A_HI_EN_N, HIGH);
    digitalWrite(D_EN_N,    HIGH);
    digitalWrite(BANK_LE,   LOW);
    digitalWrite(CTRL_LE,   LOW);

    ctrlValue = 255 & ~CTRL_RST_N;
    setCtrl(ctrlValue);

    Serial.begin(115200);
    while(!Serial)
    {
        //waiting for the serial channel
    }
    
    reset6502(); 
}

void loop() 
{  
    uint16_t cpuAddr;
    uint8_t cpuData;
    
    //CLK is LOW here
    cpuAddr = getAddr();  
    gpio_put(CLK, HIGH);
    
    if (gpio_get(RW) == LOW) 
    { 
        //write operation            
        if ((cpuAddr & 0xFF00) == 0xFE00)
        {
            //I/O-device write
            cpuData = getData();
            ports6502[cpuAddr & 0xFF] = cpuData;
            switch (cpuAddr)
            {
                // ******** Serial port registers ********
                case PORT_SERIAL_0_OUT:
                {
                    //serial output                        
                    Serial.write(cpuData);
                    break;
                }                
                default: 
                {
                    break;
                }
            }
        }
        else if ((cpuAddr & 0xFF00) >= 0xC000)
        {
            //do nothing, trying to write to ROM
        }
        else 
        {
            //RAM write 0-48KiB
            setBank(cpuAddr >> 14);
            ctrlValue = ctrlValue & ~CTRL_RAM_W_N;
            setCtrl(ctrlValue);
            ctrlValue = ctrlValue | CTRL_RAM_W_N;
            setCtrl(ctrlValue);      
        }        
        gpio_put(CLK, LOW);
    }
    else
    {
        //read operation          
        if ((cpuAddr & 0xFF00) == 0xFE00) 
        {   
            //I/O-device read
            switch (cpuAddr) 
            {
                case PORT_SERIAL_0_IN: 
                {
                    //serial input
                    if (Serial.available()) 
                    {
                        cpuData = Serial.read();
                    }
                    else 
                    {
                        cpuData = 0;
                    }
                    setDataAndClk(cpuData);
                    break;
                }
                case PORT_SERIAL_0_FLAGS:
                {                    
                    //serial flags
                    if (Serial.available()) 
                    {
                        cpuData = 64;
                    }
                    else 
                    {
                        cpuData = 0;
                    }
                    setDataAndClk(cpuData);
                    break;
                }
                default: 
                {
                    break;
                }
            }
        }
        else if ((cpuAddr & 0xFF00) == 0xFF00) 
        {
            //top page ROM read
            cpuData = toppage_bin[cpuAddr & 0xFF];
            setDataAndClk(cpuData);
        }
        else if (cpuAddr >= 0xC000) 
        {
            //ROM read 48-63.5KiB
            cpuData = ehbasic_bin[cpuAddr - 0xC000];
            setDataAndClk(cpuData);
        }
        else 
        {
            //RAM read 0-48KiB
            setBank(cpuAddr >> 14);
            ctrlValue = ctrlValue & ~CTRL_RAM_R_N;
            setCtrl(ctrlValue);
            ctrlValue = ctrlValue | CTRL_RAM_R_N;
            setCtrl(ctrlValue);
            gpio_put(CLK, LOW);
        }    
    }
}
