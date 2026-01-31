//   Copyright 2026 Aidan Holmes
//   SPIder Audio Visualiser - Standalone and Amiga AMP MHI VU response and info

#include <proto/timer.h>
#include <devices/timer.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <exec/exec.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <stdio.h>
#include <proto/mhi.h>
#include <string.h>
#include "app.h"
#include "gfx.h"
#include "listgad.h"
#include "stringgad.h"
#include "txtgad.h"
#include "vs1053.h"
#include "ampplugin.h"
#include <libraries/mhi.h>
#include "img/Spider.xbm"
#ifdef __VBCC__
#include <inline/mhi_protos.h>
#endif

#define MAX_DB 96

#define MHIBase appData->mhibase

struct Library *IntuitionBase = NULL;
struct Library *GfxBase = NULL ;

// SAS/C using compiler instruction to stop CTRL-C handling. These conflict so commented out
//int  __regargs _CXBRK(void) { return 0; } /* Disable SAS/C Ctrl-C handling */
//void __regargs __chkabort(void) { ; } /* really */

#ifdef __VBCC__
void __chkabort(void) { ; }
#endif

struct TextAttr topaz8 = {
   (STRPTR)"topaz.font", 8, 0, 1
};

struct TextAttr lcd = {
   (STRPTR)"topaz.font", 8, FSF_BOLD, 1
};

static void destroyAppData(App *myApp);
static void closeAmigaAmp(App *app);
static void doInfo(struct MHIVisAppData *appData);
static void enableLCDIndicator(struct LCDIndicator *ico, BOOL enable);

struct LCDIndicator{
	char *text;
	BOOL enabled;
	UWORD x;
	UWORD y;
	UWORD width;
	UWORD height;
	BOOL changed;
	BOOL rjustify;
};

struct LCDIndicator mp3ico = {"MP3", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator aacico = {"AAC", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator oggico = {"OGG", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator flcico = {"FLC", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator wmaico = {"WMA", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator wavico = {"WAV", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator vs53ico = {"VS1053", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator vs63ico = {"VS1063", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator waico = {"AMP", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator mhiico = {"MHI", FALSE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator bpsico = {"bps", TRUE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator freqico = {"Hz", TRUE, 0,0,0,0,TRUE, FALSE};
struct LCDIndicator bpstxtico = {"", TRUE, 0,0,0,0,TRUE, TRUE};
struct LCDIndicator freqtxtico = {"", TRUE, 0,0,0,0,TRUE, TRUE};

struct MHIMinHandle{
	UWORD version;
	struct MsgPort *port;
};

struct PixelImage{
	UBYTE *pixelArray;
	UWORD width;
	UWORD height;
	UWORD stride;
};

struct MHIVisAppData{
	struct  Library *mhibase ;
	APTR	mhihandle;
	BYTE 	pluginSig;
	BYTE 	infoSig;
	BYTE 	mhisig;
	BOOL 	initialisedAmigaAmp;
	BOOL	initialisedMHI;
	struct 	PluginMessage *PluginMsg;
	struct 	IOStdReq *mhiMsg ;
	UWORD	*SpecRawL;
	UWORD	*SpecRawR;
	WORD  	*SampleRaw;            // v1.3
	struct TrackInfo 		*tinfo;
	struct MsgPort	 		*mhiReplyPort; 		// MHI comms port for direct messages to driver
	struct MsgPort	 		*mhiDrvPort;			// Driver port for MHI
	struct MsgPort   		*PluginMP;				// Public port exposed by AmigaAmp
	struct MsgPort   		*pluginReplyPort;		// For any replies regarding plugin data
	struct MsgPort   		*windowReplyPort;		// For any AmigaAmp window updates
	struct timeval 			lastVis;
	struct timeval 			lastAmpCheck;
	struct VSParameterData 	param;
	struct VSMediaInfo		info;
	LONG bgPen;
	LONG vuPen;
	LONG vuPenFade;
	struct TextFont *lcdFont;
	struct RastPort *rpVU;
	UWORD windowWidth;
	UWORD windowHeight;
	UWORD vuWidth;
	UWORD vuHeight;
	UWORD vuLeftYStart;
	UWORD vuLeftXStart;
	UWORD vuRightYStart;
	UWORD vuRightXStart;
	UBYTE *vuPixels ;
	struct PixelImage *bgImage;
	BOOL mhiMsgInFlight;
	UBYTE dbLeft;
	UBYTE dbRight;
	UBYTE lastMHIStatus;
	BOOL refreshMHIData;
	char frequencyText[10];
	char rateText[10];
	UBYTE compressor[MAX_DB+1];
};

void StrNCpy(char *to, char *from, UWORD maxbuf)
{
	size_t len = 0;
	if (to && from){
		len = strlen(from);
		if (len > maxbuf - 1){
			len = maxbuf - 1;
		}
		memcpy(to, from, len);
		to[len] = '\0';
	}
}

static struct PixelImage* xbmToPixelImage(UBYTE *xbm, UWORD width, UWORD height, UBYTE colour0, UBYTE colour1)
{
	struct PixelImage *pixImg;
	UBYTE *newPixels = NULL, *row = NULL, *p = NULL, *q = NULL ;
	UWORD xbmstride = (width+7) / 8 ;
	UWORD x=0, y=0;
	
