#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include <SDFS.h>

#include "lowLevel.h"
#include "irqTimer.h"
#include "sid.h"
#include "pwmAudio.h" 
#include "fileSystem.h"

#include "6502-src/ehbasic.h"
#include "6502-src/toppage.h"

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
    
    pinMode(DEBUG_PIN, OUTPUT);
    
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

    Serial.begin(115200);
    while(!Serial)
    {   //waiting for the serial channel
    }
    
    reset6502();
    irqTimerInit();    
    sid_reset();
    pwmInit();
    fsInit();    
    
    frameCounter = 0;    
    frameTimer = time_us_32() + FRAME_TIME_US;    
}

void loop() 
{  
    uint16_t cpuAddr;
    uint8_t cpuData;

    //time with next frame start
    while (time_us_32() < frameTimer)
    {
    }
    
    //debug: visualize the timing of 1 frame
    gpio_put(DEBUG_PIN, 1);
    
    // run 65C02 for one frame
    for (int frameCycles = 0; frameCycles < (CPU_FREQUENCY / FRAME_RATE); frameCycles++)
    {   
        //CLK is LOW here
        cpuAddr = getAddr();  
        gpio_put(CLK, HIGH);
        
        if (gpio_get(RW) == HIGH) 
        {   //read operation          
            if ((cpuAddr & 0xFF00) == 0xFE00) 
            {   //I/O-device read                
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
                    {   //serial input
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
                    {   //serial flags
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
            else if ((cpuAddr & 0xFF00) == 0xFF00) 
            {   //top page ROM read, regardless of BANK_3                
                cpuData = toppage_bin[cpuAddr & 0xFF];
                setDataAndClk(cpuData);
            }
            else 
            {
                uint8_t activeBank = portMem6502[(cpuAddr >> 14) + (PORT_BANK_0 & 0xFF)];
                if (activeBank <= 127)
                {
                    setBank(activeBank);
                    setCtrlFast(ctrlValue & ~CTRL_RAM_R_N);
                    setCtrlFast(ctrlValue | CTRL_RAM_R_N);
                    gpio_put(CLK, LOW);
                }
                else
                {
                    cpuData = ehbasic_bin[cpuAddr & 0x3FFF];
                    setDataAndClk(cpuData);
                }
            }    
        }
        else
        {   //write operation
            if ((cpuAddr & 0xFF00) == 0xFE00)
            {   //I/O-device write
                //write to portMem6502-memory
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
                        //serial output                        
                        Serial.write(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }       
                    // ******** Frame counter registers ********
                        // Same as default
                    // ******** File system registers ********
                    case PORT_FILE_CMD: {
                        fsCmdWrite(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_FILE_DATA: {
                        fsDataWrite(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    // ******** Irq timer registers ********
                    case PORT_IRQ_TIMER_LO: //target lo
                    {
                        irqTimerWriteLo(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }
                    case PORT_IRQ_TIMER_HI: //target hi
                    {
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
                        irqTimerReset();
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
                        irqTimerPause();
                        gpio_put(CLK, LOW);
                        break;
                    }
                    // ******** Default ********
                    default: 
                    {
                        gpio_put(CLK, LOW);
                        break;
                    }
                }
            }
            else 
            {   
                uint8_t activeBank = portMem6502[(cpuAddr >> 14) + (PORT_BANK_0 & 0xFF)];
                if (activeBank <= 127)
                {
                    setBank(activeBank);
                    setCtrlFast(ctrlValue & ~CTRL_RAM_W_N);
                    setCtrlFast(ctrlValue | CTRL_RAM_W_N);      
                    gpio_put(CLK, LOW);
                }
                else
                {   //Writing to ROM? Nope!
                    gpio_put(CLK, LOW);
                }
            }
        }
    }
    
    frameCounter++;
    frameTimer += FRAME_TIME_US;
    //debug: visualize the timing of 1 frame
    gpio_put(DEBUG_PIN, 0);
}
