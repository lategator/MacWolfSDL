#include "WolfDef.h"
#include "SDLWolf.h"
#include <string.h>
#include <errno.h>

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#define TSF_MALLOC AllocSomeMem
#define TSF_REALLOC SDL_realloc
#define TSF_FREE FreeSomeMem
#define TSF_MEMCPY SDL_memcpy
#define TSF_MEMSET SDL_memset
#define TSF_POW SDL_pow
#define TSF_POWF SDL_powf
#define TSF_EXPF SDL_expf
#define TSF_LOG SDL_log
#define TSF_TAN SDL_tan
#define TSF_LOG10 SDL_log10
#define TSF_SQRTF SDL_sqrtf
#include "tsf.h"

typedef struct {
	void *data;
	uint32_t refcount;
} Resource;

static const char *MainResourceFile = "Wolf3D.rsrc";
static const char *LevelsFolder = "Levels/";
static const char *DefaultLevelsPath = "Levels/_Second_Encounter_(30_Levels).rsrc";

static const uint32_t BrgrType = 0x42524752; /* BRGR */
static const uint32_t PictType = 0x50494354; /* PICT */
static const uint32_t SndType  = 0x736e6420; /* snd  */
static const uint32_t CsndType = 0x63736E64; /* csnd */
static const uint32_t MidiType = 0x4d696469; /* Midi */
static const uint32_t InstType = 0x494E5354; /* INST */

RFILE *MainResources = NULL;
static Resource *ResourceCache = NULL;
static RFILE *LevelResources = NULL;
static Resource *LevelResourceCache = NULL;
static LongWord NumSounds = 0;
static Sound *SoundCache = NULL;
tsf *MusicSynth;

static void ReleaseSounds(void);

static RFILE *LoadResources(const char *FileName, Resource **CacheOut)
{
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
	{
		BasePath = PrefPath();
		char *TmpPath = AllocSomeMem(strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1);
		stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
		MainResources = LoadResources(TmpPath, &ResourceCache);
		if (!MainResources && errno != ENOENT)
			BailOut("%s: %s", TmpPath, strerror(errno));
		FreeSomeMem(TmpPath);
	}
	if (!MainResources) {
		BasePath = SDL_GetBasePath();
		char *TmpPath = AllocSomeMem(strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1);
		stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
		MainResources = LoadResources(TmpPath, &ResourceCache);
		FreeSomeMem(TmpPath);
	}
	if (!MainResources)
		BailOut("Resource file not found or not readable: '%s'\n\n"
				"This file contains the Wolfenstein 3D assets and is required to run the game.\n"
				"It must be copied from an installed executable of Wolfenstein 3D Third Encounter for Macintosh Classic.\n"
				"The file format can be any of: MacBinary II, AppleSingle, AppleDouble, or a raw resource fork.\n\n"
				"After extracting, it should be placed in the same folder as the executable, or in this folder:\n\n%s",
				MainResourceFile, PrefPath());
}

void KillResources(void)
{
	ReleaseSounds();
	ReleaseResources(&MainResources, &ResourceCache);
	ReleaseResources(&LevelResources, &LevelResourceCache);
}

Boolean MountMapFile(const char *FileName)
{
	if (MapListPtr) {
		MapListPtr = NULL;
		ReleaseAResource(rMapList);
	}
	if (SoundListPtr) {
		SoundListPtr = NULL;
		ReleaseAResource(MySoundList);
	}
	if (WallListPtr) {
		WallListPtr = NULL;
		ReleaseAResource(MyWallList);
	}
	if (SongListPtr) {
		SongListPtr = NULL;
		ReleaseAResource(rSongList);
	}
	ReleaseResources(&LevelResources, &LevelResourceCache);

	if (!FileName)
		FileName = DefaultLevelsPath;

	LevelResources = LoadResources(FileName, &LevelResourceCache);
	if (LevelResources == NULL)
		BailOut("MountMapFile: %s: %s", FileName, strerror(errno));
	return LevelResources != NULL;
}

