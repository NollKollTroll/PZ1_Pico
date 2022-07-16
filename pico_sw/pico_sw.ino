/*
Compiled with -O3 optimization,
timings are tested to work at 250MHz and 133MHz
*/
//#define PICO_250_MHZ

#define SER_DEBUG
//SerialPIO Ser6502(21, 22, 32);
#define Ser6502 Serial
#define SerDebug Serial    

#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#include <SD.h>

#include "lowLevel.h"
#include "irqTimer.h"
#include "sid.h"
#include "pwmAudio.h" 
#include "fileSystem.h"

#include "6502-src/ehbasic.h"
#include "6502-src/scheduler.h"

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
    
    pinMode(SD_CS_N,      OUTPUT);  
    digitalWrite(SD_CS_N, HIGH);
    
    digitalWrite(CLK,       HIGH);
    digitalWrite(A_LO_EN_N, HIGH);
    digitalWrite(A_HI_EN_N, HIGH);
    digitalWrite(D_EN_N,    HIGH);
    digitalWrite(BANK_LE,   LOW);
    digitalWrite(CTRL_LE,   LOW);

    ctrlValue = 255 & ~CTRL_RST_N;
    setCtrl(ctrlValue);

    Ser6502.begin(115200);
    SerDebug.begin(115200);
    while(!Serial)
    {
    }

    reset6502();
    irqTimerInit();    
    sid_reset(SAMPLE_RATE);
    pwmInit(SAMPLE_RATE);
    fsInit();    
    
    frameCounter = 0;    
    frameTimer = time_us_32() + FRAME_TIME_US;    
}

