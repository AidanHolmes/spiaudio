/*****************************************************************************/
/*                                                                           */
/*                         AmigaAMP Plugin headers v1.4                      */
/*                                                                           */
/*                      Written August 1999 by Thomas Wenzel                 */
/*              Added essentials to a header file by Aidan Holmes 2026       */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/* This is the basic header structures for every AmigaAMP plugin.            */
/* To make it as easy as possible all plugins are normal AmigaDOS            */
/* executables which can be launched and stopped at any time.                */
/*                                                                           */
/* This sourcecode is free for non-commercial use only!                      */
/*                                                                           */
/*****************************************************************************/

#ifndef __AMGPLUGIN_H
#define __AMGPLUGIN_H

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include <dos/dostags.h>
#include <graphics/gfxbase.h>

struct TrackInfo {
	UBYTE	private[4];  // DO NOT USE!
	ULONG	StreamSize;
	ULONG	HeaderSize;
	ULONG	Length;
	ULONG	Frequency;
	UBYTE	Layer;
	UBYTE	Version;
	UBYTE	CRCs;
	UBYTE	Emphasis;
	UBYTE	Private;
	UBYTE	Copyright;
	UBYTE	Original;
	UBYTE	Mode;
	UWORD	Bitrate;
	UWORD	Channels;

	char	*TrackInfoText;  // same text as displayed in AmigaAMPs TrackInfo line
	char	*ID3title;
	char	*ID3artist;
	char	*ID3album;
	char	*ID3year;
	char	*ID3comment;
	char	*ID3genre;
	ULONG	Position;
	UWORD	TrackNumber;
	UWORD	DriveMode;       // 0=STOP, 1=PLAY, 3=PAUSE
	UWORD	Hardware;        // 0=none, 1=MPEGA, 2=PowerUP, 3=External, 4=MPEGit
};

struct PluginMessage {
	struct Message msg;
	ULONG          PluginMask;
	struct Process *PluginTask;
	UWORD          **SpecRawL;
	UWORD          **SpecRawR;
	WORD           Accepted;     /* v1.3: Don't use BOOL any more because  */
	WORD           reserved0;    /* it causes alignment problems!          */

	/* All data beyond this point is new for v1.2.                         */
	/* AmigaAMP v2.5 and up will detect this new data and act accordingly. */
	/* Older versions of AmigaAMP will simply ignore it.                   */

	ULONG            InfoMask;
	struct TrackInfo **tinfo;
	struct MsgPort   *PluginWP;  /* v1.4 (used to be reserved)             */
                               /* If not used set to NULL to avoid       */
                               /* unnessecary overhead!                  */

	/* All data beyond this point is new for v1.3.                         */
	/* AmigaAMP v2.7 and up will detect this new data and act accordingly. */
	/* Older versions of AmigaAMP will simply ignore it.                   */

	WORD             **SampleRaw;
};

struct WindowMessage {
	struct Message msg;
	struct Window *AmpWin; // Pointer to main window
	struct Window *EqWin;  // Pointer to equalizer window
	struct Window *PlWin;  // Pointer to playlist window
	ULONG Flags;           // Not used yet
};

#endif