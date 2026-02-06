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

#define _STR(A) #A
#define STR(A) _STR(A)

static const char version_string[] =
  "$VER: spidervis "  STR(LIBDEVMAJOR) "." STR(LIBDEVMINOR) " (" STR(LIBDEVDATE) ")\n";

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
static ULONG doHeadphoneSetting(struct MHIVisAppData *appData, UBYTE level);
static BOOL doAdmixSetting(struct MHIVisAppData *appData, BOOL write, BOOL enable);
static void enableLCDIndicator(struct LCDIndicator *ico, BOOL enable);
static struct RenderObject* createButtonRender(App *myApp, AppGadget *gad);
static struct RenderObject* createButtonIndicator(App *myApp);
static void freeRenderObject(struct RenderObject *obj);
static void addTailRenderObject(struct RenderObject *first, struct RenderObject *second);
static struct RenderObject *cloneRenderObject(struct RenderObject *obj, WORD x, WORD y);
static struct RenderObject* createIconPanel(LONG pen, LONG width);
static struct RenderObject* createHeadphoneIcon(LONG pen);
static void detachTailRenderObject(struct RenderObject *obj);
static void setRenderObjectOffset(struct RenderObject *obj, WORD x, WORD y);
static void updateHeadphoneIndicator(struct MHIVisAppData *appData);
static struct RenderObject* createAdmixIcon(LONG pen);
static void updateAdmixIndicator(struct MHIVisAppData *appData);

struct RenderObject
{
	struct Border head;
	struct Border *tail;
	struct RenderObject *next;
	struct RenderObject *prev;
	struct ColorMap *cm; 
	UWORD allocatedPenCount;
	LONG *allocatedPens;
	BOOL isClone;
};

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

INIT_APPGADGET(btnHeadphone, BUTTON_KIND,15, 254, 94, 25, NULL, NULL, 1, PLACETEXT_IN);
INIT_APPGADGET(btnAdmix, BUTTON_KIND,110, 254, 94, 25, NULL, NULL, 1, PLACETEXT_IN);

struct MHIMinHandle{
	UWORD version;
	struct MsgPort *port;
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
	struct 	IOStdReq *mhiUIMsg ;
	UWORD	*SpecRawL;
	UWORD	*SpecRawR;
	WORD  	*SampleRaw;            // v1.3
	struct TrackInfo 		*tinfo;
	struct MsgPort	 		*mhiReplyPort; 			// MHI comms port for direct messages to driver
	struct MsgPort 			*mhiUIReplyPort;		// MHI comms port for UI (should wait on this port)
	struct MsgPort	 		*mhiDrvPort;			// Driver port for MHI
	struct MsgPort   		*PluginMP;				// Public port exposed by AmigaAmp
	struct MsgPort   		*pluginReplyPort;		// For any replies regarding plugin data
	struct MsgPort   		*windowReplyPort;		// For any AmigaAmp window updates
	struct timeval 			lastVis;
	struct timeval 			lastAmpCheck;
	struct VSParameterData 	param;
	struct VSParameterData 	uiparam;
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
	struct RenderObject *customButtonRender;
	struct RenderObject *customButtonIndicatorRender[3];
	struct RenderObject *customButtonRenderAdmix;
	struct RenderObject *customButtonIndicatorAdmix;
	UBYTE headphoneMode;
	struct RenderObject *lcdPanel;
	struct RenderObject *headphoneIcon;
	struct RenderObject *admixIcon;
	BOOL adMixOn;
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
	const UWORD button_border_area_height = 50;
	struct Border *tmpborder;
	
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
	appData->vuHeight = appData->windowHeight / 3;
	appData->vuLeftYStart = appData->windowHeight - button_border_area_height;
	appData->vuLeftXStart = appData->vuWidth * 1;
	appData->vuRightYStart = appData->windowHeight - button_border_area_height;
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
	if (!(appData->bgImage = xbmToPixelImage(SPIder_Xbitmap_bits, SPIder_Xbitmap_width, SPIder_Xbitmap_height, appData->bgPen, appData->vuPen))){
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
	
	// Create new button border render
	if (!(appData->customButtonRender = createButtonRender(myApp, &btnHeadphone))){
		return FALSE;
	}
	
	
	// Create a render for the button active indicator
	if (!(appData->customButtonIndicatorRender[0] = createButtonIndicator(myApp))){
		return FALSE;
	}
	
	appData->customButtonIndicatorRender[0]->tail->Count = 0; // disable the border for "on" colour
	
	// Create 2 more button indicators as clones for the headphone button
	if (!(appData->customButtonIndicatorRender[1] = cloneRenderObject(appData->customButtonIndicatorRender[0], -16,0))){
		return FALSE;
	}
	if (!(appData->customButtonIndicatorRender[2] = cloneRenderObject(appData->customButtonIndicatorRender[1], -16,0))){
		return FALSE;
	}
	// Link all renders together
	addTailRenderObject(appData->customButtonRender, appData->customButtonIndicatorRender[0]);
	addTailRenderObject(appData->customButtonIndicatorRender[0], appData->customButtonIndicatorRender[1]);
	addTailRenderObject(appData->customButtonIndicatorRender[1], appData->customButtonIndicatorRender[2]);
	
	// Create a clone of the button render for Admix control
	if (!(appData->customButtonRenderAdmix = cloneRenderObject(appData->customButtonRender, 0,0))){
		return FALSE;
	}
	if (!(appData->customButtonIndicatorAdmix = cloneRenderObject(appData->customButtonIndicatorRender[0], 0,0))){
		return FALSE;
	}
	addTailRenderObject(appData->customButtonRenderAdmix, appData->customButtonIndicatorAdmix);
	
	// Draw button panel, steal a pen
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->customButtonRender->head.NextBorder->NextBorder->FrontPen);
	RectFill(myApp->mainWnd.appWindow->RPort, 
					0,249, 
					appData->windowWidth , 
					253);
	SetAPen(myApp->mainWnd.appWindow->RPort, appData->customButtonRender->head.FrontPen);
	RectFill(myApp->mainWnd.appWindow->RPort, 
					0,254, 
					appData->windowWidth , 
					appData->windowHeight);
	
