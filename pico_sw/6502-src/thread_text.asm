/*
Written in 2022 by Adam Klotblixt (adam.klotblixt@gmail.com)

To the extent possible under law, the author have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide.
This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see
<http://creativecommons.org/publicdomain/zero/1.0/>.
*/

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