	if (!(pixImg = AllocVec(sizeof(struct PixelImage), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}
	pixImg->width = width;
	pixImg->height = height;
	pixImg->stride = (((width+15)>>4)<<4);
	if (!(newPixels=AllocVec((pixImg->stride * height), MEMF_ANY | MEMF_CLEAR))){
		// Failed to allocate memory
		FreeVec(pixImg);
		return NULL;
	}
	pixImg->pixelArray = newPixels;
	for (y=0,row=newPixels; y < height; y++, row+=pixImg->stride){
		p=row;
		q = xbm + (xbmstride*y);
		for (x=0;x<width;x++){
			if (q[x>>3] & (1 << (x&0x07))){
				*p++ = colour1;
			}else{
				*p++ = colour0;
			}
		}
	}
	return pixImg;
}

static void freePixelImage(struct PixelImage *pi)
{
	if (pi){
		if (pi->pixelArray){
			FreeVec(pi->pixelArray);
			pi->pixelArray = NULL;
		}
		FreeVec(pi);
	}
}

void ShowRequester(char *Text, char *Button) {
	struct EasyStruct Req;

  Req.es_Title        = "SPIder Vis Plugin";
  Req.es_TextFormat   = (UBYTE*)Text;
  Req.es_GadgetFormat = (UBYTE*)Button;
	EasyRequestArgs(NULL, &Req, NULL, NULL);
}

static BOOL existsAmigaAmp(App *app)
{
	struct MsgPort *port = NULL ;
	struct MHIVisAppData *appData = (struct MHIVisAppData *)app->appContext;
	
	Forbid();
	port = FindPort("AmigaAMP plugin port");
	Permit();
	if (port){
		return TRUE;
	}else{
		// If it is open but the port has gone away, then
		// close AmigaAmp connection and it will need 
		// reopening in the future
		if (appData->initialisedAmigaAmp){
			closeAmigaAmp(app);
		}
	}	
	return FALSE ;
}

///////////////////////////////////////////////////////////
//
// initAmigaAmp
//
// Attempt opening AmigaAmp connection - this will fail if 
// AmigaAmp isn't running. Should continue to try
// Sets initialisedAmigaAmp to TRUE in MHIVisAppData when
// connected.
static BOOL initAmigaAmp(App *app)
{
	BOOL ret = FALSE ;
	struct PluginMessage *AAmsg = NULL;
	struct MHIVisAppData *appData = (struct MHIVisAppData *)app->appContext;
	
	Forbid(); // no protection for FindPort
	appData->PluginMP=FindPort("AmigaAMP plugin port");
	Permit();
	if(!appData->PluginMP){
		closeAmigaAmp(app);
		goto exit;// Cannot talk to AmigaAMP until port is available
	}
	
	if (!appData->initialisedAmigaAmp){
		// Setup structure for plugin messages
		// All data for struct has been allocated already, just copy over for registration
		appData->PluginMsg->msg.mn_Node.ln_Type = NT_MESSAGE;
		appData->PluginMsg->msg.mn_Length       = sizeof(struct PluginMessage);
		appData->PluginMsg->msg.mn_ReplyPort    = appData->pluginReplyPort;
		appData->PluginMsg->PluginMask          = 1L << appData->pluginSig ;
		appData->PluginMsg->PluginTask          = (struct Process *)FindTask(NULL);
		appData->PluginMsg->SpecRawL            = &appData->SpecRawL;
		appData->PluginMsg->SpecRawR            = &appData->SpecRawR;
		appData->PluginMsg->InfoMask            = 1L << appData->infoSig ;   // v1.2
		appData->PluginMsg->tinfo               = &appData->tinfo;     // v1.2
		//appData->PluginMsg->PluginWP            = appData->windowReplyPort;  // v1.4 - AmigaAmp window updates (position,etc)
		appData->PluginMsg->PluginWP            = NULL ; // not using
		appData->PluginMsg->SampleRaw           = &appData->SampleRaw; // v1.3
		
		PutMsg(appData->PluginMP, (struct Message *)(appData->PluginMsg));
		WaitPort(appData->pluginReplyPort);
		AAmsg = (struct PluginMessage *)GetMsg(appData->pluginReplyPort);
		if (!AAmsg->Accepted){
			closeAmigaAmp(app);
			goto exit;
		}
		appData->initialisedAmigaAmp = TRUE;
		enableLCDIndicator(&waico, TRUE);
	}	
	ret = TRUE ;
exit:

	return ret ;
}

///////////////////////////////////////////////////////////
//
// closeAmigaAmp
//
// Close an open AmigaAmp connection - app may close, but 
// we can continue with the MHI connection
static void closeAmigaAmp(App *app)
{
	struct MHIVisAppData *appData = (struct MHIVisAppData *)app->appContext;
	
	if (appData){
		if (!appData->initialisedAmigaAmp){
			return ; // nothing to do
		}
		
		if (appData->PluginMsg){ // Should be allocated, but check anyway
			appData->PluginMsg->PluginMask          = 0;
			appData->PluginMsg->PluginTask          = NULL;
			appData->PluginMsg->SpecRawL            = NULL;
			appData->PluginMsg->SpecRawR            = NULL;
			appData->PluginMsg->InfoMask            = 0;     // v1.2
			appData->PluginMsg->tinfo               = NULL;  // v1.2
			appData->PluginMsg->PluginWP            = NULL;  // v1.4
			appData->PluginMsg->SampleRaw           = NULL;  // v1.3
		}		
		
		PutMsg(appData->PluginMP, (struct Message *)appData->PluginMsg);
		/* Wait for confirmation before going on! */
		WaitPort(appData->pluginReplyPort);
		GetMsg(appData->pluginReplyPort);
		
		appData->initialisedAmigaAmp = FALSE;
		enableLCDIndicator(&waico, FALSE);
	}
}

static void sizeLCDIndicator(struct RastPort *rp, struct LCDIndicator *ico)
{
	struct TextExtent te;
	TextExtent(rp, ico->text, strlen(ico->text), &te);
	ico->width = te.te_Width;
	ico->height = te.te_Height;
}

static void textLCDIndicator(struct MHIVisAppData *appData, struct RastPort *rp, struct LCDIndicator *ico, char *text)
{
	ULONG oldPen = 0;

	oldPen = GetAPen(rp);
	// Erase existing
	SetAPen(rp, appData->bgPen);
	if (ico->rjustify){
		RectFill(rp, ico->x - ico->width, ico->y - ico->height, ico->x, ico->y);
	}else{
		RectFill(rp, ico->x, ico->y - ico->height, ico->x+ico->width, ico->y);
	}
	SetAPen(rp, oldPen);
	ico->text = text;
	sizeLCDIndicator(rp, ico); // resize for new text
	ico->changed = TRUE;
}

static void enableLCDIndicator(struct LCDIndicator *ico, BOOL enable)
{
	if (ico->enabled != enable){
		ico->changed = TRUE ;
		ico->enabled = enable;
	}	
}
static void drawLCDIndicator(struct MHIVisAppData *appData, struct RastPort *rp, struct LCDIndicator *ico)
{
	if (ico->changed){
		if (ico->width == 0 || ico->height == 0){
			sizeLCDIndicator(rp, ico);
		}
		SetAPen(rp, appData->bgPen);
		if (ico->rjustify){
			RectFill(rp, ico->x - ico->width, ico->y - ico->height, ico->x, ico->y);
			Move(rp, ico->x - ico->width, ico->y);
		}else{
			RectFill(rp, ico->x, ico->y - ico->height, ico->x+ico->width, ico->y);
			Move(rp, ico->x, ico->y);
		}
		if (ico->enabled){
			SetAPen(rp, appData->vuPen);
		}else{
			SetAPen(rp, appData->vuPenFade);
		}
		Text(rp,ico->text,strlen(ico->text));
		ico->changed = FALSE;
	}
}

static BOOL initGraphics(App *myApp, struct MHIVisAppData *appData)
{
	UWORD i = 0, stridebmp = 0;
	UBYTE usePen = 0;
	UWORD stridepix = 0;
	
	// Setup graphics
	if (!myApp->appScreen){
		ShowRequester("Failed to get screen data\n", "Exit");
		return FALSE ;
	}
	appData->windowWidth = myApp->mainWnd.appWindow->Width - myApp->mainWnd.appWindow->BorderLeft - myApp->mainWnd.appWindow->BorderRight;
	appData->windowHeight = myApp->mainWnd.appWindow->Height - myApp->mainWnd.appWindow->BorderTop - myApp->mainWnd.appWindow->BorderBottom;
	appData->bgPen = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0x7EFFFFFF, 0x8DFFFFFF, 0x6CFFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	appData->vuPen = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0x39FFFFFF, 0x40FFFFFF, 0x31FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	appData->vuPenFade = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0x70FFFFFF, 0x7CFFFFFF, 0x60FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	appData->vuWidth = appData->windowWidth / 7;
	appData->vuHeight = appData->windowHeight / 2;
	appData->vuLeftYStart = appData->windowHeight;
	appData->vuLeftXStart = appData->vuWidth * 1;
	appData->vuRightYStart = appData->windowHeight;
	appData->vuRightXStart = appData->vuWidth * 5;
	
	if (!(appData->rpVU =(struct RastPort *)AllocVec(sizeof(struct RastPort),MEMF_CHIP|MEMF_CLEAR))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		return FALSE;
	}
	CopyMem(myApp->mainWnd.appWindow->RPort, appData->rpVU, sizeof(struct RastPort));
	
	stridebmp = (((appData->vuWidth+15)>>4)<<1);
	stridepix = (((appData->vuWidth+15)>>4)<<4);
	
	appData->rpVU->Layer  = NULL;
	appData->rpVU->BitMap = AllocBitMap(stridebmp, 1, 8, BMF_CLEAR|BMF_DISPLAYABLE, myApp->mainWnd.appWindow->RPort->BitMap);
	if (!(appData->vuPixels=AllocVec((stridepix * (appData->vuHeight)), MEMF_ANY | MEMF_CLEAR))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		return FALSE;
	}
	
	// Alloc background image
	if (!(appData->bgImage = xbmToPixelImage(SPIder_Xbitmap_bits, SPIder_Xbitmap_width, SPIder_Xbitmap_height, appData->bgPen, appData->vuPenFade))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		return FALSE ;
	}
	
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->vuPen);
	SetBPen(myApp->mainWnd.appWindow->RPort, appData->bgPen);
	myApp->mainWnd.appWindow->BlockPen = appData->bgPen;
	myApp->mainWnd.appWindow->DetailPen = appData->vuPen;
	
	// Draw stripes in pixel array
	for (i=0; i < appData->vuHeight; i++){
		if ((i % 5) == 0 || (i % 5) == 1){
			usePen = appData->vuPen;
		}else{
			usePen = appData->bgPen;
		}
		memset(appData->vuPixels + (i*stridepix), usePen, stridepix);
	}
	
	if ((appData->lcdFont = OpenFont(&lcd))){
		SetFont(myApp->mainWnd.appWindow->RPort, appData->lcdFont);
	}

	// Clear background to theme pen colour
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->bgPen);
	RectFill(myApp->mainWnd.appWindow->RPort, 
					0,0, 
					appData->windowWidth , 
					appData->windowHeight);
					
