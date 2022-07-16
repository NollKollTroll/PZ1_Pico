#ifndef IRQTIMER_H
#define IRQTIMER_H

int32_t irqTimerTicks;
uint16_t irqTimerTarget;

enum irqTimerState_e 
{
    RUNNING,
    TRIGGERED,
    PAUSED
};
irqTimerState_e irqTimerState;

static inline void irqTimerInit(void)
{
    irqTimerState = PAUSED;
    irqTimerTicks = 0;
    irqTimerTarget = 0;
}

static inline void irqTimerTick(void)
{
    if (irqTimerState == RUNNING)
    {
        irqTimerTicks++;
        if (irqTimerTicks >= irqTimerTarget)
        {
            irqTimerTicks -= irqTimerTarget;
            irqTimerState = TRIGGERED;
            ctrlValue = ctrlValue & ~CTRL_IRQ_N;
            setCtrl(ctrlValue);
        }
    }
    else if (irqTimerState == TRIGGERED)
    {
        irqTimerTicks++;
    }    
}

static inline void irqTimerWriteLo(uint8_t cpuData)
{   // Do nothing, data already written to io-memory
    // portMem6502[PORT_IRQ_TIMER_LO & 0xFF] = cpuData;
}

static inline void irqTimerWriteHi(uint8_t cpuData)
{   // Trigger the use of a new 16-bit timer value by writing the high byte and clear IRQ
    // portMem6502[PORT_IRQ_TIMER_HI] = cpuData;
    irqTimerTarget = (cpuData << 8) + portMem6502[PORT_IRQ_TIMER_LO & 0xFF];
    irqTimerState = RUNNING;                    
    ctrlValue = ctrlValue | CTRL_IRQ_N;
    setCtrl(ctrlValue);
}

static inline void irqTimerReset(void)
{   // Reset the timer and clear IRQ 
    irqTimerTicks = 0;
    irqTimerState = RUNNING;
    ctrlValue = ctrlValue | CTRL_IRQ_N;
    setCtrl(ctrlValue);
}

static inline void irqTimerTrig(void)
{   // Triggers the timer IRQ
    irqTimerTicks = irqTimerTarget;
}

static inline void irqTimerPause(void)
{   // Pause the timer
    irqTimerState = PAUSED;
}

static inline void irqTimerCont(void)
{   // Set the timer running and clear IRQ
    irqTimerState = RUNNING;
    ctrlValue = ctrlValue | CTRL_IRQ_N;
    setCtrl(ctrlValue);
}

#endif //IRQTIMER_H
