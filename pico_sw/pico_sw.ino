#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "lowLevel.h"

#include "irqTimer.h"

#include "6502-src/ehbasic.h"
#include "6502-src/toppage.h"

#include "sid.h"

//uint16_t soundSamples[2048];
int audio_pin_slice;
//int cur_sample = 0;

void pwm_irh() 
{
    pwm_clear_irq(audio_pin_slice);
    pwm_set_gpio_level(AUDIO_PIN, sid_calc() / 64);
    //pwm_set_gpio_level(AUDIO_PIN, soundSamples[cur_sample]);    
    //cur_sample = (cur_sample + 1) & 2047;
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
    irqTimerInit();    
    frameCounter = 0;    
    frameTimer = time_us_32() + FRAME_TIME_US;
    sid_reset();
    /*
    for (int i = 0; i < 2048; i++)
    {
        soundSamples[i] = (i & 255) << 2;
    }
    */
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);    
    audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    // Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_irh);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    // 133MHz / 44100 / 256 -> 11.78
    // 133MHz / 44100 / 1024 -> 2.9452
    pwm_config_set_clkdiv(&config, 2.9452f);
    pwm_config_set_wrap(&config, 1023);
    pwm_init(audio_pin_slice, &config, true);
    pwm_set_gpio_level(AUDIO_PIN, 128);    
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
    gpio_put(LED_PIN, 1);
    // run 65C02 for one frame
    for (int frameCycles = 0; frameCycles < (CPU_FREQUENCY / FRAME_RATE); frameCycles++)
    {   //CLK is LOW
        cpuAddr = getAddr();  
        gpio_put(CLK, HIGH);
        
        if (gpio_get(RW) == LOW) 
        {   //write operation
            if ((cpuAddr & 0xFF00) == 0xFE00)
            {   //I/O-device write
                //write to ports6502-memory
                cpuData = getData();
                ports6502[cpuAddr & 0xFF] = cpuData;                
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
                    // ******** Serial port registers ********
                    case PORT_SERIAL_0_OUT:
                    {
                        //serial output                        
                        Serial.write(cpuData);
                        gpio_put(CLK, LOW);
                        break;
                    }       
                    // ******** Irq counter registers ********
                    case PORT_IRQ_TIMER_LO: //target lo
                    {
                        irqTimerWriteLo(cpuData);
                        break;
                    }
                    case PORT_IRQ_TIMER_HI: //target hi
                    {
                        irqTimerWriteHi(cpuData);
                        break;
                    }
                    case PORT_IRQ_TIMER_RESET:
                    {  
                        irqTimerReset();
                        break;
                    }
                    case PORT_IRQ_TIMER_TRIG: 
                    {
                        irqTimerReset();
                        break;
                    }
                    case PORT_IRQ_TIMER_PAUSE: 
                    {
                        irqTimerPause();
                        break;
                    }
                    case PORT_IRQ_TIMER_CONT:
                    {
                        irqTimerPause();
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
            else if ((cpuAddr & 0xFF00) >= 0xC000)
            {   //do nothing, trying to write to ROM                
                gpio_put(CLK, LOW);
            }
            else 
            {   //RAM write 0-48KiB                
                setBank(cpuAddr >> 14);
                setCtrlFast(ctrlValue & ~CTRL_RAM_W_N);
                setCtrlFast(ctrlValue | CTRL_RAM_W_N);      
                gpio_put(CLK, LOW);
            }
        }
        else
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
                    // ******** Frame counter ********
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
                    // ******** All other I/O ********
                    default: 
                    {
                        cpuData = ports6502[cpuAddr & 0xFF];
                        setDataAndClk(cpuData);
                        break;
                    }
                }
            }
            else if ((cpuAddr & 0xFF00) == 0xFF00) 
            {   //top page ROM read                
                cpuData = toppage_bin[cpuAddr & 0xFF];
                setDataAndClk(cpuData);
            }
            else if ((cpuAddr & 0xFF00) >= 0xC000) 
            {   //ROM read 48-63.5KiB                
                cpuData = ehbasic_bin[cpuAddr - 0xC000];
                setDataAndClk(cpuData);
            }
            else 
            {   //RAM read 0-48KiB                
                setBank(cpuAddr >> 14);
                setCtrlFast(ctrlValue & ~CTRL_RAM_R_N);
                setCtrlFast(ctrlValue | CTRL_RAM_R_N);
                gpio_put(CLK, LOW);
            }    
        }
    }
    
    //debug: visualize the timing of 1 frame
    //gpio_put(AUDIO_PIN, 1);
    /*
    int16_t soundPort;
    for (int sampleCount = 0; sampleCount < (SAMPLE_RATE / FRAME_RATE); sampleCount++)
    {
        soundPort = sid_calc();        
    }
    */
    
    frameCounter++;
    frameTimer += FRAME_TIME_US;
    //debug: visualize the timing of 1 frame
    gpio_put(LED_PIN, 0);
}