	// Draw background image
	
	WritePixelArray8(myApp->mainWnd.appWindow->RPort, 
						80, 
						50, 
						80 + appData->bgImage->width-1, 
						50 + appData->bgImage->height-1, 
						appData->bgImage->pixelArray, 
						appData->rpVU);
	
	// Setup right side indicators for stream type
	sizeLCDIndicator(myApp->mainWnd.appWindow->RPort, &mp3ico);
	mp3ico.x = appData->windowWidth - 35;
	wavico.x = wmaico.x = flcico.x = oggico.x = aacico.x = mp3ico.x;
	mp3ico.y = ((appData->windowHeight*1)/10) + 5;
	wavico.y = mp3ico.y + 15;
	aacico.y = wavico.y + 15;
	wmaico.y = aacico.y + 15;
	flcico.y = wmaico.y + 15;
	oggico.y = flcico.y + 15;
	
	// Setup top of screen indicators for device type
	sizeLCDIndicator(myApp->mainWnd.appWindow->RPort, &vs53ico); // set the size
	sizeLCDIndicator(myApp->mainWnd.appWindow->RPort, &vs63ico); // set the size
	vs53ico.x = (appData->windowWidth /2) + 10;
	vs53ico.y = 20;
	vs63ico.x = vs53ico.x + vs53ico.width + 10;
	vs63ico.y = vs53ico.y;
	
