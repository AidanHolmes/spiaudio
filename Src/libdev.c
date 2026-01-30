#include <exec/types.h>
#include <exec/resident.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include "libdev.h"

/*
#define OFFSET(struct_name, struct_field) \
   ((ULONG)(&(((struct struct_name *)0)->struct_field)))
*/
/* Use the following macros in the structure definition */

#define INITBYTEDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o1; UBYTE name ## _o2; \
   UBYTE name ## _o3; UBYTE name ## _v; UBYTE name ## _p
#define INITWORDDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o1; UBYTE name ## _o2; \
   UWORD name ## _v
#define INITLONGDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o1; UBYTE name ## _o2; \
   ULONG name ## _v
#define INITPINTDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o1; UBYTE name ## _o2; \
   ULONG name ## _v

#define SMALLINITBYTEDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o; UBYTE name ## _v; UBYTE name ## _p
#define SMALLINITWORDDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o; UWORD name ## _v
#define SMALLINITLONGDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o; ULONG name ## _v
#define SMALLINITPINTDEF(name) \
   UBYTE name ## _c; UBYTE name ## _o; ULONG name ## _v

#define INITENDDEF UBYTE the_end

/* Use the following macros to fill in a structure */

#define NEWINITBYTE(offset, value) \
   0xe0, (UBYTE)((offset) >> 16), (UBYTE)((offset) >> 8), (UBYTE)(offset), \
   (UBYTE)(value), 0
#define NEWINITWORD(offset, value) \
   0xd0, (UBYTE)((offset) >> 16), (UBYTE)((offset) >> 8), (UBYTE)(offset), \
   (UWORD)(value)
#define NEWINITLONG(offset, value) \
   0xc0, (UBYTE)((offset) >> 16), (UBYTE)((offset) >> 8), (UBYTE)(offset), \
   (ULONG)(value)
#define INITPINT(offset, value) \
   0xc0, (UBYTE)((offset) >> 16), (UBYTE)((offset) >> 8), (UBYTE)(offset), \
   (ULONG)(value)

#define SMALLINITBYTE(offset, value) \
   0xa0, (offset), (UBYTE)(value), 0
#define SMALLINITWORD(offset, value) \
   0x90, (offset), (UWORD)(value)
#define SMALLINITLONG(offset, value) \
   0x80, (offset), (ULONG)(value)
#define SMALLINITPINT(offset, value) \
   0x80, (offset), (ULONG)(value)

#define INITEND 0

/* Obsolete definitions */
/*
#define INITBYTE(offset, value) \
   (0xe000 | ((UWORD)(offset) >> 16)), \
   (UWORD)(offset), (UWORD)((value) << 8)
#define INITWORD(offset, value) \
   (0xd000 | ((UWORD)(offset) >> 16)), (UWORD)(offset), (UWORD)(value)
#define INITLONG(offset, value) \
   (0xc000 | ((UWORD)(offset) >> 16)), (UWORD)(offset), \
   (UWORD)((value) >> 16), (UWORD)(value)
#define INITAPTR(offset, value) \
   (0xc000 | ((UWORD)(offset) >> 16)), (UWORD)(offset), \
   (UWORD)((ULONG)(value) >> 16), (UWORD)(value)

#endif
*/

#define _STR(A) #A
#define STR(A) _STR(A)

#ifdef AS_LIBRARY
__SAVE_DS__ __ASM__ static struct LibDevBase*  library_open(__REG__(a6, struct LibDevBase *) base);
__SAVE_DS__ __ASM__ extern struct LibDevBase*  libdev_library_open(__REG__(a6, struct LibDevBase *) base);
#else
__SAVE_DS__ __ASM__ static int device_open(__REG__(a6, struct LibDevBase *) base, __REG__(d0, ULONG) unit, __REG__(d1, ULONG) flags, __REG__(a1, struct IORequest *) ior);
__SAVE_DS__ __ASM__ static APTR device_close(__REG__(a6, struct LibDevBase *) base, __REG__(a1, struct IORequest *) ior);
__SAVE_DS__ __ASM__ extern int libdev_device_open(__REG__(a6, struct LibDevBase *) base, __REG__(d0, ULONG) unit, __REG__(d1, ULONG) flags, __REG__(a1, struct IORequest *) ior);
__SAVE_DS__ __ASM__ extern void libdev_device_close(__REG__(a6, struct LibDevBase *) base, __REG__(a1, struct IORequest *) ior);
__SAVE_DS__ __ASM__ extern void beginio(__REG__(a6, struct LibDevBase *) base, __REG__(a1, struct IORequest *) ior);
__SAVE_DS__ __ASM__ extern void abortio(__REG__(a6, struct LibDevBase *) base, __REG__(a1, struct IORequest *) ior);
#endif

