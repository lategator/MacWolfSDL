#include "WolfDef.h"
#include "SDLWolf.h"
#include <stdio.h>
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

static const char *MainResourceFile = "Wolf3D.rsrc";
static const char *LevelsFolder = "Levels/";
static const char *DefaultLevelsPath = "Levels/ Second Encounter (30 Levels).rsrc";

static const uint32_t BrgrType = 0x42524752; /* BRGR */
static const uint32_t PictType = 0x50494354; /* PICT */
static const uint32_t SndType  = 0x736e6420; /* snd  */
static const uint32_t CsndType = 0x63736E64; /* csnd */
static const uint32_t MidiType = 0x4d696469; /* Midi */
static const uint32_t InstType = 0x494E5354; /* INST */

ResourceFile *MainResources = NULL;
static ResourceFile *LevelResources = NULL;
static LongWord NumSounds = 0;
static Sound *SoundCache = NULL;
tsf *MusicSynth;

static ResourceFile *LoadResourceFork(FILE *File, long BaseOffset);
static ResourceFile *LoadMacBinary(FILE *File);
static ResourceFile *LoadAppleSingle(FILE *File);
static void ReleaseSounds(void);

ResourceFile *LoadResources(const char *FileName)
{
	FILE *File;
	ResourceFile *Rp;

	File = fopen(FileName, "rb");
	if (!File)
		return NULL;
	Rp = LoadMacBinary(File);
	if (!Rp) {
		fseek(File, 0, SEEK_SET);
		Rp = LoadAppleSingle(File);
		if (!Rp) {
			fseek(File, 0, SEEK_SET);
			Rp = LoadResourceFork(File, 0);
		}
	}
	if (!Rp)
		fclose(File);
	return Rp;
}

#pragma pack(push, r1, 1)
typedef struct {
	LongWord DataOffset;
	LongWord MapOffset;
	LongWord DataLen;
	LongWord MapLen;
} ResourceHeader;

typedef struct {
	ResourceHeader Header;
	LongWord NextMap;
	Word RefNum;
	Word Attributes;
	Word TypeListOffset;
	Word NameListOffset;
} ResourceMap;

typedef struct {
	LongWord Type;
	Word Count;
	Word RefListOffset;
} ResourceType;

typedef struct {
	Word NumTypes;
	ResourceType Types[];
} ResourceTypeList;

typedef struct {
	Word ID;
	Word NameOffset;
	Byte Attributes;
	Byte DataOffset[3];
	LongWord Handle;
} ResourceRef;
#pragma pack(pop, r1)

typedef struct {
	void *Data;
	long Offset;
	LongWord RefCount;
	LongWord Size;
	Word ID;
} Resource;

typedef struct {
	LongWord Type;
	Word Count;
	Resource *Refs;
} ResourceGroup;

struct ResourceFile {
	FILE *File;
	Word NumGroups;
	ResourceGroup Groups[];
};

static void *ReadAtOffset(FILE *File, LongWord Offset, LongWord Size)
{
	void *Buf;

	if (fseek(File, Offset, SEEK_SET))
		return NULL;
	Buf = AllocSomeMem(Size);
	if (fread(Buf, 1, Size, File) != Size) {
		FreeSomeMem(Buf);
		return NULL;
	}
	return Buf;
}

static int SortResourceTypes(const void *a, const void *b)
{
	LongWord ua, ub;

	ua = ((const ResourceType *)a)->Type;
	ub = ((const ResourceType *)b)->Type;
	return ua == ub ? 0 : (ua < ub ? -1 : 1);
}

static int SortResourceRefs(const void *a, const void *b)
{
	int ua, ub;

	ua = ((const ResourceRef *)a)->ID;
	ub = ((const ResourceRef *)b)->ID;
	return ua - ub;
}