void EnumerateLevels(SDL_EnumerateDirectoryCallback callback, void *userdata)
{
	const char *BasePath;
	{
		BasePath = PrefPath();
		char* TmpPath = AllocSomeMem(strlen(BasePath) + __builtin_strlen(LevelsFolder) + 1);
		stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder);
		SDL_EnumerateDirectory(TmpPath, callback, userdata);
		FreeSomeMem(TmpPath);
	}
	{
		BasePath = SDL_GetBasePath();
		char* TmpPath = AllocSomeMem(strlen(BasePath) + __builtin_strlen(LevelsFolder) + 1);
		stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder);
		SDL_EnumerateDirectory(TmpPath, callback, userdata);
		FreeSomeMem(TmpPath);
	}
}

static void *ReadResource(Word RezNum, LongWord Type, RFILE *Rp, LongWord *Len)
{
	ResAttr Attr;
	void *Data;

	if (res_attr(Rp, Type, RezNum, &Attr) == NULL)
		return NULL;
	Data = SDL_malloc(Attr.size);
	if (!Data)
		return NULL;
	if (!res_read_ind(Rp, Type, Attr.index, Data, 0, Attr.size, NULL, NULL)) {
		SDL_free(Data);
		return NULL;
	}
	if (Len)
		*Len = Attr.size;
	return Data;
}

