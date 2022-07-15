section bss, "zeropage os", 224, 256
{
    //only used while doing osCall
    {
        reserve word osCallAddr
        reserve byte [16] osCallData
    }

    //only used while loading
    {
        reserve byte threadNew
        reserve byte hexTemp
        reserve word loadCount
        reserve word loadTargetAddr
        reserve word loadStartAddr
    }
}