	// Setup MHI external decoding indicator
	sizeLCDIndicator(myApp->mainWnd.appWindow->RPort, &waico);
	waico.x = 10;
	waico.y = mp3ico.y;
	mhiico.x = waico.x;
	mhiico.y = waico.y + 15;
	
	// Setup frequency and rate indicators
	sizeLCDIndicator(myApp->mainWnd.appWindow->RPort, &freqico);
	freqico.x = (appData->windowWidth /2) + 90;
	freqico.y = wavico.y;
	bpsico.x = freqico.x;
	bpsico.y = freqico.y + freqico.height + 10;
	
	freqtxtico.x = (appData->windowWidth /2) + 85;
	freqtxtico.y = freqico.y;
	bpstxtico.x = freqtxtico.x;
	bpstxtico.y = bpsico.y;
	
	// Create separator for hw type
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->vuPen);
	Move(myApp->mainWnd.appWindow->RPort, vs53ico.x, vs53ico.y + 5);
	Draw(myApp->mainWnd.appWindow->RPort, vs53ico.x + vs53ico.width + 10 + vs63ico.width , vs53ico.y + 5);
	
	// Create a separator for stream type
	Move(myApp->mainWnd.appWindow->RPort, mp3ico.x - 8, mp3ico.y - mp3ico.height);
	Draw(myApp->mainWnd.appWindow->RPort, mp3ico.x - 8, oggico.y);
	
	// Mirror stream and hw type on left
	// Left horizontal bar
	Move(myApp->mainWnd.appWindow->RPort, 55, vs53ico.y + 5);
	Draw(myApp->mainWnd.appWindow->RPort, (appData->windowWidth /2) - 10 , vs53ico.y + 5);
	// Left column bar
	Move(myApp->mainWnd.appWindow->RPort, 43, mp3ico.y - mp3ico.height);
	Draw(myApp->mainWnd.appWindow->RPort, 43, oggico.y);
	
	// Draw horizontal and vertical screen divide
	Move(myApp->mainWnd.appWindow->RPort, 10, appData->vuLeftYStart - appData->vuHeight - 10);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth - 10, appData->vuLeftYStart - appData->vuHeight - 10);
	
	Move(myApp->mainWnd.appWindow->RPort, appData->windowWidth / 2, appData->vuLeftYStart - appData->vuHeight);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth / 2, appData->vuLeftYStart);
	