static ResourceFile *LoadResourceFork(FILE *File, long BaseOffset)
{
	ResourceHeader Header;
	Byte *Map;
	ResourceMap *MapHead;
	ResourceTypeList *TypeList;
	ResourceType *Type;
	ResourceRef *Ref;
	ResourceFile *Rp = NULL;
	ResourceGroup *Group;
	Resource *Res;
	LongWord TotalCount = 0;
	LongWord TotalSize;
	LongWord RefSize;
	int i, j;

	if (fseek(File, BaseOffset, SEEK_SET))
		return NULL;
	if (fread(&Header, 1, sizeof Header, File) != sizeof Header)
		goto Done;

	Header.DataOffset = SwapLongBE(Header.DataOffset);
	Header.MapOffset = SwapLongBE(Header.MapOffset);
	Header.DataLen = SwapLongBE(Header.DataLen);
	Header.MapLen = SwapLongBE(Header.MapLen);

	Map = ReadAtOffset(File, BaseOffset + Header.MapOffset, Header.MapLen);
	if (!Map)
		goto Done;
	MapHead = (void*)Map;
	MapHead->TypeListOffset = SwapUShortBE(MapHead->TypeListOffset);
	if (MapHead->TypeListOffset + 2 > Header.MapLen)
		goto Done;
	TypeList = (void*)(Map + MapHead->TypeListOffset);
	TypeList->NumTypes = SwapUShortBE(TypeList->NumTypes);
	if (MapHead->TypeListOffset + sizeof(ResourceTypeList) + (TypeList->NumTypes+1)*sizeof(ResourceType) > Header.MapLen)
		goto Done;
	Type = TypeList->Types;
	for (i = 0; i < TypeList->NumTypes+1; i++, Type++) {
		Type->Type = SwapLongBE(Type->Type);
		Type->RefListOffset = SwapUShortBE(Type->RefListOffset);
		Type->Count = SwapUShortBE(Type->Count);
		if (MapHead->TypeListOffset + Type->RefListOffset + (Type->Count + 1)*sizeof(ResourceRef) > Header.MapLen)
			goto Done;
		TotalCount += Type->Count+1;
		Ref = (void*)(Map + MapHead->TypeListOffset + Type->RefListOffset);
		for (j = 0; j < Type->Count + 1; j++, Ref++) {
			Ref->ID = SwapUShortBE(Ref->ID);
			Ref->Handle = (Ref->DataOffset[0]<<16)|(Ref->DataOffset[1]<<8)|Ref->DataOffset[2];
			if (Ref->Handle > Header.DataLen - 4)
				goto Done;
		}
		Ref = (void*)(Map + MapHead->TypeListOffset + Type->RefListOffset);
		SDL_qsort(Ref, Type->Count+1, sizeof(ResourceRef), SortResourceRefs);
	}
	SDL_qsort(TypeList->Types, TypeList->NumTypes+1, sizeof(ResourceType), SortResourceTypes);
	TotalSize = sizeof(ResourceFile) + (TypeList->NumTypes+1)*sizeof(ResourceGroup) + TotalCount*sizeof(Resource);
	Rp = AllocSomeMem(TotalSize);
	memset(Rp, 0, TotalSize);
	Rp->NumGroups = TypeList->NumTypes+1;
	Res = (void*)((Byte*)Rp+sizeof(ResourceFile) + Rp->NumGroups*sizeof(ResourceGroup));
	Type = TypeList->Types;
	Group = Rp->Groups;
	for (i = 0; i < Rp->NumGroups; i++, Type++, Group++) {
		Group->Type = Type->Type;
		Group->Count = Type->Count+1;
		Group->Refs = Res;
		Ref = (void*)(Map + MapHead->TypeListOffset + Type->RefListOffset);
		for (j = 0; j < Type->Count + 1; j++, Ref++, Res++) {
			if (fseek(File, BaseOffset + Header.DataOffset + Ref->Handle, SEEK_SET)
				|| fread(&RefSize, 1, sizeof(LongWord), File) != sizeof(LongWord)) {
				ReleaseResources(Rp);
				Rp = NULL;
				goto Done;
			}
			Res->ID = Ref->ID;
			Res->Offset = BaseOffset + Header.DataOffset + Ref->Handle + 4;
			Res->Size = SwapLongBE(RefSize);
		}
	}
	Rp->File = File;
Done:
	if (Map)
		FreeSomeMem(Map);
	return Rp;
}

