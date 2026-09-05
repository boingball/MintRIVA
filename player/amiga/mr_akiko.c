/* MintVID - safe CD32 Akiko presence check. */
#include "mr_akiko.h"

#include <exec/types.h>

#define AKIKO_ID_WORD_ADDR 0x00B80002UL
#define AKIKO_ID_WORD      0xCAFEU

int mr_akiko_available(void)
{
    /* Akiko exposes the read-only ID C0CACAFE at $B80000. The CD32 Kickstart
     * probe only checks the CAFE word at +2, which also avoids touching the
     * writable C2P port at $B80038 on ordinary AGA machines. */
    volatile const UWORD *id =
        (volatile const UWORD *)AKIKO_ID_WORD_ADDR;
    return *id == AKIKO_ID_WORD;
}
