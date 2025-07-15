#include "SDLWolf.h"
#include <string.h>
#include <err.h>

typedef uint32_t __attribute__((aligned(1), may_alias)) u_uint32_t;
typedef uint16_t __attribute__((aligned(1), may_alias)) u_uint16_t;

typedef struct {
	void *data;
} Resource;

static const char *MainResourceFile = "Wolf3D.rsrc";
static const char *LevelsFolder = "Levels/";
static const char *DefaultLevelsPath = "Levels/_Second_Encounter_(30_Levels).rsrc";

static const uint32_t BrgrType = 0x42524752; /* BRGR */
static const uint32_t PictType = 0x50494354; /* PICT */
static const uint32_t CsndType = 0x63736E64; /* csnd */

RFILE *MainResources = NULL;
static Resource *ResourceCache = NULL;
static RFILE *LevelResources = NULL;
static Resource *LevelResourceCache = NULL;
static LongWord NumSounds = 0;
static Sound *SoundCache = NULL;

static RFILE *LoadResources(const char *FileName, Resource **CacheOut) {
	size_t Count;
	RFILE *Rp;
	Resource *Cache;

	Rp = res_open(FileName, 0);
	if (!Rp)
		return NULL;
	Count = res_count(Rp, BrgrType);
	Cache = SDL_calloc(Count, sizeof(Resource));
	if (!Cache) {
		res_close(Rp);
		return NULL;
	}
	*CacheOut = Cache;
	return Rp;
}

static void ReleaseResources(RFILE **Rp, Resource **Cache)
{
	if (*Rp) {
		uint32_t Count = res_count((*Rp), BrgrType);
		res_close((*Rp));
		*Rp = NULL;
		if (*Cache) {
			for (int i = 0; i < Count; i++)
				if ((*Cache)[i].data)
					SDL_free((*Cache)[i].data);
			SDL_free(*Cache);
			*Cache = NULL;
		}
	}
}

void InitResources(void)
{
	const char *BasePath;
	if (MainResources)
		return;
	BasePath = PrefPath();
	char TmpPath[strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1];
	stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
	MainResources = LoadResources(TmpPath, &ResourceCache);
	if (!MainResources)
		err(1, "%s", TmpPath);
}


void KillResources(void)
{
	ReleaseResources(&MainResources, &ResourceCache);
	ReleaseResources(&LevelResources, &LevelResourceCache);
}

Boolean MountMapFileAbsolute(const char *FileName)
{
	ReleaseResources(&LevelResources, &LevelResourceCache);
	LevelResources = LoadResources(FileName, &LevelResourceCache);
	if (LevelResources == NULL)
		warn("MountMapFile: %s", FileName);
	return LevelResources != NULL;
}

Boolean MountMapFile(const char *FileName)
{
	const char *BasePath;

	if (!FileName)
		FileName = DefaultLevelsPath;
	BasePath = PrefPath();
	char TmpPath[strlen(BasePath) + __builtin_strlen(LevelsFolder) + strlen(FileName) + 1];
	stpcpy(stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder), FileName);
	return MountMapFileAbsolute(TmpPath);
}

bool EnumerateLevels(SDL_EnumerateDirectoryCallback callback, void *userdata)
{
	const char *BasePath;
	BasePath = PrefPath();
	char TmpPath[strlen(BasePath) + __builtin_strlen(LevelsFolder) + 1];
	stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder);
	return SDL_EnumerateDirectory(TmpPath, callback, userdata);
}

static Resource *GetResource2(Word RezNum, RFILE *Rp, Resource *Cache)
{
	ResAttr Attr;
	void *Data;

	if (res_attr(Rp, BrgrType, RezNum, &Attr) == NULL)
		return NULL;
	if (Cache[Attr.index].data)
		return Cache[Attr.index].data;
	Data = SDL_malloc(Attr.size);
	if (!Data)
		return NULL;
	if (!res_read_ind(Rp, BrgrType, Attr.index, Data, 0, Attr.size, NULL, NULL)) {
		SDL_free(Data);
		return NULL;
	}
	Cache[Attr.index].data = Data;
	return Data;
}

static Resource *GetResource(Word RezNum)
{
	Resource *Res;
	if (LevelResources) {
		Res = GetResource2(RezNum, LevelResources, LevelResourceCache);
		if (Res)
			return Res;
	}
	return GetResource2(RezNum, MainResources, ResourceCache);
}

/**********************************

	Load a personal resource

**********************************/

void *LoadAResource(Word RezNum)
{
	Resource *Res;

	Res = GetResource(RezNum);
	if (Res)
		return Res;
	return NULL;
}

LongWord ResourceLength(Word RezNum)
{
	ResAttr attr;
	if (LevelResources) {
		if (res_attr(LevelResources, BrgrType, RezNum, &attr))
			return attr.size;
	}
	if (res_attr(MainResources, BrgrType, RezNum, &attr))
		return attr.size;
	return 0;
}

/**********************************

	Allow a resource to be purged

**********************************/