void ReleaseResources(ResourceFile *Rp)
{
	int i, j;

	if (!Rp)
		return;

	for (i = 0; i < Rp->NumGroups; i++) {
		if (Rp->Groups[i].Refs) {
			for (j = 0; j < Rp->Groups[i].Count; j++) {
				if (Rp->Groups[i].Refs[j].Data)
					FreeSomeMem(Rp->Groups[i].Refs[j].Data);
			}
		}
	}

	if (Rp->File)
		fclose(Rp->File);
	FreeSomeMem(Rp);
}

#pragma pack(push, r1, 1)
typedef struct {
	Byte OldVersion;
	Byte FileNameLen;
	char FileName[63];
	char FileType[4];
	char FileCreator[4];
	Byte FinderFlags;
	Byte :8;
	Word VertPos;
	Word HorizPos;
	Word FolderID;
	Byte Protected;
	Byte :8;
	LongWord DataForkLen;
	LongWord ResourceForkLen;
	LongWord CreationDate;
	LongWord ModifiedDate;
	Word CommentLen;
	Byte FinderFlags2;
	char Signature[4];
	Byte FileNameScript;
	Byte FinderFlags3;
	LongWord :32;
	LongWord :32;
	LongWord UnpackLen;
	Word SecondaryHeaderLen;
	Byte Version;
	Byte MinVersion;
	Word CRC;
	Word :16;
} MacBinaryHeader;
#pragma pack(pop, r1)

static ResourceFile *LoadMacBinary(FILE *File)
{
	MacBinaryHeader Header;
	ResourceFile *Rp = NULL;
	LongWord ResourceForkOffset;

	if (fread(&Header, 1, sizeof Header, File) != sizeof Header)
		goto Done;

	if (Header.OldVersion != 0 || Header.FileNameLen == 0 || Header.FileNameLen > 63)
		goto Done;
	if (strncmp(Header.FileType, "APPL", 4)
		&& strncmp(Header.FileType, "MAPS", 4)
		&& strncmp(Header.FileType, "????", 4)
		&& strncmp(Header.FileCreator, "WOLF", 4)
		&& strncmp(Header.FileCreator, "????", 4)
		&& strncmp(Header.Signature, "mBIN", 4))
		goto Done;

	ResourceForkOffset = sizeof Header + ((SwapUShortBE(Header.SecondaryHeaderLen) + 127) & ~127) + ((SwapLongBE(Header.DataForkLen) + 127) & ~127);
	Rp = LoadResourceFork(File, ResourceForkOffset);
Done:
	return Rp;
}

#pragma pack(push, r1, 1)
typedef struct {
	LongWord ID;
	LongWord Offset;
	LongWord Length;
} AppleSingleEntry;
typedef struct {
	char Signature[4];
	LongWord Version;
	LongWord :32;
	LongWord :32;
	LongWord :32;
	LongWord :32;
	Word NumEntries;
} AppleSingleHeader;
#pragma pack(pop, r1)

