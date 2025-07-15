#include "Burger.h"
#include "res.h"
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <SDL3/SDL.h>

typedef uint32_t __attribute__((aligned(1), may_alias)) u_uint32_t;

typedef struct {
	void *data;
} Resource;


static const char *MainResourceFile = "data/Wolf3D.rsrc";
static char *LevelsPath = "data/Levels/_Second_Encounter_(30_Levels).rsrc";
static const uint32_t ResType = 0x42524752; /* BRGR */
static const uint32_t CsndType = 0x63736E64; /* csnd */
static RFILE *MainResources = NULL;
static Resource *ResourceCache = NULL;
static RFILE *LevelResources = NULL;
static Resource *LevelResourceCache = NULL;
static LongWord NumSounds = 0;
static Sound *SoundCache = NULL;

static RFILE *LoadResources(const char *Filename, Resource **cache_out) {
	size_t Count;
	RFILE *Rp;
	Resource *Cache;

	Rp = res_open(Filename, 0);
	if (!Rp)
		return NULL;
	Count = res_count(Rp, ResType);
	Cache = SDL_calloc(Count, sizeof(Resource));
	if (!Cache) {
		res_close(Rp);
		return NULL;
	}
	*cache_out = Cache;
	return Rp;
}

void InitResources(void)
{
	if (MainResources)
		return;
	MainResources = LoadResources(MainResourceFile, &ResourceCache);
	if (!MainResources)
		err(1, "%s", MainResourceFile);
}

Boolean LoadLevelResources(void)
{
	if (LevelResources) {
		uint32_t Count = res_count(LevelResources, ResType);
		res_close(LevelResources);
		for (int i = 0; i < Count; i++)
			if (LevelResourceCache[i].data)
				SDL_free(LevelResourceCache[i].data);
		SDL_free(ResourceCache);
		ResourceCache = NULL;
	}
	LevelResources = LoadResources(LevelsPath, &LevelResourceCache);
	return LevelResources != NULL;
}

static Resource *GetResource2(Word RezNum, RFILE *Rp, Resource *Cache)
{
	ResAttr Attr;
	void *Data;

	if (res_attr(Rp, ResType, RezNum, &Attr) == NULL)
		return NULL;
	if (Cache[Attr.index].data)
		return Cache[Attr.index].data;
	Data = SDL_malloc(Attr.size);
	if (!Data)
		return NULL;
	if (!res_read_ind(Rp, ResType, Attr.index, Data, 0, Attr.size, NULL, NULL)) {
		SDL_free(Data);
		return NULL;
	}
	Cache[Attr.index].data = Data;
	return Data;
}

static Resource *GetResource(Word RezNum)
{
	Resource *Res;
	Res = GetResource2(RezNum, LevelResources, LevelResourceCache);
	if (Res)
		return Res;
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
	if (res_attr(MainResources, ResType, RezNum, &attr))
		return attr.size;
	return 0;
}

/**********************************

	Allow a resource to be purged

**********************************/

void ReleaseAResource(Word RezNum)
{
	ResAttr attr;
	if (res_attr(LevelResources, ResType, RezNum, &attr)) {
		if (LevelResourceCache[attr.index].data) {
			SDL_free(LevelResourceCache[attr.index].data);
			LevelResourceCache[attr.index].data = NULL;
		}
	} else if (res_attr(MainResources, ResType, RezNum, &attr)) {
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