void ReleaseAResource(Word RezNum)
{
	ResAttr attr;
	if (LevelResources && res_attr(LevelResources, BrgrType, RezNum, &attr)) {
		if (LevelResourceCache[attr.index].data) {
			SDL_free(LevelResourceCache[attr.index].data);
			LevelResourceCache[attr.index].data = NULL;
		}
	} else if (res_attr(MainResources, BrgrType, RezNum, &attr)) {
		if (ResourceCache[attr.index].data) {
			SDL_free(ResourceCache[attr.index].data);
			ResourceCache[attr.index].data = NULL;
		}
	}
}

/**********************************

	Decompress using LZSS

**********************************/

void DLZSS(Byte * restrict Dest,const Byte * restrict Src,LongWord Length)
{
	Word BitBucket;
	Word RunCount;
	Word Fun;
	Byte *BackPtr;

	if (!Length) {
		return;
	}
	BitBucket = (Word) Src[0] | 0x100;
	++Src;
	do {
		if (BitBucket&1) {
			Dest[0] = Src[0];
			++Src;
			++Dest;
			--Length;
		} else {
			RunCount = (Word) Src[0] | ((Word) Src[1]<<8);
			Fun = 0x1000-(RunCount&0xfff);
			BackPtr = Dest-Fun;
			RunCount = ((RunCount>>12) & 0x0f) + 3;
			if (Length >= RunCount) {
				Length -= RunCount;
			} else {
				Length = 0;
			}
			do {
				*Dest++ = *BackPtr++;
			} while (--RunCount);
			Src+=2;
		}
		if (Length == 0)
			break;
		BitBucket>>=1;
		if (BitBucket==1) {
			BitBucket = (Word)Src[0] | 0x100;
			++Src;
		}
	} while (1);
}

/**********************************

	Sound loading

**********************************/

static void SoundDLZSS(const Byte* Src, size_t SrcSize, Byte *Dst, size_t DstSize) {
	const Byte * const SrcEnd = Src + SrcSize;
	Byte * const DstStart = Dst;
	Byte * const DstEnd = Dst + DstSize;
	for (;;) {
		if (Src >= SrcEnd)
			break;
		uint8_t control_bits = *Src++;

		for (uint8_t control_mask = 0x01; control_mask; control_mask <<= 1) {
			if (control_bits & control_mask) {
				if (Src >= SrcEnd)
					break;
				if (Dst >= DstEnd)
					break;
				uint8_t c = *Src++;
				*Dst++ = c;

			} else {
				if (Src >= SrcEnd - 1)
					break;
				uint16_t params = (Src[0] << 8) | Src[1];
				Src += 2;

				size_t copy_offset = Dst - DstStart - ((1 << 12) - (params & 0x0FFF));
				uint8_t count = ((params >> 12) & 0x0F) + 3;
				size_t copy_end_offset = copy_offset + count;

				for (; copy_offset != copy_end_offset; copy_offset++) {
					if (Dst >= DstEnd)
						break;
					*Dst++ = DstStart[copy_offset];
				}
			}
		}
	}
}

Boolean DecodeCsnd(const Byte *Buf, LongWord Len, Sound *Snd)
{
	LongWord RealSize;
	Byte *Data;

	if (Len < 4)
		return 0;
	RealSize = SwapLongBE(*(const u_uint32_t*)Buf) & 0x00FFFFFF;
	Data = SDL_malloc(RealSize);
	if (!Data) return 0;
	SoundDLZSS(Buf + 4, Len - 4, Data, RealSize);
	uint8_t* Delta = Data;
	uint8_t* const DataEnd = Data + RealSize;
	for (uint8_t sample = *Delta++; Delta != DataEnd; Delta++) {
		*Delta = (sample += *Delta);
	}
	if (RealSize < 42) {
		SDL_free(Data);
		return 0;
	}
	/*
	Snd->base_note = Data[41] ? Data[41] : 0x3C;
	Snd->loop_start = SwapLongBE(*(u_uint32_t*)&Data[32]) >> 16;
	Snd->loop_end = SwapLongBE(*(u_uint32_t*)&Data[36]) >> 16;
	*/
	Snd->samplerate = SwapLongBE(*(u_uint32_t*)&Data[28]) >> 16;
	Snd->size = RealSize - 42;
	Snd->data = &Data[42];
	return 1;
}

void RegisterSounds(short *SoundIDs)
{
	ResAttr Attr;
	size_t Count = 0;
	size_t i;
	uint8_t *Buf;

	if (SoundCache) {
		for (i = 0; i < NumSounds; i++)
			if (SoundCache[i].data)
				SDL_free(SoundCache[i].data - 42);
		SDL_free(SoundCache);
		SoundCache = NULL;
		NumSounds = 0;
	}
	for (short *SoundID = SoundIDs; *SoundID != -1; SoundID++, Count++);
	SoundCache = SDL_calloc(Count, sizeof(Sound));
	NumSounds = Count;
	if (!SoundCache) err(1, "malloc");
	i = 0;
	for (short *SoundID = SoundIDs; *SoundID != -1; SoundID++, i++) {
		if (!res_attr(MainResources, CsndType, *SoundID, &Attr))
			continue;
		Buf = SDL_malloc(Attr.size);
		if (!Buf) err(1, "malloc");
		if (!res_read_ind(MainResources, CsndType, Attr.index, Buf, 0, Attr.size, NULL, NULL))
			goto Done;
		if (DecodeCsnd(Buf, Attr.size, &SoundCache[i]))
			SoundCache[i].ID = *SoundID;
	Done:
		SDL_free(Buf);
   }
}