	// Draw screen shadow
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->vuPenFade);
	Move(myApp->mainWnd.appWindow->RPort, 0, 0);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth, 0);
	Move(myApp->mainWnd.appWindow->RPort, 0, 1);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth, 1);
	Move(myApp->mainWnd.appWindow->RPort, 0, 2);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth, 2);
	Move(myApp->mainWnd.appWindow->RPort, 0, 3);
	Draw(myApp->mainWnd.appWindow->RPort, appData->windowWidth, 3);

	Move(myApp->mainWnd.appWindow->RPort, 0, 4);
	Draw(myApp->mainWnd.appWindow->RPort, 0, appData->windowHeight);
	Move(myApp->mainWnd.appWindow->RPort, 1, 4);
	Draw(myApp->mainWnd.appWindow->RPort, 1, appData->windowHeight);
	Move(myApp->mainWnd.appWindow->RPort, 2, 4);
	Draw(myApp->mainWnd.appWindow->RPort, 2, appData->windowHeight);
	Move(myApp->mainWnd.appWindow->RPort, 3, 4);
	Draw(myApp->mainWnd.appWindow->RPort, 3, appData->windowHeight);
	
	return TRUE ;
}

void configureCompressor(struct MHIVisAppData *appData)
{
	// Define compressor with x^2 + 3x = 4ay, where a=23 for 0-100% scale
	WORD x = 0, percent = 0;
	for (;x<MAX_DB+1;x++){
		percent = ((x*x) - 10*x)/(25*4);
		if (percent < 0){
			percent = 0; // clipped
		}
		if (percent > 100){
			percent = 100; // clipped
		}
		appData->compressor[x] = (UBYTE)percent;
	}
}

///////////////////////////////////////////
//
// initAppData
//
// All data we can setup in advance and is 
// one off for use with AmigaAmp
//
static BOOL initAppData(App *myApp, struct MHIVisAppData *appData)
{
	BOOL ret = FALSE ;
	const UWORD buildVersion = LIBDEVMAJOR << 8 | LIBDEVMINOR;
	struct MHIMinHandle *drv;
	
	myApp->appContext = appData;
	
	memset(appData, 0, sizeof(struct MHIVisAppData));
	appData->pluginSig = appData->mhisig = appData->infoSig = -1;
	appData->bgPen = -1;
	appData->vuPen = -1;
	appData->initialisedMHI = TRUE ;
  
	if (!(appData->mhibase = OpenLibrary("LIBS:MHI/mhispiaudio.library", 0))){
		//ShowRequester("Cannot open mhispiaudio.library. This needs to be installed to use the plugin with MHI", "Continue");
		appData->initialisedMHI = FALSE;
	}
	enableLCDIndicator(&mhiico, appData->initialisedMHI);
	
	appData->pluginSig = AllocSignal(-1);
	appData->infoSig = AllocSignal(-1);
	appData->mhisig = AllocSignal(-1);
	if (appData->pluginSig < 0 || appData->infoSig < 0 || appData->mhisig < 0){
		ShowRequester("Failed to allocate signal resources", "Exit");
		goto exit;
	}
	
	if (appData->initialisedMHI){
		if (!(appData->mhihandle = MHIAllocDecoder (FindTask(NULL) , appData->mhisig))){
			ShowRequester("Couldn't alloc handle\n", "Exit") ;
			goto exit;
		}
		appData->lastMHIStatus = MHIGetStatus(appData->mhihandle);
	
		drv = (struct MHIMinHandle*)appData->mhihandle;
		if (drv->version > buildVersion){
			ShowRequester("MHI library different to this visualiser build\n", "Exit") ;
			goto exit;
		}
		appData->mhiDrvPort = drv->port; // Set the port used by the MHI driver for SPIder
	}	
	if (!(appData->mhiReplyPort = CreateMsgPort())){
		ShowRequester("Failed to allocate port resources", "Exit");
		goto exit;
	}
	
	if (!(appData->pluginReplyPort = CreateMsgPort())){
		ShowRequester("Failed to allocate port resources", "Exit");
		goto exit;
	}
	
	if (!(appData->windowReplyPort = CreateMsgPort())){
		ShowRequester("Failed to allocate port resources", "Exit");
		goto exit;
	}
		
	if (!(appData->PluginMsg=AllocVec(sizeof(struct PluginMessage), MEMF_PUBLIC|MEMF_CLEAR))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		goto exit ;
	}
	
	if (!(appData->mhiMsg=AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC|MEMF_CLEAR))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		goto exit ;
	}
	
	appData->mhiMsg->io_Message.mn_ReplyPort = appData->mhiReplyPort;
	appData->mhiMsg->io_Message.mn_Length = sizeof(struct IOStdReq) ;
	appData->mhiMsg->io_Message.mn_Node.ln_Type = NT_MESSAGE;

	configureCompressor(appData);
	// Setup the screen
	if (!initGraphics(myApp, appData)){
		goto exit;
	}
	