	// Remove appgadget so it can be reapplied with new flags
	removeAppGadget(&btnHeadphone);
	removeAppGadget(&btnAdmix);

	// Add custom render and change the button flags. We are now ready to render the custom button
	if (!(setCustomBorder(&btnHeadphone, (struct Border*)appData->customButtonRender))){
		return FALSE ;
	}
	if (!(setCustomBorder(&btnAdmix, (struct Border*)appData->customButtonRenderAdmix))){
		return FALSE ;
	}
	
	//btnAdmix.gadget->Flags &= ~ GFLG_GADGHIGHBITS; // if you set this to zero then you get the invert selection look
	addAppGadget(&myApp->mainWnd, &btnAdmix);
	//btnHeadphone.gadget->Flags &= ~ GFLG_GADGHIGHBITS; // if you set this to zero then you get the invert selection look
	addAppGadget(&myApp->mainWnd, &btnHeadphone);
	
	// Draw panel for headphone
	if (!(appData->lcdPanel = createIconPanel(appData->vuPen, btnHeadphone.gadget->Width))){
		return FALSE;
	}
	if (!(appData->headphoneIcon = createHeadphoneIcon(appData->bgPen))){
		return FALSE;
	}
	setRenderObjectOffset(appData->headphoneIcon, (btnHeadphone.gadget->Width/2) -6, 3);
	addTailRenderObject(appData->lcdPanel, appData->headphoneIcon);
	DrawBorder( myApp->mainWnd.appWindow->RPort, (struct Border*)appData->lcdPanel, btnHeadphone.gadget->LeftEdge, btnHeadphone.gadget->TopEdge - 4 - 14 );
	detachTailRenderObject(appData->lcdPanel);
	
	// Draw panel for Admix
	if (!(appData->lcdPanel = createIconPanel(appData->vuPen, btnAdmix.gadget->Width))){
		return FALSE;
	}
	if (!(appData->admixIcon = createAdmixIcon(appData->bgPen))){
		return FALSE;
	}
	setRenderObjectOffset(appData->admixIcon, (btnAdmix.gadget->Width/2) -17, 3);
	addTailRenderObject(appData->lcdPanel, appData->admixIcon);
	DrawBorder( myApp->mainWnd.appWindow->RPort, (struct Border*)appData->lcdPanel, btnAdmix.gadget->LeftEdge, btnAdmix.gadget->TopEdge - 4 - 14 );
	detachTailRenderObject(appData->lcdPanel);
	
	return TRUE ;
}