static ResourceFile *LoadAppleSingle(FILE *File)
{
	AppleSingleHeader Header;
	ResourceFile *Rp = NULL;
	int i;
	AppleSingleEntry *Entries;

	if (fread(&Header, 1, sizeof Header, File) != sizeof Header)
		goto Done;

	if (strncmp(Header.Signature, "\x00\x05\x16\x00", 4))
		goto Done;

	Header.NumEntries = SwapUShortBE(Header.NumEntries);
	Entries = AllocSomeMem(Header.NumEntries*sizeof(AppleSingleEntry));
	if (fread(Entries, sizeof(AppleSingleEntry), Header.NumEntries, File) != Header.NumEntries)
		goto Done;
	for (i = 0; i < Header.NumEntries; i++) {
		if (SwapLongBE(Entries[i].ID) != 2)
			continue;
		Rp = LoadResourceFork(File, SwapLongBE(Entries[i].Offset));
		break;
	}

Done:
	if (Entries)
		FreeSomeMem(Entries);
	return Rp;
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
		MainResources = LoadResources(TmpPath);
		FreeSomeMem(TmpPath);
	}
	if (!MainResources) {
		BasePath = SDL_GetBasePath();
		char *TmpPath = AllocSomeMem(strlen(BasePath) + __builtin_strlen(MainResourceFile) + 1);
		stpcpy(stpcpy(TmpPath, BasePath), MainResourceFile);
		MainResources = LoadResources(TmpPath);
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
	ReleaseResources(MainResources);
	ReleaseResources(LevelResources);
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
	ReleaseResources(LevelResources);
	LevelResources = NULL;

	if (!FileName)
		FileName = DefaultLevelsPath;

	LevelResources = LoadResources(FileName);
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

static int SearchResourceGroups(const void *a, const void *b)
{
	LongWord ua, ub;

	ua = (size_t)a;
	ub = ((const ResourceGroup *)b)->Type;
	return ua == ub ? 0 : (ua < ub ? -1 : 1);
}

static int SearchResources(const void *a, const void *b)
{
	int ua, ub;

	ua = (size_t)a;
	ub = ((const Resource *)b)->ID;
	return ua - ub;
}

static ResourceGroup *FindResourceGroup(LongWord Type, ResourceFile *Rp)
{
	return SDL_bsearch((void*)(size_t)Type, Rp->Groups, Rp->NumGroups, sizeof(ResourceGroup), SearchResourceGroups);
}

static Resource *FindResource(Word RezNum, LongWord Type, ResourceFile *Rp)
{
	ResourceGroup *Group;

	Group = FindResourceGroup(Type, Rp);
	if (!Group)
		return NULL;
	return SDL_bsearch((void*)(size_t)RezNum, Group->Refs, Group->Count, sizeof(Resource), SearchResources);
}

static void *ReadResource(Word RezNum, LongWord Type, ResourceFile *Rp, LongWord *Len)
{
	Resource *Res;
	void *Data;

	Res = FindResource(RezNum, Type, Rp);
	if (!Res)
		return NULL;
	Data = ReadAtOffset(Rp->File, Res->Offset, Res->Size);
	if (!Data)
		return NULL;
	if (Len)
		*Len = Res->Size;
	return Data;
}

static Boolean ReadResourceTo(Resource *Res, FILE *File, void *Buf, LongWord Len)
{
	void *Data;

	if (Res->Size != Len)
		return FALSE;
	if (Res->Data) {
		memcpy(Buf, Res->Data, Len);
		return TRUE;
	}
	if (fseek(File, Res->Offset, SEEK_SET))
		return FALSE;
	if (fread(Buf, 1, Len, File) != Len)
		return FALSE;
	return TRUE;
}

static Resource *CacheResource(Word RezNum, LongWord Type, ResourceFile *Rp)
{
	Resource *Res;
	void *Data;

	Res = FindResource(RezNum, Type, Rp);
	if (!Res)
		return NULL;
	if (Res->Data) {
		Res->RefCount++;
		return Res;
	}
	Data = ReadAtOffset(Rp->File, Res->Offset, Res->Size);
	if (!Data)
		return NULL;
	Res->RefCount++;
	Res->Data = Data;
	return Res;
}

static Resource *GetResource2(Word RezNum, ResourceFile *Rp)
{
	Resource *Res;
	if (LevelResources) {
		Res = CacheResource(RezNum, BrgrType, LevelResources);
		if (Res)
			return Res;
	}
	return CacheResource(RezNum, BrgrType, MainResources);
}

static Resource *GetResource(Word RezNum)
{
	Resource *Res;
	if (LevelResources) {
		Res = CacheResource(RezNum, BrgrType, LevelResources);
		if (Res)
			return Res;
	}
	return CacheResource(RezNum, BrgrType, MainResources);
}

/**********************************

	Load a personal resource

**********************************/

void *LoadAResource(Word RezNum)
{
	Resource *Res;

	Res = GetResource(RezNum);
	if (Res)
		return Res->Data;
	return NULL;
}

LongWord ResourceLength(Word RezNum)
{
	Resource *Res = NULL;

	if (LevelResources)
		Res = FindResource(RezNum, BrgrType, LevelResources);
	if (!Res)
		Res = FindResource(RezNum, BrgrType, MainResources);
	if (Res)
		return Res->Size;
	return 0;
}

/**********************************

	Allow a resource to be purged

**********************************/

static Boolean UnrefResource(Word RezNum, ResourceFile *Rp)
{
	Resource *Res;

	Res = FindResource(RezNum, BrgrType, Rp);
	if (!Res)
		return FALSE;
	if (Res->Data) {
		Res->RefCount--;
		if (Res->RefCount == 0) {
			FreeSomeMem(Res->Data);
			Res->Data = NULL;
		}
	}
	return TRUE;
}

void ReleaseAResource(Word RezNum)
{
	if (LevelResources && UnrefResource(RezNum, LevelResources))
		return;
	UnrefResource(RezNum, MainResources);
}

static Boolean DestroyResource(Word RezNum, ResourceFile *Rp)
{
	Resource *Res;

	Res = FindResource(RezNum, BrgrType, Rp);
	if (!Res)
		return FALSE;
	if (Res->Data) {
		FreeSomeMem(Res->Data);
		Res->Data = NULL;
		Res->RefCount = 0;
	}
	return TRUE;
}


void KillAResource(Word RezNum)
{
	if (LevelResources)
		DestroyResource(RezNum, LevelResources);
	DestroyResource(RezNum, MainResources);
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

static Boolean LoadSound(ResourceFile *Rp, Sound *Snd, Word ID)
{
	Resource *Res;
	uint8_t *Buf, *Decode;
	LongWord Type;
	LongWord Size;

	Type = CsndType;
	Res = FindResource(ID, Type, Rp);
	if (!Res) {
		Type = SndType;
		Res = FindResource(ID, Type, Rp);
		if (!Res)
			return 0;
	}
	Buf = ReadAtOffset(Rp->File, Res->Offset, Res->Size);
	if (Type == CsndType) {
		Decode = DecodeCsnd(Buf, Res->Size, &Size);
		if (!Decode)
			goto Fail;
		SDL_free(Buf);
		Buf = Decode;
	} else {
		Size = Res->Size;
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
	Snd = SDL_bsearch((void*)(size_t)RezNum, SoundCache, NumSounds, sizeof(Sound), SearchSounds);
	if (!Snd || !Snd->data)
		return NULL;
	return Snd;
}

Byte *LoadSong(Word RezNum, LongWord *Len)
{
	Byte *Data = NULL;
	if (LevelResources)
		Data = ReadResource(RezNum, MidiType, LevelResources, Len);
	if (!Data)
		Data = ReadResource(RezNum, MidiType, MainResources, Len);
	return Data;
}

void MacLoadSoundFont(void)
{
	size_t i, j;
	ResourceFile *Rp = NULL;
	ResourceGroup *Group = NULL;
	Resource *Res;
	Byte Buf[22];
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

	if (LevelResources) {
		Rp = LevelResources;
		Group = FindResourceGroup(InstType, Rp);
	}
	if (!Rp) {
		Rp = MainResources;
		Group = FindResourceGroup(InstType, Rp);
	}
	if (!Group || !Group->Count)
		return;
	MusicSynth = TSF_MALLOC(sizeof(tsf));
	memset(MusicSynth, 0, sizeof(tsf));
	MusicSynth->presetNum = Group->Count;
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
	for (i = 0; i < MusicSynth->presetNum; i++) {
		Res = &Group->Refs[i];
		if (Res->Size < sizeof Buf)
			continue;
		if (!ReadResourceTo(Res, Rp->File, Buf, sizeof Buf))
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
		MusicSynth->presets[i].preset = Res->ID;
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
		Region->ampenv.release = 4.f;
		Region->ampenv.decay = 8.f;
		Region->ampenv.hold = 10.f;
		if (Flags & 0x04)
			Region->group = Res->ID;
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

SDL_Surface *LoadPict(ResourceFile *Rp, Word PicNum)
{
	Resource *Res;
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
	Res = FindResource(PicNum, PictType, Rp);
	if (!Res)
		return NULL;
	if (Res->Size < 0x87E)
		return NULL;
	Data = ReadAtOffset(Rp->File, Res->Offset, Res->Size);
	if (!Data) return NULL;
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
		PackedEnd = Data + Res->Size;
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