	if (appData->initialisedMHI){
		//doInfo(appData);
		//WaitPort(appData->mhiReplyPort);
		//GetMsg(appData->mhiReplyPort);
	}
	ret = TRUE;
exit:
	if (!ret){
		destroyAppData(myApp);
	}
	return ret;
}

static void destroyAppData(App *myApp)
{
	struct MHIVisAppData *appData = (struct MHIVisAppData *)myApp->appContext;
	if (appData){
		if (appData->rpVU){
			if (appData->rpVU->BitMap){
				FreeBitMap(appData->rpVU->BitMap);
				appData->rpVU->BitMap = NULL;
			}
			FreeVec(appData->rpVU);
			appData->rpVU = NULL;
		}
		if (appData->lcdFont){
			CloseFont(appData->lcdFont);
		}
		if (appData->vuPixels){
			FreeVec(appData->vuPixels);
			appData->vuPixels = NULL ;
		}
		if (appData->bgPen >= 0 && myApp->appScreen){
			ReleasePen(myApp->appScreen->ViewPort.ColorMap, appData->bgPen);
			appData->bgPen = -1;
		}
		if (appData->vuPen >= 0 && myApp->appScreen){
			ReleasePen(myApp->appScreen->ViewPort.ColorMap, appData->vuPen);
			appData->vuPen = -1;
		}
		if (appData->pluginSig >= 0){
			FreeSignal(appData->pluginSig);
			appData->pluginSig = -1;
		}
		if (appData->infoSig >= 0){
			FreeSignal(appData->infoSig);
			appData->infoSig = -1;
		}
		if (appData->mhisig  >= 0){
			FreeSignal(appData->mhisig );
			appData->mhisig = -1;
		}
		if (appData->pluginReplyPort){
			DeleteMsgPort(appData->pluginReplyPort);
			appData->pluginReplyPort = NULL;
		}
		if (appData->windowReplyPort){
			DeleteMsgPort(appData->windowReplyPort);
			appData->windowReplyPort = NULL;
		}
		if (appData->mhiReplyPort){
			if (appData->mhiMsgInFlight){
				WaitPort(appData->mhiReplyPort);
				GetMsg(appData->mhiReplyPort);
			}
			DeleteMsgPort(appData->mhiReplyPort);
			appData->mhiReplyPort = NULL;
		}
		if (appData->mhiMsg){
			FreeVec(appData->mhiMsg);
			appData->mhiMsg = NULL;
		}
		if (appData->PluginMsg){
			FreeVec(appData->PluginMsg);
			appData->PluginMsg = NULL;
		}
	}
}

static ULONG vsDataRate(struct MHIVisAppData *appData)
{
	ULONG dataRate = 0;
	UWORD ID = 0, Layer = 0, BitRate = 0 ;
	UWORD l2ID3[16] = {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0};
	UWORD l2IDX[16] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
	UWORD l3ID3[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
	UWORD l3IDX[16] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
	switch(appData->info.hdat1){
		case 0x7665: // WAV
		case 0x4154: // AAC
		case 0x574D: // WMA
		case 0x4F67: // OGG
			dataRate = appData->info.hdat0 * 8;
			break;
		case 0x664c:
			dataRate = appData->info.hdat0 * 32;
			break;
		default:
			if (appData->info.hdat1 >= 0xFFE0){
				// MPX
				ID = (appData->info.hdat1 >> 3) & 0x0003;
				Layer = (appData->info.hdat1 >> 1) & 0x0003;
				BitRate = (appData->info.hdat0 >> 12) & 0x000F;

				if (Layer == 2){
					if (ID == 3){
						dataRate = l2ID3[BitRate];
					}else{
						dataRate = l2IDX[BitRate];
					}
				}else if(Layer == 1){
					if (ID == 3){
						dataRate = l3ID3[BitRate] ;
					}else{
						dataRate = l3IDX[BitRate] ;
					}
				}// Else MP1 with no tables in documentation
				dataRate *= 1000;
			}
	};
	
	return dataRate;
}

static ULONG vsSampleRate(struct MHIVisAppData *appData)
{
	ULONG sampleRate = 0;
	UWORD ID = 0, rate = 0 ;
	UWORD samplesX[4] = {11025, 12000, 8000, 0};
	UWORD samples2[4] = {22050, 24000, 16000, 0};
	UWORD samples3[4] = {44100, 48000, 32000, 0};
	
	
	if (appData->info.hdat1 >= 0xFFE0){
		ID = (appData->info.hdat1 >> 3) & 0x0003;
		rate = (appData->info.hdat0 >> 10) & 0x0003;
		if (ID == 3){
			sampleRate = samples3[rate];
		}else if (ID == 2){
			sampleRate = samples2[rate];
		}else{
			sampleRate = samplesX[rate];
		}
	}else{
		sampleRate = appData->info.audata & 0xFFFE;
	}
	return sampleRate;
}

static void doInfo(struct MHIVisAppData *appData)
{
	if (!appData->mhiMsgInFlight){
		appData->mhiMsgInFlight = TRUE;
		appData->mhiMsg->io_Command = CMD_VSAUDIO_INFO;
		appData->mhiMsg->io_Length = sizeof(struct VSMediaInfo);
		appData->mhiMsg->io_Data = &appData->info;
		PutMsg(appData->mhiDrvPort, (struct Message *)appData->mhiMsg);
	}
}
static void doVUMeter(struct MHIVisAppData *appData)
{
	if (!appData->mhiMsgInFlight){
		appData->mhiMsgInFlight = TRUE;
		appData->param.parameter = VS_PARAM_VUMETER;
		appData->param.value = 0;
		appData->mhiMsg->io_Command = CMD_VSAUDIO_PARAMETER;
		appData->mhiMsg->io_Length = sizeof(struct VSParameterData);
		appData->mhiMsg->io_Data = &appData->param;
		PutMsg(appData->mhiDrvPort, (struct Message *)appData->mhiMsg);
	}
}
static void appSigs(struct App *myApp, ULONG sigs)
{
	struct Device *TimerBase = myApp->tmr->io_Device;
	struct IOStdReq *msg = NULL;
	struct timeval t1, t2;
	struct WindowMessage *winMsg = NULL ;
	UWORD barHeightL = 0, barHeightR = 0, stridepix = 0, i =0;
	struct MHIVisAppData *appData = (struct MHIVisAppData *)myApp->appContext;
	
	GetSysTime(&t1);
	t2 = t1; // Save the system time (as SubTime overwrites)
	stridepix = (((appData->vuWidth+15)>>4)<<4);

	if (sigs & SIGBREAKF_CTRL_C){
		closeAmigaAmp(myApp);
	}else{
		// Only process these if Amiga Amp is around and not just closed
		if (sigs & (1L << appData->pluginSig)){
			// Visualisation changes
			if (!appData->initialisedMHI || appData->lastMHIStatus != MHIF_PLAYING){
				// Don't update left and right if MHI is playing (data will be zero from AmigaAmp)
				appData->dbLeft = appData->SpecRawL[0] / 682; 
				appData->dbRight = appData->SpecRawR[0] / 682; 
				for (i=1;i<512;i++){
					if (appData->dbLeft < (appData->SpecRawL[i] / 682)){
						appData->dbLeft = (appData->SpecRawL[i] / 682);
					}
					if (appData->dbRight < (appData->SpecRawR[i] / 682)){
						appData->dbRight = (appData->SpecRawR[i] / 682);
					}
				}
			}
		}
		if (sigs & (1L << appData->infoSig)){
			// New track info
			sprintf(appData->frequencyText,"%u",appData->tinfo->Frequency);
			textLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &freqtxtico, appData->frequencyText);
			sprintf(appData->rateText,"%u",appData->tinfo->Bitrate * 1000);
			textLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &bpstxtico, appData->rateText);
		}
		if (sigs & (1L << appData->windowReplyPort->mp_SigBit)){
			// Window updates from AmigaAmp
			while(winMsg=(struct WindowMessage *)GetMsg(appData->windowReplyPort)) {

				ReplyMsg((struct Message *)winMsg);
			};
		}
	}
	if (sigs & (1L << appData->mhiReplyPort->mp_SigBit)){
		// MHI data responses
		if ((msg=(struct IOStdReq*)GetMsg(appData->mhiReplyPort))){
			appData->mhiMsgInFlight = FALSE ;
			if (msg->io_Command == CMD_VSAUDIO_PARAMETER){
				if (appData->param.parameter == VS_PARAM_VUMETER){
					appData->dbLeft = appData->param.actual >> 8;
					appData->dbRight = appData->param.actual & 0x00FF;

				}
			}else if (msg->io_Command == CMD_VSAUDIO_INFO){
				enableLCDIndicator(&vs53ico, appData->info.hwVersion == 4?TRUE:FALSE);
				enableLCDIndicator(&vs63ico, appData->info.hwVersion == 6?TRUE:FALSE);
				enableLCDIndicator(&wavico, appData->info.hdat1 == 0x7665?TRUE:FALSE);
				enableLCDIndicator(&aacico, appData->info.hdat1 == 0x4154?TRUE:FALSE);
				enableLCDIndicator(&wmaico, appData->info.hdat1 == 0x574D?TRUE:FALSE);
				enableLCDIndicator(&oggico, appData->info.hdat1 == 0x4F67?TRUE:FALSE);
				enableLCDIndicator(&flcico, appData->info.hdat1 == 0x664C?TRUE:FALSE);
				enableLCDIndicator(&mp3ico, appData->info.hdat1 >=0xFFE0?TRUE:FALSE);
				sprintf(appData->frequencyText,"%u",vsSampleRate(appData));
				textLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &freqtxtico, appData->frequencyText);
				sprintf(appData->rateText,"%u",vsDataRate(appData));
				textLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &bpstxtico, appData->rateText);
			}
		}
	}

