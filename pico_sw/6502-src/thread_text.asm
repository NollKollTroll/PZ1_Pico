include "portdefs.asm"
include "zpos.asm"
include "oscalls.asm"

section bss, "ZeroPage", 0, 224
{
    reserve byte threadCharZP
}

section code, "threadStack", 256, 512
{
}

const WAIT_LOOP = 65535

section code, "threadCode", 512
{
THREAD_START:
    lda   #OS.THREAD_SELF_GET
    jsr   osCall
    lda   osCallData[0]
    clc
    adc     #'A'
    sta     threadCharZP
    {
        lda     threadCharZP
        sta     PORT_SERIAL_0_OUT
        ldx     #>WAIT_LOOP
        {
            ldy     #<WAIT_LOOP
            {
                dey
                bne     @loop
            }
            dex
            bne     @loop
        }
        jmp     @loop
    }
}
