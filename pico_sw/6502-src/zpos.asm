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