	if (appData->initialisedMHI){
		if (appData->refreshMHIData && !appData->mhiMsgInFlight){
			doInfo(appData);
			appData->refreshMHIData = FALSE ;
		}

		if (appData->lastMHIStatus == MHIF_PLAYING){
			// Request new values from the MHI driver
			SubTime(&t1, &appData->lastVis);
			if (t1.tv_secs >0 || t1.tv_micro > 100000){ // Request new values every 1/10 sec	
				doVUMeter(appData);
				appData->lastVis = t2;
			}
			
		}else { // Not playing and no AmigaAmp connected
			if (!appData->initialisedAmigaAmp){
				appData->dbLeft = appData->dbRight = 0; // drop VU to zero if not playing
			}
		}
	}
	
	// is it time to check AmigaAmp again? Has MHI changed?
	// Check it still exists and connect if not already connected.
	t1 = t2; // Restore system time for next check
	SubTime(&t1, &appData->lastAmpCheck);
	if (t1.tv_secs > 1){
		if (!appData->initialisedAmigaAmp && existsAmigaAmp(myApp)){
			if (initAmigaAmp(myApp)){
				// Do something - If you want to - maybe an icon lights up
			}
		}
		if (appData->initialisedMHI){
			// Check if the play status has changed for MHI
			if (appData->lastMHIStatus != MHIGetStatus(appData->mhihandle)){
				appData->lastMHIStatus = MHIGetStatus(appData->mhihandle);
				if (appData->lastMHIStatus == MHIF_PLAYING){
					appData->refreshMHIData = TRUE ;
				}
			}
		}
		appData->lastAmpCheck = t2;
	}