void loop() 
{  
    uint16_t cpuAddr;
    uint8_t cpuData;

    //sync with next frame start
    while (time_us_32() < frameTimer)
    {
    }
    
    //run 65C02 for one frame
    for (int frameCycles = 0; frameCycles < (CPU_FREQUENCY / FRAME_RATE); frameCycles++)
    {   
        //CLK is LOW here
        cpuAddr = getAddr();  
        gpio_put(CLK, HIGH);
        
        if (gpio_get(RW) == HIGH) 
        {   //read operation          
            if (cpuAddr <= 0xFDFF) 
            {   //banked memory 0x0000-0xFDFF
                uint8_t activeBlock = portMem6502[(cpuAddr >> 14) + (PORT_BANK_0 & 0xFF)];
                if (activeBlock <= 127)
                {   //blocks 0-127 are RAM
                    setBank(activeBlock);
                    setCtrlFast(ctrlValue & ~CTRL_RAM_R_N);
                    wait1();
                    #ifdef PICO_250_MHZ
                    wait1();
                    wait1();
                    wait1();
                    #endif
                    gpio_put(CLK, LOW);
                    setCtrlFast(ctrlValue);
                }
                else if (activeBlock == 254)
                {   //block 254 is EhBasic in ROM
                    cpuData = ehbasic_bin[cpuAddr & 0x3FFF];
                    setDataAndClk(cpuData);
                }
                else
                {   //block 255 is scheduler/reset ROM
                    //blocks 128-253 are unused, mirrored scheduler ROM
                    cpuData = scheduler_bin[cpuAddr & 0x3FFF];
                    setDataAndClk(cpuData);
                }
            }
            else if (cpuAddr <= 0xFEFF) 
            {   //I/O-device read at 0xFE00-0xFEFF               
                switch (cpuAddr) 
                {   //take action depending on device
                    // ******** SID registers ********                
                    case PORT_SID_00:
                    case PORT_SID_01:
                    case PORT_SID_02:
                    case PORT_SID_03:
                    case PORT_SID_04:
                    case PORT_SID_05:
                    case PORT_SID_06:
                    case PORT_SID_07:
                    case PORT_SID_08:
                    case PORT_SID_09:
                    case PORT_SID_0A:
                    case PORT_SID_0B:
                    case PORT_SID_0C:
                    case PORT_SID_0D:
                    case PORT_SID_0E:
                    case PORT_SID_0F:
                    case PORT_SID_10:
                    case PORT_SID_11:
                    case PORT_SID_12:
                    case PORT_SID_13:
                    case PORT_SID_14:
                    case PORT_SID_15:
                    case PORT_SID_16:
                    case PORT_SID_17:
                    case PORT_SID_18:
                    case PORT_SID_19:
                    case PORT_SID_1A:
                    case PORT_SID_1B:
                    case PORT_SID_1C:
                    {
                        cpuData = sid_read(cpuAddr & 0x1F);
                        setDataAndClk(cpuData);
                        break;
                    }
                    // ******** Memory bank registers ********
                        // Same as default
                    // ******** Serial port registers ********
                    case PORT_SERIAL_0_IN: 
                    {
                        if (Ser6502.available()) 
                        {
                            cpuData = Ser6502.read();
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
                        if (Ser6502.available()) 
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
                    // ******** Frame counter registers********
                    case PORT_FRAME_COUNTER_LO: 
                    {
                        cpuData = frameCounter & 255;
                        setDataAndClk(cpuData);
                        break;
                    }
                    case PORT_FRAME_COUNTER_HI: 
                    {
                        cpuData = frameCounter >> 8;
                        setDataAndClk(cpuData);
                        break;
                    }
                    // ******** File system registers ********
                    case PORT_FILE_DATA: 
                    {
                        cpuData = fsDataRead();
                        setDataAndClk(cpuData);
                        break;
                    }
                    case PORT_FILE_STATUS: 
                    {
                        cpuData = (uint8_t)fileStatus;
                        setDataAndClk(cpuData);
                        break;
                    }
                    // ******** Irq timer registers ********
                        // Same as default
                    // ******** All other I/O ********
                    default: 
                    {
                        cpuData = portMem6502[cpuAddr & 0xFF];
                        setDataAndClk(cpuData);
                        break;
                    }
                }
            }
            else 
            {   //top page ROM read at 0xFF00-0xFFFF, regardless of BANK_3                
                cpuData = scheduler_bin[0x3F00 + (cpuAddr & 0xFF)];
                setDataAndClk(cpuData);
            }
        }
        else
        {   //write operation
            if (cpuAddr <= 0xFDFF)
            {   //banked memory 0x0000-0xFDFF
                uint8_t activeBank = portMem6502[(cpuAddr >> 14) + (PORT_BANK_0 & 0xFF)];                
                if (activeBank <= 127)
                {   //banks 0-127 are RAM
                    setBank(activeBank);
                    setCtrlFast(ctrlValue & ~CTRL_RAM_W_N);
                    setCtrlFast(ctrlValue);
                    gpio_put(CLK, LOW);
                }
                else
                {   //banks 128-255 are ROM
                    //Writing to ROM? Nope.
                    gpio_put(CLK, LOW);
                }
            }
            else if (cpuAddr <= 0xFEFF)
            {   //I/O-device write at 0xFE00-0xFEFF
                cpuData = getData();
                portMem6502[cpuAddr & 0xFF] = cpuData;                
                switch (cpuAddr)
                {   //take action depending on device
                    //******** SID registers ********
                    case PORT_SID_00: 
                    case PORT_SID_01: 
                    case PORT_SID_02: 
                    case PORT_SID_03: 
                    case PORT_SID_04: 
                    case PORT_SID_05: 
                    case PORT_SID_06: 
                    case PORT_SID_07: 
                    case PORT_SID_08: 
                    case PORT_SID_09: 
                    case PORT_SID_0A: 
                    case PORT_SID_0B: 
                    case PORT_SID_0C: 
                    case PORT_SID_0D: 
                    case PORT_SID_0E: 
                    case PORT_SID_0F: 
                    case PORT_SID_10: 
                    case PORT_SID_11: 
                    case PORT_SID_12: 
                    case PORT_SID_13: 
                    case PORT_SID_14: 
                    case PORT_SID_15: 
                    case PORT_SID_16: 
                    case PORT_SID_17: 
                    case PORT_SID_18: 
                    case PORT_SID_19: 
                    case PORT_SID_1A: 
                    case PORT_SID_1B: 
                    case PORT_SID_1C:
                    {
                        sid_write(cpuAddr & 0x1F, cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    // ******** Memory bank registers ********
                        // Same as default
                    // ******** Serial port registers ********
                    case PORT_SERIAL_0_OUT:
                    {
                        Ser6502.write(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }       
                    // ******** Frame counter registers ********
                        // Same as default
                    // ******** File system registers ********
                    case PORT_FILE_CMD:
                    {
                        fsCmdWrite(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_FILE_DATA:
                    {
                        fsDataWrite(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    // ******** Irq timer registers ********
                    case PORT_IRQ_TIMER_LO:
                    {   //target lo
                        irqTimerWriteLo(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_HI:
                    {   //target hi
                        irqTimerWriteHi(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_RESET:
                    {  
                        irqTimerReset();
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_TRIG: 
                    {
                        irqTimerTrig();
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_PAUSE: 
                    {
                        irqTimerPause();
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_CONT:
                    {
                        irqTimerCont();
                        gpio_put(CLK, LOW);
                        break;
                    }
                    // ******** Default ********
                    default: 
                    {   //nothing more to do, already written cpuData to portMem6502
                        gpio_put(CLK, LOW);
                        break;
                    }
                }
            }
            else 
            {   //top page ROM write at 0xFF00-0xFFFF, regardless of BANK_3                
                //Writing to ROM? Nope.
                gpio_put(CLK, LOW);
            }
        }
        //update the timer every CPU-cycle
        irqTimerTick();
    }
    
    frameCounter++;
    
    if (time_us_32() >= (frameTimer + FRAME_TIME_US))
    {   //frame took too long, skip an extra frame to catch up        
        #ifdef SER_DEBUG        
        SerDebug.print("\t\t\t!!! ");
        SerDebug.println(time_us_32() - frameTimer);
        #endif
        frameTimer += (FRAME_TIME_US * 2);
    }
    #ifdef SER_DEBUG        
    else if(frameCounter % 50 == 0)
    {   //every 10th frame, print how many us the last frame took
        SerDebug.print("\t\t ");
        SerDebug.println(time_us_32() - frameTimer);
    }
    #endif
    frameTimer += FRAME_TIME_US;
}