static void configureCompressor(struct MHIVisAppData *appData)
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
		//appData->lastMHIStatus = MHIGetStatus(appData->mhihandle);
		appData->lastMHIStatus = MHIF_OUT_OF_DATA;
	
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
	
	if (!(appData->mhiUIReplyPort = CreateMsgPort())){
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
	
	if (!(appData->mhiUIMsg=AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC|MEMF_CLEAR))){
		ShowRequester("Failed to allocate memory resources", "Exit");
		goto exit ;
	}
	
	appData->mhiMsg->io_Message.mn_ReplyPort = appData->mhiReplyPort;
	appData->mhiMsg->io_Message.mn_Length = sizeof(struct IOStdReq) ;
	appData->mhiMsg->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	
	appData->mhiUIMsg->io_Message.mn_ReplyPort = appData->mhiUIReplyPort;
	appData->mhiUIMsg->io_Message.mn_Length = sizeof(struct IOStdReq) ;
	appData->mhiUIMsg->io_Message.mn_Node.ln_Type = NT_MESSAGE;

	configureCompressor(appData);
	// Setup the screen
	if (!initGraphics(myApp, appData)){
		goto exit;
	}
	
	if (appData->initialisedMHI){
		// Get hardware settings
		appData->headphoneMode = doHeadphoneSetting(appData, 255);
		updateHeadphoneIndicator(appData);
		removeAppGadget(&btnHeadphone);
		addAppGadget(&myApp->mainWnd, &btnHeadphone);
		
		appData->adMixOn = doAdmixSetting(appData, FALSE, TRUE);
		updateAdmixIndicator(appData);
		removeAppGadget(&btnAdmix);
		addAppGadget(&myApp->mainWnd, &btnAdmix);
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
	UWORD i = 0;
	
	struct MHIVisAppData *appData = (struct MHIVisAppData *)myApp->appContext;
	if (appData){
		if (appData->headphoneIcon){
			freeRenderObject(appData->headphoneIcon);
			appData->headphoneIcon = NULL;
		}
		if (appData->admixIcon){
			freeRenderObject(appData->admixIcon);
			appData->admixIcon = NULL;
		}
		if (appData->customButtonIndicatorAdmix){
			freeRenderObject(appData->customButtonIndicatorAdmix);
			appData->customButtonIndicatorAdmix = NULL;
		}
		if (appData->customButtonRenderAdmix){
			freeRenderObject(appData->customButtonRenderAdmix);
			appData->customButtonRenderAdmix = NULL;
		}
		if (appData->lcdPanel){
			freeRenderObject(appData->lcdPanel);
			appData->lcdPanel = NULL;
		}
		if (appData->customButtonRender){
			freeRenderObject(appData->customButtonRender);
			appData->customButtonRender = NULL;
		}
		for (i=0; i < 3; i++){
			if (appData->customButtonIndicatorRender[i]){
				freeRenderObject(appData->customButtonIndicatorRender[i]);
				appData->customButtonIndicatorRender[i] = NULL;
			}
		}
		if (appData->rpVU){
			if (appData->rpVU->BitMap){
				FreeBitMap(appData->rpVU->BitMap);
				appData->rpVU->BitMap = NULL;
			}
			FreeVec(appData->rpVU);
			appData->rpVU = NULL;
		}
		if (appData->bgImage){
			freePixelImage(appData->bgImage);
			appData->bgImage = NULL;
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
		if (appData->mhiUIReplyPort){
			DeleteMsgPort(appData->mhiUIReplyPort);
			appData->mhiUIReplyPort = NULL;
		}
		if (appData->mhiMsg){
			FreeVec(appData->mhiMsg);
			appData->mhiMsg = NULL;
		}
		if (appData->mhiUIMsg){
			FreeVec(appData->mhiUIMsg);
			appData->mhiUIMsg = NULL;
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

static ULONG doHeadphoneSetting(struct MHIVisAppData *appData, UBYTE level)
{
	ULONG actual = 0;
	struct IOStdReq *reply ;
	appData->uiparam.parameter = VS_PARAM_EARSPEAKER;
	appData->uiparam.value = level;
	appData->mhiUIMsg->io_Command = CMD_VSAUDIO_PARAMETER;
	appData->mhiUIMsg->io_Length = sizeof(struct VSParameterData);
	appData->mhiUIMsg->io_Data = &appData->uiparam;
	PutMsg(appData->mhiDrvPort, (struct Message *)appData->mhiUIMsg);
	WaitPort(appData->mhiUIReplyPort);
	reply = (struct IOStdReq*)GetMsg(appData->mhiUIReplyPort);
	if (reply && reply->io_Error == 0){
		actual = appData->uiparam.actual;
	}
	return actual;
}

static BOOL doAdmixSetting(struct MHIVisAppData *appData, BOOL write, BOOL enable)
{
	BOOL on = FALSE;
	const BYTE level = -3;
	struct IOStdReq *reply ;
	appData->uiparam.parameter = VS_PARAM_ADMIX;
	if (write){
		appData->uiparam.value = (enable?VS_PARAM_ADMIX_ENABLE:VS_PARAM_ADMIX_DISABLE) | (UBYTE)level;
	}else{
		appData->uiparam.value = 0;
	}
	appData->mhiUIMsg->io_Command = CMD_VSAUDIO_PARAMETER;
	appData->mhiUIMsg->io_Length = sizeof(struct VSParameterData);
	appData->mhiUIMsg->io_Data = &appData->uiparam;
	PutMsg(appData->mhiDrvPort, (struct Message *)appData->mhiUIMsg);
	WaitPort(appData->mhiUIReplyPort);
	reply = (struct IOStdReq*)GetMsg(appData->mhiUIReplyPort);
	if (reply && reply->io_Error == 0){
		on = (appData->uiparam.actual & VS_PARAM_ADMIX_ENABLE)?TRUE:FALSE;
	}
	return on;
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

static void updateButtonRender(struct RenderObject *obj, AppGadget *gad)
{
	struct Border *bdr = NULL ;
	struct Gadget *g = gad->gadget ;
	UWORD i = 0, maxFace = 35, pathi = 0;
	
	bdr = (struct Border*)obj;
	
	// BG Fill Clear
	bdr->XY[0]  = 0; 			bdr->XY[1]  = g->Height;
	bdr->XY[2]  = 0; 			bdr->XY[3]  = 0;
	bdr->XY[4]  = g->Width;		bdr->XY[5]  = 0;
	bdr->XY[6]  = g->Width;		bdr->XY[7]  = 1;
	bdr->XY[8]  = 1;			bdr->XY[9]  = 1;
	bdr->XY[10] = 1; 			bdr->XY[11] = g->Height;
	bdr->XY[12] = 3; 			bdr->XY[13] = g->Height;
	bdr->XY[14] = 2; 			bdr->XY[15] = g->Height - 1;

	bdr=bdr->NextBorder;
	if (!bdr){
		return;
	}
	
	// Highlight
	bdr->XY[0]  = 2; 			bdr->XY[1]  = 0;
	bdr->XY[2]  = g->Width-2; 	bdr->XY[3]  = 0;
	bdr->XY[4]  = g->Width-2; 	bdr->XY[5]  = 1;
	bdr->XY[6]  = 2; 			bdr->XY[7]  = 1;
	bdr->XY[8]  = 1; 			bdr->XY[9]  = 2;
	bdr->XY[10] = 1; 			bdr->XY[11] = g->Height - 6;
	bdr->XY[12] = 0; 			bdr->XY[13] = g->Height - 6;
	bdr->XY[14] = 0; 			bdr->XY[15] = 2;

	bdr=bdr->NextBorder;
	if (!bdr){
		return;
	}

	// Shadow
	bdr->XY[0]  = 0; 			bdr->XY[1]  = g->Height - 5;
	bdr->XY[2]  = 1; 			bdr->XY[3]  = g->Height - 5;
	bdr->XY[4]  = 0; 			bdr->XY[5]  = g->Height - 4;
	bdr->XY[6]  = 2; 			bdr->XY[7]  = g->Height - 4;
	bdr->XY[8]  = 1; 			bdr->XY[9]  = g->Height - 3;
	bdr->XY[10] = g->Width;		bdr->XY[11] = g->Height - 3;
	bdr->XY[12] = g->Width;		bdr->XY[13] = g->Height - 2;
	bdr->XY[14] = 2; 			bdr->XY[15] = g->Height - 2;
	bdr->XY[16] = 3; 			bdr->XY[17] = g->Height - 1;
	bdr->XY[18] = g->Width;		bdr->XY[19] = g->Height - 1;
	bdr->XY[20] = g->Width;		bdr->XY[21] = g->Height - 0;
	bdr->XY[22] = 4;			bdr->XY[23] = g->Height - 0;
	bdr->XY[24] = g->Width;		bdr->XY[25] = g->Height - 3;
	bdr->XY[26] = g->Width;		bdr->XY[27] = 2;
	bdr->XY[28] = g->Width-1;	bdr->XY[29] = 2;
	bdr->XY[30] = g->Width-1;	bdr->XY[31] = g->Height - 4;
	bdr->XY[32] = g->Width-3;	bdr->XY[33] = g->Height - 4;
	bdr->XY[34] = g->Width-2;	bdr->XY[35] = g->Height - 5;
	
	bdr=bdr->NextBorder;
	if (!bdr){
		return;
	}

	// Face
	if (g->Height - 7 < maxFace){
		maxFace = g->Height - 7;
	}
	for (i=0; i < maxFace; i++){
		bdr->XY[pathi++]  = 2; 				bdr->XY[pathi++]  = 2+i;
		bdr->XY[pathi++]  = g->Width-2; 	bdr->XY[pathi++]  = 2+i;
	}
	
	bdr->XY[pathi++]  = 2; 				bdr->XY[pathi++]  = g->Height - 5;
	bdr->XY[pathi++]  = g->Width-3; 	bdr->XY[pathi++]  = g->Height - 5;
	bdr->XY[pathi++]  = g->Width-4;		bdr->XY[pathi++]  = g->Height - 4;
	bdr->XY[pathi++]  = 3;				bdr->XY[pathi++]  = g->Height - 4;
	bdr->Count = pathi / 2;
}

static struct RenderObject* createButtonIndicator(App *myApp)
{
	const UWORD borderCount = 5, totalXYShadow = 12, totalXYLight = 17, totalXYShine = 3, totalXYGlint = 2, totalXYLightBright = 4;
	const offsetY = 7, offsetX = 70;
	struct RenderObject *indicatorObj = NULL ;
	struct Border *indicatorArray = NULL;
	UWORD *paths, i = 0, j =0;
	LONG pen, penhi;
	struct DrawInfo *di = getScreenDrawInfo(myApp);
	
	if (!(indicatorObj = AllocVec(sizeof(struct RenderObject), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}
	if (!(indicatorObj->allocatedPens = AllocVec(sizeof(LONG) * borderCount, MEMF_ANY | MEMF_CLEAR))){
		FreeVec(indicatorObj);
		return NULL; // no memory
	}
	if (!(indicatorArray = AllocVec(sizeof(struct Border) * (borderCount-1), MEMF_ANY | MEMF_CLEAR))){
		FreeVec(indicatorObj->allocatedPens);
		FreeVec(indicatorObj);
		return NULL; // no memory
	}
	if (!(paths=AllocVec(sizeof(WORD)*((totalXYShadow*2)+(totalXYLight*2)+(totalXYShine*2)+(totalXYGlint*2)+(totalXYLightBright*2)), MEMF_ANY))){
		FreeVec(indicatorObj->allocatedPens);
		FreeVec(indicatorObj);
		FreeVec(indicatorArray);
		return NULL ; // no memory
	}
	
	indicatorObj->cm = myApp->appScreen->ViewPort.ColorMap;
	indicatorObj->allocatedPenCount = 0;
	for (i=0;i<borderCount;i++){
		indicatorObj->allocatedPens[i] = -1;
	}
	indicatorObj->head.NextBorder = indicatorArray;
	indicatorObj->tail = &indicatorArray[3];
	
	// Shadow fill
	indicatorObj->head.LeftEdge = offsetX;
	indicatorObj->head.TopEdge = offsetY;
	pen = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (pen < 0){
		pen = di->dri_Pens[SHADOWPEN] ; // see screens.h for all default pens
	}else{
		indicatorObj->allocatedPens[indicatorObj->allocatedPenCount++] = pen;
	}
	indicatorObj->head.FrontPen = pen ;
	indicatorObj->head.BackPen = 0;
	indicatorObj->head.DrawMode = JAM1;
	indicatorObj->head.Count = totalXYShadow;
	indicatorObj->head.XY = paths;
	indicatorObj->head.NextBorder = &indicatorArray[0];
	
	for (j=0,i=0;j<6;j++){
		//    X					Y
		paths[i++] = 0; 	paths[i++] = j;
		paths[i++] = 14; 	paths[i++] = j;
	}	
	paths += totalXYShadow*2;
	
	// Lighting fill - chequer pattern
	indicatorArray[0].LeftEdge = offsetX;
	indicatorArray[0].TopEdge = offsetY;
	pen = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0x40FFFFFF, 0x40FFFFFF, 0x40FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (pen < 0){
		pen = di->dri_Pens[FILLPEN] ; // see screens.h for all default pens
	}else{
		indicatorObj->allocatedPens[indicatorObj->allocatedPenCount++] = pen;
	}
	indicatorArray[0].FrontPen = pen ;
	indicatorArray[0].BackPen = 0;
	indicatorArray[0].DrawMode = JAM1;
	indicatorArray[0].Count = totalXYLight;
	indicatorArray[0].XY = paths;
	indicatorArray[0].NextBorder = &indicatorArray[1];
	
	i=0; // reset
	//    X					Y
	paths[i++] = 2; 	paths[i++] = 1;
	paths[i++] = 5; 	paths[i++] = 4;
	paths[i++] = 8; 	paths[i++] = 1;
	paths[i++] = 11; 	paths[i++] = 4;
	paths[i++] = 13; 	paths[i++] = 2;
	paths[i++] = 12; 	paths[i++] = 1;
	paths[i++] = 9; 	paths[i++] = 4;
	paths[i++] = 6; 	paths[i++] = 1;
	paths[i++] = 2; 	paths[i++] = 5;
	paths[i++] = 1; 	paths[i++] = 4;
	paths[i++] = 2; 	paths[i++] = 3;
	paths[i++] = 1; 	paths[i++] = 2;
	paths[i++] = 2; 	paths[i++] = 3; // Back up
	paths[i++] = 4; 	paths[i++] = 1;
	paths[i++] = 7; 	paths[i++] = 4;
	paths[i++] = 10; 	paths[i++] = 1;
	paths[i++] = 13; 	paths[i++] = 4;
	
	paths += totalXYLight*2;
	
	// Shine border
	indicatorArray[1].LeftEdge = offsetX;
	indicatorArray[1].TopEdge = offsetY;
	penhi = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0xC0FFFFFF, 0xC0FFFFFF, 0xC0FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (penhi < 0){
		penhi = di->dri_Pens[SHINEPEN] ; // see screens.h for all default pens
	}else{
		indicatorObj->allocatedPens[indicatorObj->allocatedPenCount++] = penhi;
	}
	indicatorArray[1].FrontPen = penhi ;
	indicatorArray[1].BackPen = 0;
	indicatorArray[1].DrawMode = JAM1;
	indicatorArray[1].Count = totalXYShine;
	indicatorArray[1].XY = paths;
	indicatorArray[1].NextBorder = &indicatorArray[2];
	
	i=0; // reset
	//    X					Y
	paths[i++] = 1; 	paths[i++] = 5;
	paths[i++] = 14; 	paths[i++] = 5;
	paths[i++] = 14; 	paths[i++] = 1;
	
	paths += totalXYShine*2;
	
	// Glint border
	indicatorArray[2].LeftEdge = offsetX;
	indicatorArray[2].TopEdge = offsetY;
	indicatorArray[2].FrontPen = penhi ;
	indicatorArray[2].BackPen = 0;
	indicatorArray[2].DrawMode = JAM1;
	indicatorArray[2].Count = totalXYGlint;
	indicatorArray[2].XY = paths;
	indicatorArray[2].NextBorder = &indicatorArray[3];
	
	i=0; // reset
	//    X					Y
	paths[i++] = 10; 	paths[i++] = 3;
	paths[i++] = 12; 	paths[i++] = 3;
	
	paths += totalXYGlint*2;
	
	// Lit border
	indicatorArray[3].LeftEdge = offsetX;
	indicatorArray[3].TopEdge = offsetY;
	pen = ObtainBestPen(myApp->appScreen->ViewPort.ColorMap, 0xFFFFFFFF, 0x00FFFFFF, 0x00FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (pen < 0){
		pen = di->dri_Pens[HIGHLIGHTTEXTPEN] ; // see screens.h for all default pens
	}else{
		indicatorObj->allocatedPens[indicatorObj->allocatedPenCount++] = pen;
	}
	indicatorArray[3].FrontPen = pen ;
	indicatorArray[3].BackPen = 0;
	indicatorArray[3].DrawMode = JAM1;
	indicatorArray[3].Count = totalXYLightBright;
	indicatorArray[3].XY = paths;
	indicatorArray[3].NextBorder = NULL;
	
	i=0; // reset
	//    X					Y
	paths[i++] = 6; 	paths[i++] = 2;
	paths[i++] = 10; 	paths[i++] = 2;
	paths[i++] = 9; 	paths[i++] = 3;
	paths[i++] = 5; 	paths[i++] = 3;
	
	paths += totalXYLightBright*2;
	
	return indicatorObj;
}

static struct RenderObject* createButtonRender(App *myApp, AppGadget *gad)
{
	const UWORD borderCount = 4, totalBGFill = 8, totalXYHighlight = 8, totalXYShadow = 18, totalXYFace = 74;
	UWORD *paths, i=0;
	LONG pen, penfill ;
	struct Border *borderArray = NULL;
	struct RenderObject *btnObj  = NULL ;
	struct DrawInfo *di = getScreenDrawInfo(myApp);
	
	if (!(btnObj = AllocVec(sizeof(struct RenderObject), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}
	if (!(btnObj->allocatedPens = AllocVec(sizeof(LONG) * borderCount, MEMF_ANY | MEMF_CLEAR))){
		FreeVec(btnObj);
		return NULL; // no memory
	}
	if (!(borderArray = AllocVec(sizeof(struct Border) * (borderCount-1), MEMF_ANY | MEMF_CLEAR))){
		FreeVec(btnObj->allocatedPens);
		FreeVec(btnObj);
		return NULL;
	}
	if (!(paths=AllocVec(sizeof(WORD)*((totalBGFill*2)+(totalXYHighlight*2)+(totalXYShadow*2)+(totalXYFace*2)), MEMF_ANY))){
		FreeVec(btnObj->allocatedPens);
		FreeVec(btnObj);
		FreeVec(borderArray);
		return NULL ; // no memory
	}
	
	btnObj->cm = myApp->appScreen->ViewPort.ColorMap;
	
	btnObj->allocatedPenCount = 0;
	for (i=0;i<borderCount;i++){
		btnObj->allocatedPens[i] = -1;
	}
	btnObj->tail = &borderArray[2];
	
	// BG Fill
	btnObj->head.LeftEdge = 0;
	btnObj->head.TopEdge = 0;
	penfill = ObtainBestPen(btnObj->cm, 0x40FFFFFF, 0x40FFFFFF, 0x40FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (penfill < 0){
		penfill = di->dri_Pens[FILLPEN] ; // see screens.h for all default pens
	}else{
		btnObj->allocatedPens[btnObj->allocatedPenCount++] = penfill;
	}
	btnObj->head.FrontPen = penfill ;
	btnObj->head.BackPen = 0;
	btnObj->head.DrawMode = JAM1;
	btnObj->head.Count = totalBGFill;
	btnObj->head.XY = paths;
	btnObj->head.NextBorder = &borderArray[0];
	paths += totalBGFill*2;
	
	// Highlights
	borderArray[0].LeftEdge = 0;
	borderArray[0].TopEdge = 0;
	pen = ObtainBestPen(btnObj->cm, 0xC0FFFFFF, 0xC0FFFFFF, 0xC0FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (pen < 0){
		pen = di->dri_Pens[SHINEPEN] ; // see screens.h for all default pens
	}else{
		btnObj->allocatedPens[btnObj->allocatedPenCount++] = pen;
	}
	borderArray[0].FrontPen = pen ;
	borderArray[0].BackPen = 0;
	borderArray[0].DrawMode = JAM1;
	borderArray[0].Count = totalXYHighlight;
	borderArray[0].XY = paths;
	borderArray[0].NextBorder = &borderArray[1];
	paths += totalXYHighlight*2;
	
	// Shadow
	borderArray[1].LeftEdge = 0;
	borderArray[1].TopEdge = 0;
	pen = ObtainBestPen(btnObj->cm, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, OBP_Precision, PRECISION_EXACT, 0);
	if (pen < 0){
		pen = di->dri_Pens[SHADOWPEN] ; // see screens.h for all default pens
	}else{
		btnObj->allocatedPens[btnObj->allocatedPenCount++] = pen;
	}
	borderArray[1].FrontPen = pen ;
	borderArray[1].BackPen = 0;
	borderArray[1].DrawMode = JAM1;
	borderArray[1].Count = totalXYShadow;
	borderArray[1].XY = paths;
	borderArray[1].NextBorder = &borderArray[2];
	paths += totalXYShadow*2;
	
	// Face - must be last in list due to variable XY paths
	borderArray[2].LeftEdge = 0;
	borderArray[2].TopEdge = 0;
	borderArray[2].FrontPen = penfill;
	borderArray[2].BackPen = 0;
	borderArray[2].DrawMode = JAM1;
	borderArray[2].Count = totalXYFace;
	borderArray[2].XY = paths;
	borderArray[2].NextBorder = NULL;
	
	updateButtonRender(btnObj, gad);

	return btnObj;
}

static struct RenderObject* createIconPanel(LONG pen, LONG width)
{
	const UWORD totalXYPanel = 28;
	const offsetY = 0, offsetX = 0;
	struct RenderObject *robj = NULL ;
	UWORD *paths, i = 0, j = 0;
	
	if (!(robj = AllocVec(sizeof(struct RenderObject), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}

	if (!(paths=AllocVec(sizeof(WORD)*(totalXYPanel*2), MEMF_ANY))){
		FreeVec(robj);
		return NULL ; // no memory
	}
	
	robj->allocatedPenCount = 0;
	robj->tail = (struct Border *)robj;
	
	// Panel fill
	robj->head.LeftEdge = offsetX;
	robj->head.TopEdge = offsetY;
	robj->head.FrontPen = pen ;
	robj->head.BackPen = 0;
	robj->head.DrawMode = JAM1;
	robj->head.Count = totalXYPanel;
	robj->head.XY = paths;
	robj->head.NextBorder = NULL;
	
	//    X						Y
	paths[i++] = 8; 		paths[i++] = 0;
	paths[i++] = width-8; 	paths[i++] = 0;
	paths[i++] = width-8; 	paths[i++] = 1;
	paths[i++] = 8;		 	paths[i++] = 1;
	
	paths[i++] = width-6; 	paths[i++] = 2;
	paths[i++] = 6; 		paths[i++] = 2;
	paths[i++] = 6;		 	paths[i++] = 3;
	paths[i++] = width-6; 	paths[i++] = 3;
	
	paths[i++] = 4; 		paths[i++] = 4;
	paths[i++] = width-4;	paths[i++] = 4;
	paths[i++] = width-4;	paths[i++] = 5;
	paths[i++] = 4;			paths[i++] = 5;
	
	for (j=0; j < 8; j++){
		paths[i++] = width-2;	paths[i++] = 6+j;
		paths[i++] = 2;			paths[i++] = 6+j;
	}
	
	return robj;
}

static struct RenderObject* createAdmixIcon(LONG pen)
{
	const UWORD totalXYHPIcon = 54;
	const offsetY = 0, offsetX = 0;
	struct RenderObject *robj = NULL ;
	UWORD *paths, i = 0;
	
	if (!(robj = AllocVec(sizeof(struct RenderObject), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}

	if (!(paths=AllocVec(sizeof(WORD)*(totalXYHPIcon*2), MEMF_ANY))){
		FreeVec(robj);
		return NULL ; // no memory
	}
	
	robj->allocatedPenCount = 0;
	robj->tail = (struct Border *)robj;
	
	// Icon
	robj->head.LeftEdge = offsetX;
	robj->head.TopEdge = offsetY;
	robj->head.FrontPen = pen ;
	robj->head.BackPen = 0;
	robj->head.DrawMode = JAM1;
	robj->head.Count = totalXYHPIcon;
	robj->head.XY = paths;
	robj->head.NextBorder = NULL;
	
	//    X						Y
	paths[i++] = 3; 		paths[i++] = 0;
	paths[i++] = 16; 		paths[i++] = 0;
	paths[i++] = 16; 		paths[i++] = 1;
	paths[i++] = 3;		 	paths[i++] = 1;
	paths[i++] = 2;		 	paths[i++] = 2;
	paths[i++] = 2;		 	paths[i++] = 3;
	paths[i++] = 3;		 	paths[i++] = 4;
	paths[i++] = 0;		 	paths[i++] = 4;
	paths[i++] = 0;		 	paths[i++] = 5;
	paths[i++] = 3;		 	paths[i++] = 5;
	paths[i++] = 2;			paths[i++] = 6;
	paths[i++] = 2;			paths[i++] = 7;
	paths[i++] = 3;			paths[i++] = 8;
	paths[i++] = 3;			paths[i++] = 9;
	paths[i++] = 16;		paths[i++] = 9;
	paths[i++] = 16;		paths[i++] = 8;
	paths[i++] = 4;			paths[i++] = 8;
	paths[i++] = 7;			paths[i++] = 5;
	paths[i++] = 7;			paths[i++] = 4;
	paths[i++] = 5;			paths[i++] = 2;
	paths[i++] = 5;		paths[i++] = 3;
	paths[i++] = 6;		paths[i++] = 2;
	paths[i++] = 6;		paths[i++] = 7;
	paths[i++] = 5;		paths[i++] = 6;
	paths[i++] = 7;		paths[i++] = 8;
	paths[i++] = 16;	paths[i++] = 8;
	paths[i++] = 19;	paths[i++] = 5;
	paths[i++] = 19;	paths[i++] = 4;
	paths[i++] = 17;	paths[i++] = 2;
	paths[i++] = 17;	paths[i++] = 3;
	paths[i++] = 18;	paths[i++] = 2;
	paths[i++] = 18;	paths[i++] = 7;
	paths[i++] = 17;	paths[i++] = 6;
	paths[i++] = 19;	paths[i++] = 4;
	paths[i++] = 22;	paths[i++] = 4;
	paths[i++] = 22;	paths[i++] = 5;
	paths[i++] = 19;	paths[i++] = 5;
	paths[i++] = 22;	paths[i++] = 4;
	paths[i++] = 23;	paths[i++] = 3;
	paths[i++] = 26;	paths[i++] = 2;
	paths[i++] = 23;	paths[i++] = 2;
	paths[i++] = 26;	paths[i++] = 3;
	paths[i++] = 27;	paths[i++] = 4;
	paths[i++] = 28;	paths[i++] = 4;
	paths[i++] = 27;	paths[i++] = 5;
	paths[i++] = 28;	paths[i++] = 5;
	paths[i++] = 29;	paths[i++] = 6;
	paths[i++] = 32;	paths[i++] = 7;
	paths[i++] = 29	;	paths[i++] = 7;
	paths[i++] = 32	;	paths[i++] = 6;
	paths[i++] = 33	;	paths[i++] = 5;
	paths[i++] = 34	;	paths[i++] = 4;
	paths[i++] = 33	;	paths[i++] = 4;
	paths[i++] = 34	;	paths[i++] = 5;
	return robj;
}

static struct RenderObject* createHeadphoneIcon(LONG pen)
{
	const UWORD totalXYHPIcon = 24;
	const offsetY = 0, offsetX = 0;
	struct RenderObject *robj = NULL ;
	UWORD *paths, i = 0;
	
	if (!(robj = AllocVec(sizeof(struct RenderObject), MEMF_ANY | MEMF_CLEAR))){
		return NULL; // no memory
	}

	if (!(paths=AllocVec(sizeof(WORD)*(totalXYHPIcon*2), MEMF_ANY))){
		FreeVec(robj);
		return NULL ; // no memory
	}
	
	robj->allocatedPenCount = 0;
	robj->tail = (struct Border *)robj;
	
	// Icon
	robj->head.LeftEdge = offsetX;
	robj->head.TopEdge = offsetY;
	robj->head.FrontPen = pen ;
	robj->head.BackPen = 0;
	robj->head.DrawMode = JAM1;
	robj->head.Count = totalXYHPIcon;
	robj->head.XY = paths;
	robj->head.NextBorder = NULL;
	
	//    X						Y
	paths[i++] = 0; 		paths[i++] = 4;
	paths[i++] = 0; 		paths[i++] = 7;
	paths[i++] = 1; 		paths[i++] = 7;
	paths[i++] = 1;		 	paths[i++] = 4;
	paths[i++] = 2;		 	paths[i++] = 3;
	paths[i++] = 2;		 	paths[i++] = 9;
	paths[i++] = 3;		 	paths[i++] = 9;
	paths[i++] = 3;		 	paths[i++] = 3;
	paths[i++] = 2;		 	paths[i++] = 2;
	paths[i++] = 3;		 	paths[i++] = 2;
	
	paths[i++] = 4;		 	paths[i++] = 1;
	paths[i++] = 9;		 	paths[i++] = 1;
	paths[i++] = 4;		 	paths[i++] = 0;
	paths[i++] = 9;		 	paths[i++] = 0;
	paths[i++] = 9;		 	paths[i++] = 1;
	
	paths[i++] = 10;		paths[i++] = 2;
	paths[i++] = 10;		paths[i++] = 9;
	paths[i++] = 11;		paths[i++] = 9;
	paths[i++] = 11;		paths[i++] = 2;
	paths[i++] = 11;		paths[i++] = 3;
	paths[i++] = 12;		paths[i++] = 4;
	paths[i++] = 12;		paths[i++] = 7;
	paths[i++] = 13;		paths[i++] = 7;
	paths[i++] = 13;		paths[i++] = 4;
	
	return robj;
}

// Adds first and second together - doesn't check if first has linked items and will break that chain (doesn't insert)
static void addTailRenderObject(struct RenderObject *first, struct RenderObject *second)
{
	if (first && second){
		first->next = second; // link the render objects
		first->tail->NextBorder = (struct Border*)&second->head;
		second->prev = first;
	}
}

static void detachTailRenderObject(struct RenderObject *obj)
{
	if (obj){
		if (obj->next){
			obj->next->prev = NULL;
			obj->next = NULL;
		}
		if (obj->tail){
			obj->tail->NextBorder = NULL;
		}
	}
}

static void removeRenderObject(struct RenderObject *obj)
{
	if (obj){
		if (obj->prev){
			if (obj->prev->tail){// should have a tail to connect but check anyway
				// Link NextBorder to list of next render object, if it exists
				obj->prev->tail->NextBorder = obj->next?(struct Border*)&obj->next->head:NULL;
			}
			obj->prev->next = obj->next;
		}
		if (obj->next){
			obj->next->prev = obj->prev;
		}
		obj->next = NULL;
		obj->prev = NULL;
	}
}

static void freeRenderObject(struct RenderObject *obj)
{
	struct Border *b, *bnext;
	UWORD i =0;
	
	if (obj){
		removeRenderObject(obj); // take out of any linked list
		if (!obj->isClone && obj->head.XY){
			FreeVec(obj->head.XY) ; // this frees all XYs
		}
		// Clear attributes and free memory
		if (obj->tail){
			b=(struct Border*)obj;
			while(b && b != obj->tail->NextBorder){
				bnext = b->NextBorder ;
				b->XY=NULL;
				b->Count = 0;
				b = bnext;
			}
			if(obj->head.NextBorder){ // All borders allocated to first border from head
				FreeVec(obj->head.NextBorder);
			}
		}
		if (!obj->isClone && obj->allocatedPens){
			if(obj->cm){
				for(i=0;i<obj->allocatedPenCount;i++){
					ReleasePen(obj->cm, obj->allocatedPens[i]);
				}
			}
			FreeVec(obj->allocatedPens);
		}
		obj->allocatedPens = NULL ;
		obj->allocatedPenCount = 0;
		obj->tail = NULL ;
		
		FreeVec(obj);
	}
}

static struct RenderObject *cloneRenderObject(struct RenderObject *obj, WORD x, WORD y)
{
	// Clone an existing render object
	struct RenderObject *clone = NULL ;
	struct Border *b = NULL, *newborders = NULL, *new = NULL, *prev = NULL;
	UWORD count = 0, i=0;
	
	if (!obj || !obj->tail){
		return NULL;
	}
	
	if (!(clone = AllocVec(sizeof(struct RenderObject), MEMF_ANY|MEMF_CLEAR))){
		return NULL; // no memory
	}
	
	*clone = *obj ; // shallow copy render object
	clone->isClone = TRUE ;
	clone->head.NextBorder = NULL ;
	clone->tail = (struct Border *)clone; // point to self
	clone->prev = clone->next = NULL;
		
	// How many borders are there?
	for (b=(struct Border *)obj; b != obj->tail->NextBorder; b = b->NextBorder){
		++count;
	}
	// Allocate all apart from RenderObject which has already been allocated
	if (!(newborders=AllocVec(sizeof(struct Border)*(count-1), MEMF_ANY|MEMF_CLEAR))){
		FreeVec(clone);
		return NULL;
	}

	for (b=(struct Border *)obj; b != obj->tail->NextBorder; b = b->NextBorder){
		if (b == (struct Border*)obj){ // if this is the RenderObject then copy to the head
			new = (struct Border *)clone;
		}else{
			new = &newborders[i++];
		}
		*new = *b; // shallow copy of border info
		new->LeftEdge += x;
		new->TopEdge += y;
		new->NextBorder = NULL ; // set to null incase this is the last border item
		clone->tail = new; // move this on to the newborders 'new' tail with each iteration
		if (prev){ // Set next border from previous border object (if there is a previous)
			prev->NextBorder = new;
		}
		prev = new;
	}
	
	return clone;
}

static void setRenderObjectOffset(struct RenderObject *obj, WORD x, WORD y)
{
	struct Border *b = NULL ;
	for (b=(struct Border *)obj; b != obj->tail->NextBorder; b = b->NextBorder){
		b->LeftEdge += x;
		b->TopEdge += y;
	}
}

static void updateHeadphoneIndicator(struct MHIVisAppData *appData)
{
	const UWORD totalXYLightBright = 4; // needs to match whatever the count of light bright paths are
	LONG onPen = 0, offPen = 1;
	UWORD i =0;
	
	onPen = appData->customButtonIndicatorRender[0]->tail->FrontPen ;
	offPen = appData->customButtonRender->head.FrontPen ;
	
	for (i=0; i < 3; i++){
		appData->customButtonIndicatorRender[i]->tail->Count = (appData->headphoneMode > i)?totalXYLightBright:0;
		appData->customButtonIndicatorRender[i]->head.NextBorder->FrontPen = (appData->headphoneMode > i)?onPen:offPen;
	}
	refreshAppGadget(&btnHeadphone);
}

static void updateAdmixIndicator(struct MHIVisAppData *appData)
{
	const UWORD totalXYLightBright = 4; // needs to match whatever the count of light bright paths are
	LONG onPen = 0, offPen = 1;
	
	onPen = appData->customButtonIndicatorAdmix->tail->FrontPen ;
	offPen = appData->customButtonRenderAdmix->head.FrontPen ;
	
	appData->customButtonIndicatorAdmix->tail->Count = (appData->adMixOn)?totalXYLightBright:0;
	appData->customButtonIndicatorAdmix->head.NextBorder->FrontPen = (appData->adMixOn)?onPen:offPen;
	refreshAppGadget(&btnAdmix);
}

void btnAdmixClicked(AppGadget *g, struct IntuiMessage *m)
{
	struct MHIVisAppData *appData = g->data;
		
	if (!appData->initialisedMHI){
		appData->adMixOn = FALSE; // no MHI so set to zero
	}else{
		appData->adMixOn = !appData->adMixOn;
		// update to the actual from driver
		appData->adMixOn = (UBYTE)doAdmixSetting(appData, TRUE, appData->adMixOn);
	}
	
	updateAdmixIndicator(appData);
}

void btnHeadphoneClicked(AppGadget *g, struct IntuiMessage *m)
{
	UBYTE oldMode ;
	struct MHIVisAppData *appData = g->data;
	
	oldMode = appData->headphoneMode;
	
	if (!appData->initialisedMHI){
		appData->headphoneMode = 0; // no MHI so set to zero
	}else{
		appData->headphoneMode = (appData->headphoneMode + 1) & 0x03; // increase mode
		// update to the actual from driver
		appData->headphoneMode = (UBYTE)doHeadphoneSetting(appData, appData->headphoneMode);
	}
	
	// Only update if changed
	if (oldMode != appData->headphoneMode){
		updateHeadphoneIndicator(appData);
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
	
	btnHeadphone.data = &appData;
	addAppGadget(appWnd, &btnHeadphone) ;
	btnHeadphone.fn_gadgetUp = btnHeadphoneClicked;
	
	btnAdmix.data = &appData;
	addAppGadget(appWnd, &btnAdmix) ;
	btnAdmix.fn_gadgetUp = btnAdmixClicked;
    
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