static int SearchSounds(const void *a, const void *b) {
	int ua, ub;

	ua = (size_t)a;
	ub = *(const uint16_t *)b;
	return ua - ub;
}

Sound *LoadSound(Word RezNum)
{
	return bsearch((void*)(size_t)RezNum, SoundCache, NumSounds, sizeof(Sound), SearchSounds);
}

/**********************************

	Menus

**********************************/

static void FreePixels(void *userdata, void *value)
{
	SDL_free(value);
}

SDL_Surface *LoadPict(RFILE *Rp, Word PicNum)
{
	ResAttr Attr;
	Byte *Data;
	Byte *Pixels = NULL;
	Word Stride;
	Word W;
	Word H;
	const u_uint16_t *PalPtr;
	int i;
	int j;
	int y;
	SDL_Surface *Surface = NULL;
	SDL_Palette *Palette;
	SDL_Color Colors[256];

	/* Extremely limited support for RLE encoded quickdraw PICT files */
	if (!res_attr(Rp, PictType, PicNum, &Attr))
		return NULL;
	if (Attr.size < 0x87E)
		return NULL;
	Data = SDL_malloc(Attr.size);
	if (!Data) return NULL;
	if (!res_read_ind(Rp, PictType, Attr.index, Data, 0, Attr.size, NULL, NULL))
		goto Done;
	if (SwapUShortBE(*(const u_uint16_t*)&Data[0x34]) != 0x0098) /* check for RLE opcode */
		goto Done;

	Stride = SwapUShortBE(*(const u_uint16_t*)&Data[0x36]) & 0x7FFF;
	W = SwapUShortBE(*(const u_uint16_t*)&Data[0x8]) - SwapUShortBE(*(const u_uint16_t*)&Data[0x4]);
	H = SwapUShortBE(*(const u_uint16_t*)&Data[0x6]) - SwapUShortBE(*(const u_uint16_t*)&Data[0x2]);
	Pixels = SDL_malloc(Stride*H);
	if (!Pixels)
		goto Done;

	{
		const Byte *Packed;
		const Byte *PackedEnd;
		Byte *Pix;
		Byte *PixelsEnd;
		Byte *NextRow;
		Byte Value;
		Word PackedCount;
		short Count;

		Pix = Pixels;
		PixelsEnd = Pixels + Stride * H;
		Packed = &Data[0x87E];
		PackedEnd = Data + Attr.size;
		for (y = 0; y < H; y++) {
			NextRow = Pix + Stride;
			if (Stride > 200) {
				if (Packed >= PackedEnd - 1)
					break;
				PackedCount = (Packed[0]<<8)|Packed[1];
				Packed += 2;
			} else {
				if (Packed >= PackedEnd)
					break;
				PackedCount = *Packed++;
			}
			for (i = Packed - Data + PackedCount; Packed - Data < i;) {
				if (Packed >= PackedEnd)
					break;
				Count = (char)*Packed++;
				if (Count < 0) {			/* RLE */
					if (Packed >= PackedEnd)
						break;
					Value = *Packed++;
					for (j = 0; j < -Count + 1; j++) {
						if (Pix >= PixelsEnd)
							break;
						*Pix++ = Value;
					}
				} else {					/* Copy */
					for (j = 0; j < Count + 1; j++) {
						if (Packed >= PackedEnd)
							break;
						if (Pix >= PixelsEnd)
							break;
						*Pix++ = *Packed++;
					}
				}
			}
			if (Pix != NextRow)
				break;
		}
		if (Pix != PixelsEnd)
			goto Done;
	}

	Surface = SDL_CreateSurfaceFrom(W, H, SDL_PIXELFORMAT_INDEX8, Pixels, Stride);
	if (!Surface)
		goto Done;
	if (!SDL_SetPointerPropertyWithCleanup(SDL_GetSurfaceProperties(Surface), "Wolf.pixels", Pixels, FreePixels, NULL))
		goto Done;
	Pixels = NULL;
	Palette = SDL_CreateSurfacePalette(Surface);
	if (!Palette) {
		SDL_DestroySurface(Surface);
		Surface = NULL;
		goto Done;
	}
	PalPtr = (const u_uint16_t*)&Data[0x6E];

	i = 0;
	do {
		Colors[i].r = SwapUShortBE(PalPtr[i*4+0])/0x101;
		Colors[i].g = SwapUShortBE(PalPtr[i*4+1])/0x101;
		Colors[i].b = SwapUShortBE(PalPtr[i*4+2])/0x101;
		Colors[i].a = 255;
	} while (++i<255);
	SDL_SetPaletteColors(Palette, Colors, 0, 256);
Done:
	if (Pixels)
		SDL_free(Pixels);
	SDL_free(Data);
	return Surface;
}