static void DeleteLibrary(struct LibDevBase *base);
__SAVE_DS__ __ASM__ static struct LibDevBase* library_init(__REG__(d0, struct LibDevBase *) lib_base, __REG__(a0, APTR) seg_list, __REG__(a6, struct LibDevBase *) base);
__SAVE_DS__ __ASM__ static APTR library_close(__REG__(a6, struct LibDevBase *) base);
__SAVE_DS__ __ASM__ static APTR library_expunge(__REG__(a6, struct LibDevBase *) base);
__SAVE_DS__ __ASM__ static APTR library_reserved(void){return NULL;}

__SAVE_DS__ __ASM__ extern struct LibDevBase* libdev_initalise(__REG__(a6, struct LibDevBase *)base) ;
__SAVE_DS__ __ASM__ extern void libdev_cleanup(__REG__(a6, struct LibDevBase *) base);

extern const APTR vectors[];
extern const APTR init_table[];

int main()
{
   return -1;
}

static const char library_name[] = STR(LIBDEVNAME);
static const char version_string[] =
  STR(LIBDEVNAME) " " STR(LIBDEVMAJOR) "." STR(LIBDEVMINOR) " (" STR(LIBDEVDATE) ")\n";
//static const char library_name[] = LIBDEVNAME;
//static const char version_string[] =
//  LIBDEVNAME " " STR(LIBDEVMAJOR) "." STR(LIBDEVMINOR) " (" STR(LIBDEVDATE) ")\n";

static const struct Resident rom_tag =
{
   RTC_MATCHWORD,
   (struct Resident *)&rom_tag,
   (APTR)init_table,
   RTF_AUTOINIT,
   LIBDEVMAJOR,
#ifdef AS_LIBRARY
   NT_LIBRARY,
#else
   NT_DEVICE,
#endif
   0,
   (STRPTR)library_name,
   (STRPTR)version_string,
   (APTR)init_table
};

const APTR vectors[] =
{
#ifdef AS_LIBRARY
   (APTR)library_open,
   (APTR)library_close,
#else
   (APTR)device_open,
   (APTR)device_close,
#endif
   (APTR)library_expunge,
   (APTR)library_reserved,
#ifndef AS_LIBRARY
   (APTR)beginio,
   (APTR)abortio,
#endif
/* Add all library functions here */
	(APTR)MHIAllocDecoder,
	(APTR)MHIFreeDecoder,
	(APTR)MHIQueueBuffer,
	(APTR)MHIGetEmpty,
	(APTR)MHIGetStatus,
	(APTR)MHIPlay,
	(APTR)MHIStop,
	(APTR)MHIPause,
	(APTR)MHIQuery,
	(APTR)MHISetParam,
/* End of custom library functions */
   (APTR)-1
};


static const struct _initdata
{
	SMALLINITBYTEDEF(type);
	SMALLINITPINTDEF(name);
	SMALLINITBYTEDEF(flags);
	SMALLINITWORDDEF(version);
	SMALLINITWORDDEF(revision);
	SMALLINITPINTDEF(id_string);
	INITENDDEF;
} init_data =
{
#ifdef AS_LIBRARY
#ifdef __VBCC__
	SMALLINITBYTE(8, NT_LIBRARY),
#else
	SMALLINITBYTE(OFFSET(Node, ln_Type), NT_LIBRARY),
#endif
#else
#ifdef __VBCC__
	SMALLINITBYTE(8, NT_DEVICE),
#else
	SMALLINITBYTE(OFFSET(Node, ln_Type), NT_DEVICE),
#endif
#endif
#ifdef __VBCC__
	SMALLINITPINT(10, library_name),
	SMALLINITBYTE(14, LIBF_SUMUSED|LIBF_CHANGED),
	SMALLINITWORD(20, LIBDEVMAJOR),
	SMALLINITWORD(22, LIBDEVMINOR),
	SMALLINITPINT(24, version_string),
#else
	SMALLINITPINT(OFFSET(Node, ln_Name), library_name),
	SMALLINITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED|LIBF_CHANGED),
	SMALLINITWORD(OFFSET(Library, lib_Version), LIBDEVMAJOR),
	SMALLINITWORD(OFFSET(Library, lib_Revision), LIBDEVMINOR),
	SMALLINITPINT(OFFSET(Library, lib_IdString), version_string),