static Resource *GetResource2(Word RezNum, RFILE *Rp, Resource *Cache)
{
	ResAttr Attr;
	void *Data;

	if (res_attr(Rp, BrgrType, RezNum, &Attr) == NULL)
		return NULL;
	if (Cache[Attr.index].data) {
		Cache[Attr.index].refcount++;
		return &Cache[Attr.index];
	}
	Data = SDL_malloc(Attr.size);
	if (!Data)
		return NULL;
	if (!res_read_ind(Rp, BrgrType, Attr.index, Data, 0, Attr.size, NULL, NULL)) {
		SDL_free(Data);
		return NULL;
	}
	Cache[Attr.index].data = Data;
	Cache[Attr.index].refcount = 1;
	return &Cache[Attr.index];
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
		return Res->data;
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

static Boolean UnrefResource(Word RezNum, RFILE *Rp, Resource *Cache)
{
	ResAttr attr;
	if (!res_attr(Rp, BrgrType, RezNum, &attr))
		return FALSE;
	if (Cache[attr.index].data) {
		Cache[attr.index].refcount--;
		if (Cache[attr.index].refcount == 0) {
			SDL_free(Cache[attr.index].data);
			Cache[attr.index].data = NULL;
		}
	}
	return TRUE;
}

void ReleaseAResource(Word RezNum)
{
	if (LevelResources && UnrefResource(RezNum, LevelResources, LevelResourceCache))
		return;
	UnrefResource(RezNum, MainResources, ResourceCache);
}

static Boolean DestroyResource(Word RezNum, RFILE *Rp, Resource *Cache)
{
	ResAttr attr;
	if (!res_attr(Rp, BrgrType, RezNum, &attr))
		return FALSE;
	if (Cache[attr.index].data) {
		SDL_free(Cache[attr.index].data);
		Cache[attr.index].data = NULL;
		Cache[attr.index].refcount=0;
	}
	return TRUE;
}


void KillAResource(Word RezNum)
{
	if (LevelResources)
		DestroyResource(RezNum, LevelResources, LevelResourceCache);
	DestroyResource(RezNum, MainResources, ResourceCache);
}


/**********************************

	Decompress using LZSS

**********************************/

void DLZSS(Byte * restrict Dest, LongWord DestLen, const Byte * restrict Src,LongWord SrcLen)
{
	Word BitBucket;
	Word RunCount;
	Word Fun;
	Byte *BackPtr;
	const Byte * const SrcEnd = Src + SrcLen;
	Byte * const DstEnd = Dest + DestLen;

	if (!SrcLen || !DestLen)
		return;
	BitBucket = (Word) Src[0] | 0x100;
	++Src;
	do {
		if (BitBucket&1) {
			if (Src >= SrcEnd)
				break;
			if (Dest >= DstEnd)
				break;
			Dest[0] = Src[0];
			++Src;
			++Dest;
		} else {
			if (Src >= SrcEnd - 1)
				break;
			RunCount = (Word) Src[0] | ((Word) Src[1]<<8);
			Fun = 0x1000-(RunCount&0xfff);
			BackPtr = Dest-Fun;
			RunCount = ((RunCount>>12) & 0x0f) + 3;
			do {
				if (Dest >= DstEnd)
					return;
				*Dest++ = *BackPtr++;
			} while (--RunCount);
			Src+=2;
		}
		BitBucket>>=1;
		if (BitBucket==1) {
			if (Src >= SrcEnd)
				break;
			BitBucket = (Word)Src[0] | 0x100;
			++Src;
		}
	} while (1);
}

void *LoadCompressed(Word RezNum, LongWord *Length)
{
	LongWord *PackPtr;
	void *UnpackPtr = NULL;
	LongWord PackLength;
	LongWord UnpackLength;

	PackLength = ResourceLength(RezNum);
	if (PackLength < sizeof UnpackLength)
		return NULL;
	PackPtr = LoadAResource(RezNum);
	if (!PackPtr)
		return NULL;
	UnpackLength = SwapLongBE(PackPtr[0]);
	UnpackPtr = AllocSomeMem(UnpackLength);
	if (UnpackPtr) {
		DLZSS(UnpackPtr,UnpackLength,(Byte *) &PackPtr[1], PackLength - sizeof UnpackLength);
		if (Length)
			*Length = UnpackLength;
	}
	ReleaseAResource(RezNum);
	return UnpackPtr;
}

void *LoadCompressedShape(Word RezNum)
{
	Word *ShapePtr = NULL;
	LongWord Len;

	ShapePtr = LoadCompressed(RezNum, &Len);
	if (!ShapePtr)
		return NULL;
	if (Len < 4 || Len < 4 + SwapUShortBE(ShapePtr[0]) * SwapUShortBE(ShapePtr[1])) {
		FreeSomeMem(ShapePtr);
		return NULL;
	}
	return ShapePtr;
}

/**********************************

	Sound loading

**********************************/

static void SoundDLZSS(Byte *Dest, size_t DestLen, const Byte* Src, size_t SrcLen)
{
	const Byte * const SrcEnd = Src + SrcLen;
	Byte * const DstStart = Dest;
	Byte * const DstEnd = Dest + DestLen;
	for (;;) {
		if (Src >= SrcEnd)
			break;
		uint8_t control_bits = *Src++;

		for (uint8_t control_mask = 0x01; control_mask; control_mask <<= 1) {
			if (control_bits & control_mask) {
				if (Src >= SrcEnd)
					break;
				if (Dest >= DstEnd)
					break;
				uint8_t c = *Src++;
				*Dest++ = c;

			} else {
				if (Src >= SrcEnd - 1)
					break;
				uint16_t params = (Src[0] << 8) | Src[1];
				Src += 2;

				size_t copy_offset = Dest - DstStart - ((1 << 12) - (params & 0x0FFF));
				uint8_t count = ((params >> 12) & 0x0F) + 3;
				size_t copy_end_offset = copy_offset + count;

				for (; copy_offset != copy_end_offset; copy_offset++) {
					if (Dest >= DstEnd)
						break;
					*Dest++ = DstStart[copy_offset];
				}
			}
		}
	}
}

static Byte *DecodeCsnd(const Byte *Buf, LongWord Len, LongWord *Size)
{
	Byte *Data;
	LongWord RealSize;

	if (Len < 4)
		return 0;
	RealSize = SwapLongBE(*(const u_uint32_t*)Buf) & 0x00FFFFFF;
	Data = SDL_malloc(RealSize);
	if (!Data) return NULL;
	SoundDLZSS(Data, RealSize, Buf + 4, Len - 4);
	uint8_t* Delta = Data;
	uint8_t* const DataEnd = Data + RealSize;
	for (uint8_t sample = *Delta++; Delta != DataEnd; Delta++) {
		*Delta = (sample += *Delta);
	}
	if (RealSize < 42) {
		SDL_free(Data);
		return NULL;
	}
	*Size = RealSize;
	return Data;
}

static Boolean LoadSound(RFILE *Rp, Sound *Snd, Word ID)
{
	ResAttr Attr;
	uint8_t *Buf, *Decode;
	LongWord Type;
	LongWord Size;

	Type = CsndType;
	if (!res_attr(Rp, Type, ID, &Attr)) {
		Type = SndType;
		if (!res_attr(Rp, Type, ID, &Attr))
			return 0;
	}
	Buf = AllocSomeMem(Attr.size);
	if (!res_read_ind(Rp, Type, Attr.index, Buf, 0, Attr.size, NULL, NULL))
		goto Fail;
	if (Type == CsndType) {
		Decode = DecodeCsnd(Buf, Attr.size, &Size);
		if (!Decode)
			goto Fail;
		SDL_free(Buf);
		Buf = Decode;
	} else {
		Size = Attr.size;
	}
	Snd->ID = ID;
	Snd->basenote = Buf[41] ? Buf[41] : 0x3C;
	Snd->loopstart = SwapLongBE(*(u_uint32_t*)&Buf[32]);
	Snd->loopend = SwapLongBE(*(u_uint32_t*)&Buf[36]);
	Snd->samplerate = SwapLongBE(*(u_uint32_t*)&Buf[28]) >> 16;
	Snd->size = Size - 42;
	Snd->data = &Buf[42];
	return 1;
Fail:
	SDL_free(Buf);
	return 0;
}

static void ReleaseSounds(void)
{
	size_t i;

	if (SoundCache) {
		for (i = 0; i < NumSounds; i++)
			if (SoundCache[i].data)
				SDL_free((Byte*)SoundCache[i].data - 42);
		SDL_free(SoundCache);
		SoundCache = NULL;
		NumSounds = 0;
	}
}

void RegisterSounds(short *SoundIDs, LongWord Len)
{
	size_t Count = 0;
	size_t i;

	ReleaseSounds();
	for (short *SoundID = SoundIDs; Len > 0 && *SoundID != -1; SoundID++, Count++, Len--);
	SoundCache = SDL_calloc(Count, sizeof(Sound));
	NumSounds = Count;
	if (!SoundCache) BailOut("Out of memory");
	i = 0;
	for (short *SoundID = SoundIDs; *SoundID != -1; SoundID++, i++) {
		SoundCache[i].ID = *SoundID;
		if (LevelResources && LoadSound(LevelResources, &SoundCache[i], *SoundID))
			continue;
		LoadSound(MainResources, &SoundCache[i], *SoundID);
	}
}

static int SearchSounds(const void *a, const void *b)
{
	int ua, ub;

	ua = (size_t)a;
	ub = *(const uint16_t *)b;
	return ua - ub;
}

Sound *LoadCachedSound(Word RezNum)
{
	Sound *Snd;
	Snd = bsearch((void*)(size_t)RezNum, SoundCache, NumSounds, sizeof(Sound), SearchSounds);
	if (!Snd || !Snd->data)
		return NULL;
	return Snd;
}

Byte *LoadSong(Word RezNum, LongWord *Len)
{
	Byte *Res;
	if (LevelResources) {
		Res = ReadResource(RezNum, MidiType, LevelResources, Len);
		if (Res)
			return Res;
	}
	return ReadResource(RezNum, MidiType, MainResources, Len);
}

void MacLoadSoundFont(void)
{
	size_t i, j;
	Byte Buf[22];
	ResAttr Attr;
	int NSounds;
	Sound *Sounds;
	Word SoundID;
	Uint8 *Data;
	size_t FloatBufferSize;
	struct tsf_region *Region;
	int DstLen;
	bool Ret;
	Word Flags;
	Byte BaseNote;

	i = res_count(MainResources, InstType);
	if (!i)
		return;
	MusicSynth = TSF_MALLOC(sizeof(tsf));
	memset(MusicSynth, 0, sizeof(tsf));
	MusicSynth->presetNum = i;
	MusicSynth->presets = TSF_MALLOC(MusicSynth->presetNum*sizeof(struct tsf_preset));
	memset(MusicSynth->presets, 0, MusicSynth->presetNum*sizeof(struct tsf_preset));
	for (i = 0; i < MusicSynth->presetNum; i++) {
		MusicSynth->presets[i].regionNum = 1;
		MusicSynth->presets[i].regions = TSF_MALLOC(sizeof(struct tsf_region));
		tsf_region_clear(MusicSynth->presets[i].regions, TSF_FALSE);
	}
	tsf_set_max_voices(MusicSynth, 8);
	for (i = 0; i < 16; i++)
		tsf_channel_set_bank(MusicSynth, i, 0);
	tsf_set_output(MusicSynth, TSF_STEREO_INTERLEAVED, 44100, 0.f);

	NSounds = 0;
	Sounds = AllocSomeMem(MusicSynth->presetNum*sizeof(Sound));
	FloatBufferSize = 0;
	if (!Sounds) BailOut("Out of memory");
	for (i = 0; i < MusicSynth->presetNum; i++) {
		if (!res_list(MainResources, InstType, &Attr, i, 1, NULL, NULL))
			continue;
		if (Attr.size < sizeof Buf)
			continue;
		if (!res_read_ind(MainResources, InstType, i, Buf, 0, sizeof Buf, NULL, NULL))
			continue;
		SoundID = SwapUShortBE(*(u_uint16_t*)&Buf[0]);
		for (j = 0; j < NSounds; j++) {
			if (Sounds[j].ID == SoundID)
				break;
		}
		if (j == NSounds) {
			if (!LoadSound(MainResources, &Sounds[j], SoundID))
				continue;
			Ret = SDL_ConvertAudioSamples(
				&(SDL_AudioSpec){SDL_AUDIO_U8, 1, Sounds[j].samplerate}, Sounds[j].data, Sounds[j].size,
				&(SDL_AudioSpec){SDL_AUDIO_F32, 1, Sounds[j].samplerate}, &Data, &DstLen);
			SDL_free((Byte*)Sounds[j].data - 42);
			Sounds[j].data = NULL;
			if (!Ret)
				continue;
			DstLen /= sizeof(float);
			Sounds[j].offset = FloatBufferSize;
			FloatBufferSize += DstLen + 46;
			MusicSynth->fontSamples = SDL_realloc(MusicSynth->fontSamples, FloatBufferSize*sizeof(float));
			if (!MusicSynth->fontSamples) BailOut("Out of memory");
			memcpy(MusicSynth->fontSamples+Sounds[j].offset, Data, DstLen*sizeof(float));
			memset(MusicSynth->fontSamples+Sounds[j].offset+DstLen, 0, 46*sizeof(float));
			SDL_free(Data);
			NSounds++;
		}
		Flags = (Buf[5]<<8)|Buf[6];
		BaseNote = Buf[3];
		MusicSynth->presets[i].preset = Attr.ID;
		Region = &MusicSynth->presets[i].regions[0];

		Region->sample_rate = (Flags & 0x800) ? Sounds[j].samplerate : 22254;
		Region->offset = Sounds[j].offset;
		Region->end = Region->offset + Sounds[j].size;
		Region->pitch_keycenter = BaseNote ? BaseNote : Sounds[j].basenote;
		if (!(Flags & 0x2000) && Sounds[j].loopend - Sounds[j].loopstart >= 64) {
			Region->loop_mode = TSF_LOOPMODE_CONTINUOUS;
			Region->loop_start = Region->offset + Sounds[j].loopstart;
			Region->loop_end = Region->offset + Sounds[j].loopend;
		}
		Region->ampenv.release = 4000.f;
		Region->ampenv.decay = 8000.f;
		Region->ampenv.hold = 5000.f;
		if (Flags & 0x04)
			Region->group = Attr.ID;
		if (Flags & 0x40)
			Region->pitch_keytrack = 0;
	}
	FreeSomeMem(Sounds);
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
	} while (++i<256);
	SDL_SetPaletteColors(Palette, Colors, 0, 256);
Done:
	if (Pixels)
		SDL_free(Pixels);
	SDL_free(Data);
	return Surface;
}
