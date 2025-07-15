#include "WolfDef.h"
#include "SDLWolf.h"
#include <string.h>
#include <errno.h>
#include <fluidsynth.h>

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

extern fluid_synth_t *FluidSynth;

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
		char TmpPath[strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1];
		stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
		MainResources = LoadResources(TmpPath, &ResourceCache);
		if (!MainResources && errno != ENOENT)
			BailOut("%s: %s", TmpPath, strerror(errno));
	}
	if (!MainResources) {
		BasePath = SDL_GetBasePath();
		char TmpPath[strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1];
		stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
		MainResources = LoadResources(TmpPath, &ResourceCache);
		if (!MainResources)
			BailOut("%s: %s", TmpPath, strerror(errno));
	}
}

void KillResources(void)
{
	ReleaseSounds();
	ReleaseResources(&MainResources, &ResourceCache);
	ReleaseResources(&LevelResources, &LevelResourceCache);
}

Boolean MountMapFile(const char *FileName)
{
	ReleaseResources(&LevelResources, &LevelResourceCache);
	MapListPtr = NULL;
	SoundListPtr = NULL;
	SongListPtr = NULL;
	WallListPtr = NULL;

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
		char TmpPath[strlen(BasePath) + __builtin_strlen(LevelsFolder) + 1];
		stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder);
		SDL_EnumerateDirectory(TmpPath, callback, userdata);
	}
	{
		BasePath = SDL_GetBasePath();
		char TmpPath[strlen(BasePath) + __builtin_strlen(LevelsFolder) + 1];
		stpcpy(stpcpy(TmpPath, BasePath), LevelsFolder);
		SDL_EnumerateDirectory(TmpPath, callback, userdata);
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
				SDL_free(SoundCache[i].data - 42);
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

typedef struct {
	Word ID;
	Word flags;
	Byte basenote;
	fluid_preset_t *preset;
	fluid_sample_t *sample;
} Instrument;

typedef struct {
	Word iter;
	Word ninstruments;
	Word nsounds;
	Instrument instruments[];
} SFHeader;

static const char *SoundFontGetName()
{
	return "";
}

static fluid_preset_t *SoundFontGetPreset(fluid_sfont_t *SFont, int Bank, int Prog)
{
	int i;
	SFHeader *Header = fluid_sfont_get_data(SFont);
	for (i = 0; i < Header->ninstruments; i++)
		if (Header->instruments[i].ID == Prog)
			return Header->instruments[i].preset;
	return NULL;
}

static void SoundFontIterStart(fluid_sfont_t *SFont)
{
	SFHeader *Header = fluid_sfont_get_data(SFont);
	Header->iter =0;
}

static fluid_preset_t *SoundFontIterNext(fluid_sfont_t *SFont)
{
	SFHeader *Header = fluid_sfont_get_data(SFont);
	if (Header->iter >= Header->ninstruments)
		return NULL;
	return Header->instruments[Header->iter++].preset;
}

static int PresetGetBank(fluid_preset_t *Preset)
{
	return 0;
}

static int PresetGetProg(fluid_preset_t *Preset)
{
	Instrument *Inst = fluid_preset_get_data(Preset);
	return Inst->ID;
}

static int PresetNoteOn(fluid_preset_t *Preset, fluid_synth_t *Synth, int Chan, int Key, int Vel)
{
	fluid_voice_t *Voice;
	Instrument *Inst = fluid_preset_get_data(Preset);
	if (Inst->flags & 0x04)
		fluid_synth_all_sounds_off(Synth, Chan);
	if (Inst->flags & 0x40)
		Key = Inst->basenote;
	Voice = fluid_synth_alloc_voice(Synth, Inst->sample, Chan, Key, Vel);
	if (!Voice)
		return FLUID_FAILED;
	if (!(Inst->flags & 0x2000))
		fluid_voice_gen_set(Voice, GEN_SAMPLEMODE, 1);
	fluid_voice_gen_set(Voice, GEN_VOLENVRELEASE, 4000.f);
	fluid_voice_gen_set(Voice, GEN_VOLENVDECAY, 8000.f);
	fluid_voice_gen_set(Voice, GEN_VOLENVHOLD, 5000.f);
	fluid_synth_start_voice(Synth, Voice);
	return FLUID_OK;
}
static void PresetFree(fluid_preset_t *Preset)
{
}

static int SoundFontFree(fluid_sfont_t *SFont)
{
	SFHeader *Header;
	int i;
	Sound *Sounds;

	Header = fluid_sfont_get_data(SFont);
	for (i = 0; i < Header->ninstruments; i++) {
		if (Header->instruments[i].preset)
			delete_fluid_preset(Header->instruments[i].preset);
		if (Header->instruments[i].sample)
			delete_fluid_sample(Header->instruments[i].sample);
	}
	Sounds = (void*)&Header->instruments[Header->ninstruments];
	for (i = 0; i < Header->nsounds; i++) {
		if (Sounds[i].data)
			SDL_free(Sounds[i].data);
	}
	SDL_free(Header);
	delete_fluid_sfont(SFont);
	return 0;
}

void MacLoadSoundFont(void)
{
	size_t i, j;
	Byte Buf[22];
	ResAttr Attr;
	SFHeader *Header;
	Instrument *Instruments;
	Sound *Sounds;
	fluid_sfont_t *SoundFont;
	fluid_preset_t *Preset;
	fluid_sample_t *Sample;
	Word SoundID;
	Uint8 *Data;
	int DstLen;
	bool Ret;

	if (!FluidSynth)
		return;
	i = res_count(MainResources, InstType);
	if (!i)
		return;
	SoundFont = new_fluid_sfont(SoundFontGetName, SoundFontGetPreset, SoundFontIterStart, SoundFontIterNext, SoundFontFree);
	if (!SoundFont)
		return;

	Header = AllocSomeMem(sizeof(SFHeader)+i*(sizeof(Instrument)+sizeof(Sound)));
	Header->ninstruments = i;
	Header->nsounds = 0;
	Header->iter = 0;
	Instruments = Header->instruments;
	Sounds = (void*)&Instruments[i];
	for (i = 0; i < Header->ninstruments; i++) {
		Instruments[i].preset = NULL;
		Instruments[i].ID = -1;
		if (!res_list(MainResources, InstType, &Attr, i, 1, NULL, NULL))
			continue;
		Instruments[i].ID = Attr.ID;
		if (Attr.size < sizeof Buf)
			continue;
		if (!res_read_ind(MainResources, InstType, i, Buf, 0, sizeof Buf, NULL, NULL))
			continue;
		Instruments[i].flags = (Buf[5]<<8)|Buf[6];
		Instruments[i].basenote = Buf[3];
		Preset = new_fluid_preset(SoundFont, SoundFontGetName, PresetGetBank, PresetGetProg, PresetNoteOn, PresetFree);
		if (!Preset)
			continue;
		Sample = new_fluid_sample();
		if (!Sample) {
			delete_fluid_preset(Preset);
			continue;
		}
		Instruments[i].preset = Preset;
		Instruments[i].sample = Sample;
		SoundID = SwapUShortBE(*(u_uint16_t*)&Buf[0]);
		for (j = 0; j < Header->nsounds; j++) {
			if (Sounds[j].ID == SoundID)
				break;
		}
		if (j == Header->nsounds) {
			if (!LoadSound(MainResources, &Sounds[j], SoundID))
				continue;
			Ret = SDL_ConvertAudioSamples(
				&(SDL_AudioSpec){SDL_AUDIO_U8, 1, Sounds[j].samplerate}, Sounds[j].data, Sounds[j].size,
				&(SDL_AudioSpec){SDL_AUDIO_S16, 1, Sounds[j].samplerate}, &Data, &DstLen);
			SDL_free(Sounds[j].data - 42);
			if (!Ret)
				continue;
			Sounds[j].data = Data;
			Header->nsounds++;
		}
		fluid_sample_set_sound_data(Sample, Sounds[j].data, NULL, Sounds[j].size,
								(Instruments[i].flags & 0x800) ? Sounds[j].samplerate : 22254, 0);
		fluid_sample_set_pitch(Sample, Instruments[i].basenote ? Instruments[i].basenote : Sounds[j].basenote, 0);
		if (!(Instruments[i].flags & 0x2000))
			fluid_sample_set_loop(Sample, Sounds[j].loopstart, Sounds[j].loopend);
	}
	if (Header->nsounds != Header->ninstruments) {
		Header = SDL_realloc(Header, sizeof(SFHeader)+Header->ninstruments*sizeof(Instrument)+Header->nsounds*sizeof(Sound));
		if (!Header) BailOut("Out of memory");
	}
	for (i = 0; i < Header->ninstruments; i++)
		fluid_preset_set_data(Header->instruments[i].preset, &Header->instruments[i]);
	fluid_sfont_set_data(SoundFont, Header);
	fluid_synth_add_sfont(FluidSynth, SoundFont);
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