	// This function also triggers for timer events
	// Run any animation frames/actions within this function. 
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &mp3ico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &wavico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &wmaico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &flcico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &oggico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &aacico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &vs63ico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &vs53ico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &waico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &mhiico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &freqico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &bpsico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &freqtxtico);
	drawLCDIndicator(appData, myApp->mainWnd.appWindow->RPort, &bpstxtico);

	barHeightL = (appData->vuHeight * (UWORD)(appData->compressor[appData->dbLeft]))/ 100;
	barHeightR = (appData->vuHeight * (UWORD)(appData->compressor[appData->dbRight]))/ 100;
	
		SetAPen(myApp->mainWnd.appWindow->RPort, appData->bgPen);
		RectFill(myApp->mainWnd.appWindow->RPort, 
					appData->vuLeftXStart,
					appData->vuLeftYStart-appData->vuHeight, 
					appData->vuLeftXStart + appData->vuWidth, 
					appData->vuLeftYStart - barHeightL);

	if(barHeightL > 1){
		SetAPen(myApp->mainWnd.appWindow->RPort, appData->vuPen);
		
		WritePixelArray8(myApp->mainWnd.appWindow->RPort, 
						appData->vuLeftXStart, 
						appData->vuLeftYStart - barHeightL, 
						appData->vuLeftXStart+appData->vuWidth-1, 
						appData->vuLeftYStart-1, 
						appData->vuPixels + (stridepix * (appData->vuHeight - barHeightL)), 
						appData->rpVU);
		
	}
	
		SetAPen(myApp->mainWnd.appWindow->RPort, appData->bgPen);
		RectFill(myApp->mainWnd.appWindow->RPort, 
					appData->vuRightXStart,
					appData->vuRightYStart-appData->vuHeight, 
					appData->vuRightXStart + appData->vuWidth, 
					appData->vuRightYStart - barHeightR);
				
	if(barHeightR > 1){
		SetAPen(myApp->mainWnd.appWindow->RPort, appData->vuPen);
		
		WritePixelArray8(myApp->mainWnd.appWindow->RPort, 
						appData->vuRightXStart, 
						appData->vuRightYStart - barHeightR, 
						appData->vuRightXStart+appData->vuWidth-1, 
						appData->vuRightYStart-1, 
						appData->vuPixels + (stridepix * (appData->vuHeight - barHeightR)), 
						appData->rpVU);
		
	}
	
}

int main(void)
{
    App myApp ;
	struct MHIVisAppData appData;
	Wnd *appWnd;
	int ret = 0;
	
	if ((ret=initialiseApp(&myApp)) != 0){
		return ret;
	}
	IntuitionBase = myApp.intu;
	GfxBase = myApp.gfx;
	
	setWakeTimer(&myApp, 0, 33333); // 30 FPS(ish)
	
	appWnd = getAppWnd(&myApp);
	wndSetSize(appWnd, 350, 300);
	appWnd->info.Title = "SPIder Visualiser";
    
	appWnd->info.Flags = WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_GIMMEZEROZERO ;
	openAppWindow(appWnd, NULL);
	if (!initAppData(&myApp, &appData)){
		goto exit;
	}
	
	initAmigaAmp(&myApp) ; // Try and open AmigaAmp 
	
	// Set app callback for all signals we are interested in
	// pluginSig - visualisation data
	// infoSig - new track info
	// window move/change sig - AmigaAmp window changed
	myApp.wake_sigs = (1L << appData.pluginSig) | (1L << appData.infoSig) | (1L << appData.windowReplyPort->mp_SigBit) | (1L << appData.mhiReplyPort->mp_SigBit) | SIGBREAKF_CTRL_C;
	myApp.fn_wakeSigs = appSigs;
       
    dispatch(&myApp);
    
exit:
	closeAmigaAmp(&myApp);
	destroyAppData(&myApp);
    appCleanUp(&myApp);
    
    return(0);
 }