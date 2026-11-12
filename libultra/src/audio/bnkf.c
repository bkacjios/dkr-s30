/*====================================================================
 * bnkf.c
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics,
 * Inc.; the contents of this file may not be disclosed to third
 * parties, copied or duplicated in any form, in whole or in part,
 * without the prior written permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to
 * restrictions as set forth in subdivision (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS
 * 252.227-7013, and/or in similar or successor clauses in the FAR,
 * DOD or NASA FAR Supplement. Unpublished - rights reserved under the
 * Copyright Laws of the United States.
 *====================================================================*/

#include <libaudio.h>
#include <os_internal.h>
#include <ultraerror.h>

/*
 * ### when the file format settles down a little, I'll remove these
 * ### for efficiency.
 */
static  void _bnkfPatchBank(ALBank *bank, s32 offset, s32 table);
static  void _bnkfPatchInst(ALInstrument *i, s32 offset, s32 table);
static  void _bnkfPatchSound(ALSound *s, s32 offset, s32 table);
static  void _bnkfPatchWaveTable(ALWaveTable *w, s32 offset, s32 table);

#ifdef TARGET_S30
/*
 * The PocketGo S30: the bank comes from the ROM big-endian; each struct's
 * fields are put in this CPU's byte order as the patching first reaches it.
 * Banks, instruments, sounds and wavetables have a flags byte for that;
 * envelopes, books and loops (shared between sounds) are remembered here.
 */
#include "s30_swap.h"
#include <stdlib.h>

static void **sSwapped;
static s32 sNumSwapped, sMaxSwapped;

/* TRUE the first time p is seen (both bank files' structures, a few thousand) */
static s32 swap_once(void *p) {
    s32 i;

    for (i = 0; i < sNumSwapped; i++) {
        if (sSwapped[i] == p) {
            return FALSE;
        }
    }
    if (sNumSwapped == sMaxSwapped) {
        sMaxSwapped = sMaxSwapped ? sMaxSwapped * 2 : 1024;
        sSwapped = realloc(sSwapped, sMaxSwapped * sizeof(*sSwapped));
    }
    sSwapped[sNumSwapped++] = p;
    return TRUE;
}
#endif

void alSeqFileNew(ALSeqFile *file, u8 *base)
{
    s32 offset = (s32) base;
    s32 i;
    
#ifdef TARGET_S30
    s30_swap32(file->seqArray, file->seqCount * 2); // the caller swapped revision and seqCount
#endif
    /*
     * patch the file so that offsets are pointers
     */
    for (i = 0; i < file->seqCount; i++) {
        file->seqArray[i].offset = (u8 *)((u8 *)file->seqArray[i].offset + offset);
    }
}

void alBnkfNew(ALBankFile *file, u8 *table)
{
    s32 offset = (s32) file;
    s32 woffset = (s32) table;
    
    s32 i;
    
#ifdef TARGET_S30
    s30_swap16(&file->revision, 2);
    s30_swap32(file->bankArray, file->bankCount);
#endif
    /*
     * check the file format revision in debug libraries
     */
    ALFailIf(file->revision != AL_BANK_VERSION, ERR_ALBNKFNEW);
    
    /*
     * patch the file so that offsets are pointers
     */
    for (i = 0; i < file->bankCount; i++) {
        file->bankArray[i] = (ALBank *)((u8 *)file->bankArray[i] + offset);
        if(file->bankArray[i])
            _bnkfPatchBank(file->bankArray[i], offset, woffset);
    }
}

void _bnkfPatchBank(ALBank *bank, s32 offset, s32 table) 
{
    s32 i;
    
    if (bank->flags)
        return;

    bank->flags = 1;
#ifdef TARGET_S30
    s30_swap16(&bank->instCount, 1);
    s30_swap32(&bank->sampleRate, 2);
    s30_swap32(bank->instArray, bank->instCount);
#endif

    if (bank->percussion) {
        bank->percussion = (ALInstrument *)((u8 *)bank->percussion + offset);
        _bnkfPatchInst(bank->percussion, offset, table);
    }
    
    for (i = 0; i < bank->instCount; i++) {
        bank->instArray[i] = (ALInstrument *)((u8 *)bank->instArray[i] +
                                              offset);
        if(bank->instArray[i])
            _bnkfPatchInst(bank->instArray[i], offset, table);
    }
}

void _bnkfPatchInst(ALInstrument *inst, s32 offset, s32 table)
{
    s32 i;

    if (inst->flags)
        return;

    inst->flags = 1;
#ifdef TARGET_S30
    s30_swap16(&inst->bendRange, 2);
    s30_swap32(inst->soundArray, inst->soundCount);
#endif
    
    for (i = 0; i < inst->soundCount; i++) {
        inst->soundArray[i] = (ALSound *)((u8 *)inst->soundArray[i] +
                                          offset);
        _bnkfPatchSound(inst->soundArray[i], offset, table);

    }
}

void _bnkfPatchSound(ALSound *s, s32 offset, s32 table)
{
    if (s->flags)
        return;

    s->flags = 1;
#ifdef TARGET_S30
    s30_swap32(s, 3); // envelope, keyMap, wavetable
#endif
    
    s->envelope  = (ALEnvelope *)((u8 *)s->envelope + offset);
    s->keyMap    = (ALKeyMap *)((u8 *)s->keyMap + offset);
#ifdef TARGET_S30
    if (swap_once(s->envelope)) {
        s30_swap32(s->envelope, 3); // the times; the volumes are bytes
    }
#endif

    s->wavetable = (ALWaveTable *)((u8 *)s->wavetable + offset);
    _bnkfPatchWaveTable(s->wavetable, offset, table);
}

void _bnkfPatchWaveTable(ALWaveTable *w, s32 offset, s32 table)
{
    if (w->flags)
        return;

    w->flags = 1;
#ifdef TARGET_S30
    s30_swap32(w, 2); // base, len
    s30_swap32(&w->waveInfo, w->type == AL_ADPCM_WAVE ? 2 : 1);
#endif
    
    w->base += table;

    /* sct 2/14/96 - patch wavetable loop info based on type. */
    if (w->type == AL_ADPCM_WAVE)
    {
	w->waveInfo.adpcmWave.book  = (ALADPCMBook *)((u8 *)w->waveInfo.adpcmWave.book + offset);
	if (w->waveInfo.adpcmWave.loop)
	    w->waveInfo.adpcmWave.loop = (ALADPCMloop *)((u8 *)w->waveInfo.adpcmWave.loop + offset);
#ifdef TARGET_S30
	if (swap_once(w->waveInfo.adpcmWave.book)) {
	    ALADPCMBook *book = w->waveInfo.adpcmWave.book;

	    s30_swap32(book, 2);
	    s30_swap16(book->book, book->order * book->npredictors * 8);
	}
	if (w->waveInfo.adpcmWave.loop && swap_once(w->waveInfo.adpcmWave.loop)) {
	    s30_swap32(w->waveInfo.adpcmWave.loop, 3);
	    s30_swap16(w->waveInfo.adpcmWave.loop->state, 16);
	}
#endif
    }
    else if (w->type == AL_RAW16_WAVE)
    {
	if (w->waveInfo.rawWave.loop)
	    w->waveInfo.rawWave.loop = (ALRawLoop *)((u8 *)w->waveInfo.rawWave.loop + offset);
#ifdef TARGET_S30
	if (w->waveInfo.rawWave.loop && swap_once(w->waveInfo.rawWave.loop)) {
	    s30_swap32(w->waveInfo.rawWave.loop, 3);
	}
#endif
    }	
}