#endif
	INITEND
};


const APTR init_table[] =
{
	(APTR)sizeof(struct LibDevBase),
	(APTR)vectors,
	(APTR)&init_data,
	(APTR)library_init
};

__SAVE_DS__ __ASM__ static struct LibDevBase* library_init(__REG__(d0, struct LibDevBase *) lib_base, __REG__(a0, APTR) seg_list, __REG__(a6, struct LibDevBase *) base)
{
	lib_base->sys_base = (APTR)base;
	base = lib_base;
	base->seg_list = seg_list;
	
	base = libdev_initalise(base);	// External function

	return base;
}
#ifdef AS_LIBRARY
__SAVE_DS__ __ASM__ static struct LibDevBase* library_open(__REG__(a6, struct LibDevBase *) base)
{	
	if (!libdev_library_open(base)){
		return NULL;
	}
	
	base->device.dd_Library.lib_OpenCnt++;
	base->device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

	return base;
}
#else
__SAVE_DS__ __ASM__ static int device_open(__REG__(a6,struct LibDevBase*) base, __REG__(d0,ULONG) unit, __REG__(d1,ULONG) flags, __REG__(a1, struct IORequest*) ior)
{
	int ret = 0;
	struct ExecBase *eb = *(struct ExecBase **)4;
	
	// Validate the exec version if certain
	// supported library calls expected. In this case a default V36 exec functions assumed (not 1.3 OS driver) unless LIBDEV_VALIDATE_EXEC changed or undefined
#ifdef LIBDEV_VALIDATE_EXEC
	if (eb->LibNode.lib_Version < LIBDEV_VALIDATE_EXEC){
		ior->io_Error = IOERR_OPENFAIL;
        return 1; // Fail
	}
#endif
	
	ret = libdev_device_open(base, unit, flags, ior);
	if (ret > 0){
		return ret ;
	}
	base->device.dd_Library.lib_OpenCnt++;
	base->device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

	return 0;
}

__SAVE_DS__ __ASM__ static APTR device_close(__REG__(a6, struct LibDevBase*) base, __REG__(a1, struct IORequest*) ior)
{
	libdev_device_close(base, ior);

	return library_close(base);
}

#endif

__SAVE_DS__ __ASM__ static APTR library_close(__REG__(a6, struct LibDevBase*) base)
{
	APTR seg_list = NULL;

	/* Expunge the library if a delayed expunge is pending */

	if((--base->device.dd_Library.lib_OpenCnt) == 0)
	{
		if((base->device.dd_Library.lib_Flags & LIBF_DELEXP) != 0){
			seg_list = library_expunge(base);
		}
	}

	return seg_list;
}

__SAVE_DS__ __ASM__ static APTR library_expunge(__REG__(a6, struct LibDevBase*) base)
{
	APTR seg_list;

	if(base->device.dd_Library.lib_OpenCnt == 0)
	{
		libdev_cleanup(base);
		seg_list = base->seg_list;
		Remove((APTR)base);
		DeleteLibrary(base);
	}
	else
	{
		base->device.dd_Library.lib_Flags |= LIBF_DELEXP;
		seg_list = NULL;
	}

	return seg_list;
}

static void DeleteLibrary(struct LibDevBase *base)
{
   UWORD neg_size, pos_size;

   /* Free library's memory */

   neg_size = base->device.dd_Library.lib_NegSize;
   pos_size = base->device.dd_Library.lib_PosSize;
   FreeMem((UBYTE *)base - neg_size, pos_size + neg_size);

   return;
}
