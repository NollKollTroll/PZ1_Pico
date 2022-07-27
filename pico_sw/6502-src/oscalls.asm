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

const osCall = $ff00

enum OS
{
    THREAD_LOAD,
    THREAD_GET_SLOT_TIME,
    THREAD_SET_SLOT_TIME,
    THREAD_KILL,
    THREAD_GET_BLOCKS,
    THREAD_STATE_GET,
    THREAD_SELF_GET
}
