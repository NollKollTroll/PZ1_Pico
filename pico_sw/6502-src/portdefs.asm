const PORT_SID_00           = $FE00
const PORT_SID_01           = $FE01
const PORT_SID_02           = $FE02
const PORT_SID_03           = $FE03
const PORT_SID_04           = $FE04
const PORT_SID_05           = $FE05
const PORT_SID_06           = $FE06
const PORT_SID_07           = $FE07
const PORT_SID_08           = $FE08
const PORT_SID_09           = $FE09
const PORT_SID_0A           = $FE0A
const PORT_SID_0B           = $FE0B
const PORT_SID_0C           = $FE0C
const PORT_SID_0D           = $FE0D
const PORT_SID_0E           = $FE0E
const PORT_SID_0F           = $FE0F
const PORT_SID_10           = $FE10
const PORT_SID_11           = $FE11
const PORT_SID_12           = $FE12
const PORT_SID_13           = $FE13
const PORT_SID_14           = $FE14
const PORT_SID_15           = $FE15
const PORT_SID_16           = $FE16
const PORT_SID_17           = $FE17
const PORT_SID_18           = $FE18
const PORT_SID_19           = $FE19
const PORT_SID_1A           = $FE1A
const PORT_SID_1B           = $FE1B
const PORT_SID_1C           = $FE1C
const PORT_SAMPLE_VOL       = $FE1E
const PORT_SAMPLE_DATA      = $FE1F

const PORT_BANK_0           = $FE20
const PORT_BANK_1           = $FE21
const PORT_BANK_2           = $FE22
const PORT_BANK_3           = $FE23

const PORT_SERIAL_0_FLAGS   = $FE30 //(bit 7: out buffer full, not yet implemented), bit 6: in-data available
const PORT_SERIAL_0_IN      = $FE31
const PORT_SERIAL_0_OUT     = $FE32

const PORT_FRAME_COUNTER_LO = $FE40
const PORT_FRAME_COUNTER_HI = $FE41

const PORT_FILE_CMD         = $FE60
const PORT_FILE_DATA        = $FE61
const PORT_FILE_STATUS      = $FE62

enum FILE_CMD {
    OPEN_R,
    OPEN_W,
    CLOSE,
    READ,
    WRITE,
    DIR,
    DIR_NEXT
}

enum FILE_STATE {
    OPEN_R,
    OPEN_W,
    OPENED_R,
    OPENED_W,
    CLOSED,
    DIR
}

enum FILE_STATUS {
    OK,
    NOK
}

const PORT_IRQ_TIMER_LO     = $FE80
const PORT_IRQ_TIMER_HI     = $FE81
const PORT_IRQ_TIMER_RESET  = $FE82
const PORT_IRQ_TIMER_TRIG   = $FE83
const PORT_IRQ_TIMER_PAUSE  = $FE84
const PORT_IRQ_TIMER_CONT   = $FE85
