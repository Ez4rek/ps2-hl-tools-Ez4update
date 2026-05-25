// Author:	supadupaplex
// Modified:	Added --scale flag for scale resize and texture dimension warnings
// License:	BSD-3-Clause (check out license.txt)

//
// This file contains model conversion and texture extraction functions
//

////////// Includes //////////
#include "util.h"
#include "main.h"

////////// Global variables //////////
// When true, use ScaleResize instead of TileResize during MDL->DOL conversion.
// Set by passing "scale" as first argument.
static bool g_UseScaleResize = false;

////////// Functions //////////
uint PSIProperSize(uint Size, bool ToLower);
void ExtractDOLTextures(const char * FileName);
void ExtractMDLTextures(const char * FileName);
void ConvertMDLToDOL(const char * FileName);
void ConvertDOLToMDL(const char * FileName);
void ConvertSubmodel(const char * FileName, char * OriginalExtension, char * TargetExtension);
void ConvertDummySubmodel(const char * FileName, char * OriginalExtension, char * TargetExtension);
void GetExtraDOLData(const char * FileName);
bool AddTerminator(char * Buffer, char Symbol);
ushort CountSymbols(char * Buffer, char Symbol);
bool CheckExtraFile(const char * FileName);
void GetValues(char * Buffer, ulong * Values, uchar ValuesCount);
bool TranslateExtraFile(const char * FileName, sDOLExtraSection * DOLExtraSect, sDOLLODEntry ** LODTable);
void PatchSubmodelRef(sModelHeader * MdlHdr, char * ModelData, ulong ModelDataSize, char * NewExtension);
int CheckModel(const char * FileName);
void PatchDOLExtraSection(char * ModelData, ulong ModelDataSize, ulong LODDataOffseet, uchar MaxBodyParts, uchar NumBodyGroups, ulong FadeStart, ulong FadeEnd);
void SeqReport(const char * FileName);


void PatchDOLExtraSection(char * ModelData, ulong ModelDataSize, ulong LODDataOffseet, uchar MaxBodyParts, uchar NumBodyGroups, ulong FadeStart, ulong FadeEnd)
{
	sDOLExtraSection * DOLExtraSection;

	if (ModelDataSize < sizeof(sDOLExtraSection))
		return;

	DOLExtraSection = (sDOLExtraSection *)ModelData;

	DOLExtraSection->LODDataOffset = LODDataOffseet;
	DOLExtraSection->MaxBodyParts = MaxBodyParts;
	DOLExtraSection->NumBodyGroups = NumBodyGroups;
	DOLExtraSection->Magic[0] = 0;
	DOLExtraSection->Magic[1] = 0;
	DOLExtraSection->FadeStart = FadeStart;
	if (FadeEnd >= FadeStart)
		DOLExtraSection->FadeEnd = FadeEnd;
	else
		DOLExtraSection->FadeEnd = FadeStart;
}

int CheckModel(const char * FileName)
{
	FILE * ptrModelFile;
	sModelHeader ModelHeader;
	int ModelType;

	SafeFileOpen(&ptrModelFile, FileName, "rb");

	if (FileSize(&ptrModelFile) < sizeof(sModelHeader))
	{
		ModelType = DUMMY_MODEL;
	}
	else
	{
		ModelHeader.UpdateFromFile(&ptrModelFile);
		ModelType = ModelHeader.CheckModel();
	}

	fclose(ptrModelFile);
	return ModelType;
}

void ConvertDOLToMDL(const char * FileName)
{
	sModelHeader ModelHeader;
	sModelTextureEntry * ModelTextureTable;
	ulong ModelTextureTableSize;
	sTexture * Textures;

	FILE * ptrInFile;
	char cNewModelName[64];
	FILE * ptrOutFile;
	char cOutFileName[PATH_LEN];

	ulong ModelSize;

	SafeFileOpen(&ptrInFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrInFile);

	if (ModelHeader.CheckModel() == NORMAL_MODEL)
	{
		printf("Internal name: %s \nTextures: %i, Texture table offset: 0x%X \n", ModelHeader.Name, ModelHeader.TextureCount, ModelHeader.TextureTableOffset);
	}
	else
	{
		puts("Incorrect model file.");
		return;
	}

	if (ModelHeader.TextureTableOffset - sizeof(sModelHeader) > sizeof(sDOLExtraSection))
		GetExtraDOLData(FileName);

	ModelTextureTableSize = ModelHeader.TextureCount * sizeof(sModelTextureEntry);
	ModelTextureTable = (sModelTextureEntry *)malloc(ModelTextureTableSize);
	Textures = (sTexture *)malloc(sizeof(sTexture) * ModelHeader.TextureCount);

	uint BitmapOffset, BitmapSize, PaletteOffset, PaletteSize;
	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].UpdateFromFile(&ptrInFile, ModelHeader.TextureTableOffset, i);

		BitmapOffset = ModelTextureTable[i].Offset + DOL_TEXTURE_HEADER_SIZE + EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE;
		BitmapSize = ModelTextureTable[i].Height * ModelTextureTable[i].Width;
		PaletteOffset = ModelTextureTable[i].Offset + DOL_TEXTURE_HEADER_SIZE;
		PaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE;

		Textures[i].Initialize();
		Textures[i].UpdateFromFile(&ptrInFile, BitmapOffset, BitmapSize, PaletteOffset, PaletteSize, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height);

		Textures[i].PaletteReformat(DOL_BMP_PALETTE_ELEMENT_SIZE);
		Textures[i].PaletteRemoveSpacers();
	}

	FileGetFullName(FileName, cOutFileName, sizeof(cOutFileName));
	strcat(cOutFileName, ".mdl");
	SafeFileOpen(&ptrOutFile, cOutFileName, "wb");

	FileGetName(cOutFileName, cNewModelName, sizeof(cNewModelName), false);
	strcat(cNewModelName, ".mdl");
	ModelHeader.Rename(cNewModelName);
	ModelHeader.TextureDataOffset = ModelHeader.TextureTableOffset + sizeof(sModelTextureEntry) * ModelHeader.TextureCount + ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	FileWriteBlock(&ptrOutFile, (char *)&ModelHeader, sizeof(sModelHeader));

	uchar * ModelData;
	ModelData = (uchar *)malloc(ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	FileReadBlock(&ptrInFile, (char *)ModelData, sizeof(sModelHeader), ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	PatchDOLExtraSection((char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), 0x00504453, 0, 0, 0, 0);
	PatchSubmodelRef(&ModelHeader, (char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), ".mdl");
	FileWriteBlock(&ptrOutFile, (char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	free(ModelData);

	uint Offset = ModelHeader.TextureTableOffset + sizeof(sModelTextureEntry) * ModelHeader.TextureCount + ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].Width = Textures[i].Width;
		ModelTextureTable[i].Height = Textures[i].Height;
		ModelTextureTable[i].Offset = Offset;
		Offset += Textures[i].Width * Textures[i].Height + Textures[i].PaletteSize;
	}
	FileWriteBlock(&ptrOutFile, (char *)ModelTextureTable, ModelTextureTableSize);

	uchar * SkinTable;
	ulong SkinTableSize = ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	SkinTable = (uchar *)malloc(SkinTableSize);
	FileReadBlock(&ptrInFile, SkinTable, ModelHeader.SkinTableOffset, SkinTableSize);
	FileWriteBlock(&ptrOutFile, SkinTable, SkinTableSize);
	free(SkinTable);

	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		FileWriteBlock(&ptrOutFile, (char *)Textures[i].Bitmap, Textures[i].Width * Textures[i].Height);
		FileWriteBlock(&ptrOutFile, (char *)Textures[i].Palette, Textures[i].PaletteSize);
	}

	ModelSize = FileSize(&ptrOutFile);
	FileWriteBlock(&ptrOutFile, &ModelSize, 0x48, sizeof(ModelSize));

	free(ModelTextureTable);
	free(Textures);
	fclose(ptrInFile);
	fclose(ptrOutFile);

	puts("Done!\n\n");
}

void ConvertMDLToDOL(const char * FileName)
{
	sModelHeader ModelHeader;
	sModelTextureEntry * ModelTextureTable;
	ulong ModelTextureTableSize;
	sDOLTextureHeader DOLTextureHeader;
	sTexture * Textures;

	FILE * ptrInFile;
	FILE * ptrOutFile;
	char cOutFileName[PATH_LEN];
	char cNewModelName[64];
	char cTextureName[64];

	ulong ModelSize;

	SafeFileOpen(&ptrInFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrInFile);

	if (ModelHeader.CheckModel() == NORMAL_MODEL)
	{
		printf("Internal name: %s \nTextures: %i, Texture table offset: 0x%X \n", ModelHeader.Name, ModelHeader.TextureCount, ModelHeader.TextureTableOffset);
	}
	else
	{
		puts("Incorrect model file.");
		return;
	}

	// Print resize mode in use
	if (g_UseScaleResize)
		puts("Resize mode: SCALE (proportional stretch - recommended)");
	else
		puts("Resize mode: TILE (original behaviour)");

	ModelTextureTableSize = ModelHeader.TextureCount * sizeof(sModelTextureEntry);
	ModelTextureTable = (sModelTextureEntry *)malloc(ModelTextureTableSize);
	Textures = (sTexture *)malloc(sizeof(sTexture) * ModelHeader.TextureCount);

	uint BitmapOffset, BitmapSize, PaletteOffset, PaletteSize;
	bool anyBadTex = false;

	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].UpdateFromFile(&ptrInFile, ModelHeader.TextureTableOffset, i);

		char TexExtension[5];
		FileGetExtension(ModelTextureTable[i].Name, TexExtension, sizeof(TexExtension));
		if (!strcmp(TexExtension, ".pvr") == true)
		{
			UTIL_WAIT_KEY("Dreamcast model conversion is not supported ...");
			exit(EXIT_FAILURE);
		}

		BitmapOffset = ModelTextureTable[i].Offset + MDL_TEXTURE_HEADER_SIZE;
		BitmapSize = ModelTextureTable[i].Height * ModelTextureTable[i].Width;
		PaletteOffset = ModelTextureTable[i].Offset + ModelTextureTable[i].Width * ModelTextureTable[i].Height;
		PaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * MDL_PALETTE_ELEMENT_SIZE;

		Textures[i].Initialize();
		Textures[i].UpdateFromFile(&ptrInFile, BitmapOffset, BitmapSize, PaletteOffset, PaletteSize, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height);

		// -------------------------------------------------------
		// Warn about bad dimensions before resize
		// -------------------------------------------------------
		if (Textures[i].WarnIfBadDimensions())
			anyBadTex = true;

		// -------------------------------------------------------
		// Resize: scale or tile depending on flag
		// -------------------------------------------------------
		uint TargetW = PSIProperSize(Textures[i].Width, false);
		uint TargetH = PSIProperSize(Textures[i].Height, false);

		if (TargetW != Textures[i].Width || TargetH != Textures[i].Height)
		{
			if (g_UseScaleResize)
			{
				printf("  Resizing \"%s\" %ux%u -> %ux%u (scale)\n", Textures[i].Name, Textures[i].Width, Textures[i].Height, TargetW, TargetH);
				Textures[i].ScaleResize(TargetW, TargetH);
			}
			else
			{
				printf("  Resizing \"%s\" %ux%u -> %ux%u (tile)\n", Textures[i].Name, Textures[i].Width, Textures[i].Height, TargetW, TargetH);
				Textures[i].TileResize(TargetW, TargetH);
			}
		}

		Textures[i].PaletteReformat(MDL_PALETTE_ELEMENT_SIZE);
		Textures[i].PaletteAddSpacers(0x80);
	}

	if (anyBadTex)
		puts("\n[NOTE] One or more textures had non-power-of-2 dimensions and were resized.\n       If the game crashes, pre-resize textures in an image editor before converting.\n");

	FileGetFullName(FileName, cOutFileName, sizeof(cOutFileName));
	strcat(cOutFileName, ".dol");
	SafeFileOpen(&ptrOutFile, cOutFileName, "wb");

	FileGetName(cOutFileName, cNewModelName, sizeof(cNewModelName), false);
	strcat(cNewModelName, ".dol");
	ModelHeader.Rename(cNewModelName);
	ModelHeader.TextureDataOffset = ModelHeader.TextureTableOffset + sizeof(sModelTextureEntry) * ModelHeader.TextureCount + ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	ModelHeader.TextureDataOffset = (((ModelHeader.TextureDataOffset / 16) + ((ModelHeader.TextureDataOffset % 16) && 1)) * 16);
	FileWriteBlock(&ptrOutFile, (char *)&ModelHeader, sizeof(sModelHeader));

	uchar * ModelData;
	ModelData = (uchar *) malloc(ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	FileReadBlock(&ptrInFile, (char *) ModelData, sizeof(sModelHeader), ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	PatchDOLExtraSection((char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), 0, 0, 0, 0, 0);
	PatchSubmodelRef(&ModelHeader, (char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), ".dol");
	FileWriteBlock(&ptrOutFile, (char *) ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader));
	free(ModelData);

	uint Offset = ModelHeader.TextureTableOffset + sizeof(sModelTextureEntry) * ModelHeader.TextureCount + ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	Offset = ((Offset / 16) + ((Offset % 16) && 1)) * 16;
	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].Width = Textures[i].Width;
		ModelTextureTable[i].Height = Textures[i].Height;
		ModelTextureTable[i].Offset = Offset;
		Offset += sizeof(sDOLTextureHeader) + Textures[i].PaletteSize + Textures[i].Width * Textures[i].Height;
	}
	FileWriteBlock(&ptrOutFile, (char *) ModelTextureTable, ModelTextureTableSize);

	uchar * SkinTable;
	ulong SkinTableSize = ModelHeader.SkinCount * ModelHeader.SkinEntrySize * 2;
	SkinTable = (uchar *)malloc(SkinTableSize);
	FileReadBlock(&ptrInFile, SkinTable, ModelHeader.SkinTableOffset, SkinTableSize);
	FileWriteBlock(&ptrOutFile, SkinTable, SkinTableSize);
	free(SkinTable);

	char Spacer = 0x00;
	int SpacersCount = (FileSize(&ptrOutFile) / 16 + ((FileSize(&ptrOutFile) % 16) && 1)) * 16 - FileSize(&ptrOutFile);
	for (int i = 0; i < SpacersCount; i++)
		FileWriteBlock(&ptrOutFile, &Spacer, 1);

	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		FileGetName(Textures[i].Name, cTextureName, sizeof(cTextureName), false);
		DOLTextureHeader.Update(cTextureName, Textures[i].Width, Textures[i].Height);

		FileWriteBlock(&ptrOutFile, &DOLTextureHeader, sizeof(sDOLTextureHeader));
		FileWriteBlock(&ptrOutFile, (char *) Textures[i].Palette, Textures[i].PaletteSize);
		FileWriteBlock(&ptrOutFile, (char *) Textures[i].Bitmap, Textures[i].Width * Textures[i].Height);
	}

	sDOLExtraSection DOLXS;
	sDOLLODEntry * LODTable;
	if (CheckExtraFile(FileName) == true)
	{
		TranslateExtraFile(FileName, &DOLXS, &LODTable);

		DOLXS.LODDataOffset = FileSize(&ptrOutFile);
		FileWriteBlock(&ptrOutFile, &DOLXS, sizeof(sModelHeader), sizeof(DOLXS));

		if (LODTable != NULL)
		{
			FileWriteBlock(&ptrOutFile, LODTable, FileSize(&ptrOutFile), DOLXS.NumBodyGroups * DOLXS.MaxBodyParts * sizeof(sDOLLODEntry));
			free(LODTable);
		}

		uchar Align = (FileSize(&ptrOutFile) % 16) == 0 ? 0 : 16 - (FileSize(&ptrOutFile) % 16);
		for (uchar i = 0; i < Align; i++)
			fputc(0x11, ptrOutFile);
	}

	ModelSize = FileSize(&ptrOutFile);
	FileWriteBlock(&ptrOutFile, &ModelSize, 0x48, sizeof(ModelSize));

	free(ModelTextureTable);
	free(Textures);
	fclose(ptrInFile);
	fclose(ptrOutFile);

	puts("Done!\n\n");
}

void ConvertSubmodel(const char * FileName, char * OriginalExtension, char * TargetExtension)
{
	FILE * ptrModelFile;
	FILE * ptrOutputFile;

	sModelHeader ModelHeader;
	char * ModelData;
	ulong ModelDataSize;

	int ModelType;
	char NewModelName[64];
	char OutputFile[PATH_LEN];

	puts("Patching submodel ...");

	SafeFileOpen(&ptrModelFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrModelFile);

	if (ModelHeader.CheckModel() != NOTEXTURES_MODEL && ModelHeader.CheckModel() != SEQ_MODEL)
	{
		puts("Invalid submodel ...");
		return;
	}

	FileGetFullName(FileName, OutputFile, sizeof(OutputFile));
	strcat(OutputFile, TargetExtension);
	SafeFileOpen(&ptrOutputFile, OutputFile, "wb");

	FileGetName(FileName, NewModelName, sizeof(NewModelName), false);
	strcat(NewModelName, TargetExtension);
	ModelHeader.Rename(NewModelName);
	FileWriteBlock(&ptrOutputFile, (char *)&ModelHeader, sizeof(sModelHeader));

	ModelDataSize = FileSize(&ptrModelFile) - sizeof(sModelHeader);
	ModelData = (char *)malloc(ModelDataSize);
	FileReadBlock(&ptrModelFile, ModelData, sizeof(sModelHeader), ModelDataSize);
	if (ModelHeader.CheckModel() == NOTEXTURES_MODEL)
	{
		PatchSubmodelRef(&ModelHeader, ModelData, ModelDataSize, TargetExtension);
		if (!strcmp(OriginalExtension, ".mdl") == true)
			PatchDOLExtraSection((char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), 0, 0, 0, 0, 0);
		else
			PatchDOLExtraSection((char *)ModelData, ModelHeader.TextureTableOffset - sizeof(sModelHeader), 0x00504453, 0, 0, 0, 0);
	}
	FileWriteBlock(&ptrOutputFile, ModelData, ModelDataSize);

	free(ModelData);
	fclose(ptrModelFile);

	puts("Done!\n\n");
}

void ConvertDummySubmodel(const char * FileName, char * OriginalExtension, char * TargetExtension)
{
	FILE * ptrModelFile;
	FILE * ptrOutputFile;

	char * ModelData;
	ulong ModelDataSize;

	char OutputFile[PATH_LEN];
	char NewInternalName[64];

	puts("Patching dummy submodel ...");

	SafeFileOpen(&ptrModelFile, FileName, "rb");

	FileGetFullName(FileName, OutputFile, sizeof(OutputFile));
	strcat(OutputFile, TargetExtension);
	SafeFileOpen(&ptrOutputFile, OutputFile, "wb");
	FileGetName(OutputFile, NewInternalName, sizeof(NewInternalName), true);

	ModelDataSize = FileSize(&ptrModelFile);
	ModelData = (char *)malloc(ModelDataSize);
	FileReadBlock(&ptrModelFile, ModelData, 0, ModelDataSize);
	for (uchar c = 8; ModelData[c] != '\0'; c++)
		ModelData[c] = '\0';
	strcpy(&ModelData[8], NewInternalName);
	FileWriteBlock(&ptrOutputFile, ModelData, ModelDataSize);

	free(ModelData);
	fclose(ptrModelFile);

	puts("Done!\n\n");
}

void GetExtraDOLData(const char * FileName)
{
	FILE * ptrInFile;
	FILE * ptrOutFile;
	char cOutFileName[PATH_LEN];
	sDOLExtraSection DOLExtraSect;

	SafeFileOpen(&ptrInFile, FileName, "rb");
	FileReadBlock(&ptrInFile, &DOLExtraSect, sizeof(sModelHeader), sizeof(sDOLExtraSection));

	ulong LODTableSize = DOLExtraSect.MaxBodyParts * DOLExtraSect.NumBodyGroups * sizeof(sDOLLODEntry);
	if ((DOLExtraSect.FadeStart != 0 || DOLExtraSect.FadeEnd != 0) || LODTableSize != 0)
	{
		sDOLLODEntry * LODTable;

		puts("Fetching extra data ...");

		FileGetFullName(FileName, cOutFileName, sizeof(cOutFileName));
		strcat(cOutFileName, ".inf");
		SafeFileOpen(&ptrOutFile, cOutFileName, "wb");

		fprintf(ptrOutFile, "\\\\ General data\r\n\r\n");

		if (DOLExtraSect.FadeStart != 0)
			fprintf(ptrOutFile, "%s[%d]\r\n", KWD_FADESTART, DOLExtraSect.FadeStart);
		if (DOLExtraSect.FadeEnd != 0)
			fprintf(ptrOutFile, "%s[%d]\r\n", KWD_FADEEND, DOLExtraSect.FadeEnd);

		if (LODTableSize != 0)
		{
			fprintf(ptrOutFile, "%s[%d]\r\n", KWD_NUMGROUPS, DOLExtraSect.NumBodyGroups);
			fprintf(ptrOutFile, "%s[%d]\r\n\r\n\r\n", KWD_MAXPARTS, DOLExtraSect.MaxBodyParts);

			fprintf(ptrOutFile, "\\\\ LOD table. Distances for each LOD are inside [].\r\n");
			fprintf(ptrOutFile, "\\\\ If you plan to use this model on PC then consider\r\n");
			fprintf(ptrOutFile, "\\\\ decompiling the model and removing LOD body parts.\r\n\r\n");

			LODTable = (sDOLLODEntry *)malloc(LODTableSize);
			if (LODTable == NULL)
			{
				puts("Can't allocate memory!");
				fclose(ptrInFile);
				return;
			}
			FileReadBlock(&ptrInFile, LODTable, DOLExtraSect.LODDataOffset, LODTableSize);

			for (ushort Group = 0, Entry = 0; Group < DOLExtraSect.NumBodyGroups; Group++)
			{
				fprintf(ptrOutFile, "%s\r\n{\r\n", KWD_GROUP);

				for (ushort Part = 0; Part < DOLExtraSect.MaxBodyParts; Part++, Entry++)
				{
					if (LODTable[Entry].LODCount != 0)
					{
						fprintf(ptrOutFile, "%s[%d", KWD_PART, LODTable[Entry].LODDistances[0]);
						for (uint Dist = 1; Dist < LODTable[Entry].LODCount; Dist++)
							fprintf(ptrOutFile, ",%d", LODTable[Entry].LODDistances[Dist]);
						fprintf(ptrOutFile, "]\r\n");
					}
					else
					{
						fprintf(ptrOutFile, "%s\r\n", KWD_BLANK);
					}
				}

				fprintf(ptrOutFile, "}\r\n\r\n");
			}
		}

		fclose(ptrOutFile);
	}

	fclose(ptrInFile);
}

bool AddTerminator(char * Buffer, char Symbol)
{
	bool Result = false;
	for (ushort i = 0; Buffer[i] != '\0'; i++)
	{
		if (Buffer[i] == Symbol)
		{
			Buffer[i] = '\0';
			Result = true;
			break;
		}
	}
	return Result;
}

ushort CountSymbols(char * Buffer, char Symbol)
{
	ushort Result = 0;
	for (ushort i = 0; Buffer[i] != '\0'; i++)
		if (Buffer[i] == Symbol)
			Result++;
	return Result;
}

bool CheckExtraFile(const char * FileName)
{
	char cInFileName[PATH_LEN];
	FILE * ptrInFile;
	char Buffer[128];
	ushort Open = 0, Close = 0;

	puts("Checking *.INF file ...");

	FileGetFullName(FileName, cInFileName, sizeof(cInFileName));
	strcat(cInFileName, ".inf");
	ptrInFile = fopen(cInFileName, "rb");
	if (ptrInFile == NULL)
		return false;

	Open = Close = 0;
	while (fgets(Buffer, sizeof(Buffer), ptrInFile) != NULL)
	{
		ushort Len = strlen(Buffer);
		if (Buffer[0] == '\\' && Buffer[1] == '\\')
			continue;
		for (ushort i = 0; i < Len; i++)
		{
			if (Buffer[i] == '[') Open++;
			if (Buffer[i] == ']')
			{
				if (Open == (Close + 1)) Close++;
				else
				{
					puts("Invalid [] order !");
					fclose(ptrInFile);
					return false;
				}
			}
		}
		if (Open != Close)
		{
			puts("Both '[' and ']' should be on one line !");
			fclose(ptrInFile);
			return false;
		}
	}

	if (Open == 0 && Close == 0)
	{
		puts("*.INF file is empty, skipping ...");
		fclose(ptrInFile);
		return false;
	}

	if (Open != Close)
	{
		puts("Number of '[' not matches ']' ...");
		fclose(ptrInFile);
		return false;
	}

	printf("Found %d active lines \n", Open);
	fseek(ptrInFile, 0, SEEK_SET);

	Open = Close = 0;
	while (fgets(Buffer, sizeof(Buffer), ptrInFile) != NULL)
	{
		AddTerminator(Buffer, '\r');
		AddTerminator(Buffer, '\n');
		if (Buffer[0] == '\\' && Buffer[1] == '\\') continue;
		if (!strcmp(Buffer, "{") == true) Open++;
		if (!strcmp(Buffer, "}") == true)
		{
			if (Open == (Close + 1)) Close++;
			else
			{
				puts("Invalid {} order !");
				fclose(ptrInFile);
				return false;
			}
		}
	}

	if (Open != Close)
	{
		puts("Number of '{' not matches '}' !");
		fclose(ptrInFile);
		return false;
	}

	printf("Found %d body groups \n", Open);
	fclose(ptrInFile);
	return true;
}

void GetValues(char * Buffer, ulong * Values, uchar ValuesCount)
{
	ushort Len = strlen(Buffer);
	memset(Values, 0x00, ValuesCount * sizeof(ulong));

	char NumBuffer[80];
	uchar NumStart = 0;
	uchar CurrentVal = 0;
	for (ushort i = 0; i < Len; i++)
	{
		if (Buffer[i] == '[') NumStart = i + 1;
		else if (Buffer[i] == ',' || Buffer[i] == ']')
		{
			memset(NumBuffer, 0x00, sizeof(NumBuffer));
			if ((i - NumStart) != 0)
			{
				if (CurrentVal >= ValuesCount)
				{
					puts("Too many values, skipping the rest of them ...\n");
					break;
				}
				memcpy(NumBuffer, &Buffer[NumStart], i - NumStart);
				sscanf(NumBuffer, "%d", &Values[CurrentVal]);
				CurrentVal++;
				NumStart = i + 1;
			}
		}
		if (Buffer[i] == ']') break;
	}
}

bool TranslateExtraFile(const char * FileName, sDOLExtraSection * DOLExtraSect, sDOLLODEntry ** LODTable)
{
	FILE * ptrInFile;
	char cInFileName[PATH_LEN];
	char Buffer[128] = "Text";
	char PrevBuffer[128] = "Text";
	bool Group = true;

	FileGetFullName(FileName, cInFileName, sizeof(cInFileName));
	strcat(cInFileName, ".inf");
	SafeFileOpen(&ptrInFile, cInFileName, "rb");

	memset(DOLExtraSect, 0x00, sizeof(sDOLExtraSection));

	Group = false;
	while (fgets(Buffer, sizeof(Buffer), ptrInFile) != NULL)
	{
		AddTerminator(Buffer, '\r');
		AddTerminator(Buffer, '\n');
		if ((Buffer[0] == '\\' && Buffer[1] == '\\') || Buffer[0] == '\0') { PrevBuffer[0] = '\0'; continue; }

		if (Buffer[0] == '{' && Buffer[1] == '\0') Group = true;
		else if (Buffer[0] == '}' && Buffer[1] == '\0') Group = false;
		else
		{
			if (Group == false && CountSymbols(Buffer, '[') != 0)
			{
				ulong Value = 0;
				GetValues(Buffer, &Value, 1);
				AddTerminator(Buffer, '[');

				if (!strcmp(Buffer, KWD_FADESTART) == true) { DOLExtraSect->FadeStart = Value; if (DOLExtraSect->FadeEnd == 0) DOLExtraSect->FadeEnd = Value; puts("Got fade start value ..."); }
				else if (!strcmp(Buffer, KWD_FADEEND) == true) { DOLExtraSect->FadeEnd = Value; if (DOLExtraSect->FadeStart == 0) DOLExtraSect->FadeStart = Value; puts("Got fade end value ..."); }
				else if (!strcmp(Buffer, KWD_NUMGROUPS) == true) { DOLExtraSect->NumBodyGroups = Value; puts("Got number of body groups ..."); }
				else if (!strcmp(Buffer, KWD_MAXPARTS) == true) { DOLExtraSect->MaxBodyParts = Value; puts("Got maximum number of body parts ..."); }
			}
		}
	}

	if (DOLExtraSect->NumBodyGroups * DOLExtraSect->MaxBodyParts == 0)
	{
		*LODTable = NULL;
		fclose(ptrInFile);
		return true;
	}

	puts("Fetching LOD table ...");
	rewind(ptrInFile);

	ulong LODTableSz = DOLExtraSect->NumBodyGroups * DOLExtraSect->MaxBodyParts * sizeof(sDOLLODEntry);
	*LODTable = (sDOLLODEntry *) calloc(1, LODTableSz);
	if (*LODTable == NULL) { puts("Can't allocate memory!"); return false; }

	uchar GroupCount = 0, PartCount = 0;
	ushort EntryNum = 0;
	Group = false;
	while (fgets(Buffer, sizeof(Buffer), ptrInFile) != NULL)
	{
		AddTerminator(Buffer, '\r');
		AddTerminator(Buffer, '\n');
		if ((Buffer[0] == '\\' && Buffer[1] == '\\') || Buffer[0] == '\0') { PrevBuffer[0] = '\0'; continue; }

		if (Buffer[0] == '{' && Buffer[1] == '\0')
		{
			printf("Fetching group #%d: \t\"%s\"\n", GroupCount, PrevBuffer);
			if (GroupCount >= DOLExtraSect->NumBodyGroups) { printf("Group #%d is out of bounds\n", GroupCount); break; }
			Group = true; PartCount = 0;
			EntryNum = GroupCount * DOLExtraSect->MaxBodyParts;
		}
		else if (Buffer[0] == '}' && Buffer[1] == '\0')
		{
			Group = false; GroupCount++;
		}
		else if (Group == true)
		{
			if (CountSymbols(Buffer, '[') != 0)
			{
				if (PartCount >= DOLExtraSect->MaxBodyParts) { printf("Part #%d is out of bounds, skipping ...\n", PartCount); PartCount++; EntryNum++; continue; }
				uchar ValuesCnt = CountSymbols(Buffer, ',') + 1;
				if (ValuesCnt > 4) ValuesCnt = 4;
				(*LODTable)[EntryNum].LODCount = ValuesCnt;
				GetValues(Buffer, (*LODTable)[EntryNum].LODDistances, ValuesCnt);
				AddTerminator(Buffer, '[');
				printf("Fetched part #%d: \t\"%s\"\n", PartCount, Buffer);
				PartCount++; EntryNum++;
			}
			else
			{
				if (PartCount >= DOLExtraSect->MaxBodyParts) { printf("Part #%d is out of bounds, skipping ...\n", PartCount); PartCount++; EntryNum++; continue; }
				printf("Fetched blank part #%d\n", PartCount);
				PartCount++; EntryNum++;
			}
		}

		strcpy(PrevBuffer, Buffer);
	}

	fclose(ptrInFile);
	return true;
}

void PatchSubmodelRef(sModelHeader * MdlHdr, char * ModelData, ulong ModelDataSize, char * NewExtension)
{
	if (MdlHdr->SubmodelCount <= 1) return;
	if (ModelDataSize < 4 || strlen(NewExtension) != 4) return;

	printf("Found %i internal submodel reference(s), patching ...\n", MdlHdr->SubmodelCount - 1);

	ulong Offset = MdlHdr->SubmodelTableOffset - sizeof(sModelHeader) + MDL_DEF_REF_SZ + MDL_FILE_REF_SPACE;

	char * RefName;
	for (ulong i = 0; i < MdlHdr->SubmodelCount - 1; i++, Offset += (MDL_FILE_REF_SZ + MDL_FILE_REF_SPACE))
	{
		if (Offset + MDL_FILE_REF_SZ > ModelDataSize) { puts("Oops, model data is too small. Patching failed ..."); return; }

		RefName = &ModelData[Offset];
		char Counter = 0;
		while (*RefName != '\0')
		{
			RefName++;
			Counter++;
			if (Counter > MDL_FILE_REF_SZ) { puts("Oops, erroneous reference. Patching failed ..."); return; }
		}
		RefName -= 4;
		memcpy(RefName, NewExtension, 4);
	}
}

uint PSIProperSize(uint Size, bool ToLower)
{
	uint CurrentSize = PSI_MIN_DIMENSION;
	while (CurrentSize < Size)
		CurrentSize <<= 1;

	if (Size <= PSI_MIN_DIMENSION) return PSI_MIN_DIMENSION;
	else
	{
		if (Size == CurrentSize) return CurrentSize;
		else
		{
			if (ToLower == true) return CurrentSize >>= 1;
			else return CurrentSize;
		}
	}
}

void ExtractDOLTextures(const char * FileName)
{
	sModelHeader ModelHeader;
	sModelTextureEntry * ModelTextureTable;
	ulong ModelTextureTableSize;
	sTexture * Textures;

	FILE * ptrInFile;
	sBMPHeader BMPHeader;
	FILE * ptrBMPOutput;
	char cOutFileName[PATH_LEN];
	char cOutFolderName[PATH_LEN];

	SafeFileOpen(&ptrInFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrInFile);

	if (ModelHeader.CheckModel() == NORMAL_MODEL)
		printf("Internal name: %s \nTextures: %i, Texture table offset: 0x%X \n", ModelHeader.Name, ModelHeader.TextureCount, ModelHeader.TextureTableOffset);
	else { puts("Can't extract textures."); return; }

	ModelTextureTable = (sModelTextureEntry *)malloc(ModelHeader.TextureCount * sizeof(sModelTextureEntry));
	Textures = (sTexture *)malloc(sizeof(sTexture) * ModelHeader.TextureCount);

	strcpy(cOutFolderName, FileName);
	strcat(cOutFolderName, "-textures");
	strcat(cOutFolderName, DIR_DELIM);
	NewDir(cOutFolderName);

	uint BitmapOffset, BitmapSize, PaletteOffset, PaletteSize;
	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].UpdateFromFile(&ptrInFile, ModelHeader.TextureTableOffset, i);
		printf(" Texture #%i \n Name: %s \n Width: %i \n Height: %i \n Offset: %x \n\n", i + 1, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height, ModelTextureTable[i].Offset);

		BitmapOffset = ModelTextureTable[i].Offset + DOL_TEXTURE_HEADER_SIZE + EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE;
		BitmapSize = ModelTextureTable[i].Height * ModelTextureTable[i].Width;
		PaletteOffset = ModelTextureTable[i].Offset + DOL_TEXTURE_HEADER_SIZE;
		PaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE;

		Textures[i].Initialize();
		Textures[i].UpdateFromFile(&ptrInFile, BitmapOffset, BitmapSize, PaletteOffset, PaletteSize, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height);

		Textures[i].FlipBitmap();
		Textures[i].PaletteReformat(DOL_BMP_PALETTE_ELEMENT_SIZE);
		Textures[i].PaletteRemoveSpacers();
		Textures[i].PaletteAddSpacers(0x00);
		Textures[i].PaletteSwapRedAndGreen(DOL_BMP_PALETTE_ELEMENT_SIZE);

		strcpy(cOutFileName, cOutFolderName);
		strcat(cOutFileName, ModelTextureTable[i].Name);
		SafeFileOpen(&ptrBMPOutput, cOutFileName, "wb");

		BMPHeader.Update(Textures[i].Width, Textures[i].Height);
		FileWriteBlock(&ptrBMPOutput, (char *) &BMPHeader, sizeof(sBMPHeader));
		FileWriteBlock(&ptrBMPOutput, (char *) Textures[i].Palette, Textures[i].PaletteSize);
		FileWriteBlock(&ptrBMPOutput, (char *) Textures[i].Bitmap, Textures[i].Width * Textures[i].Height);

		fclose(ptrBMPOutput);
	}

	free(ModelTextureTable);
	free(Textures);
	fclose(ptrInFile);
	puts("Done!\n\n");
}

void ExtractMDLTextures(const char * FileName)
{
	sModelHeader ModelHeader;
	sModelTextureEntry * ModelTextureTable;
	ulong ModelTextureTableSize;
	sTexture * Textures;

	FILE * ptrInFile;
	sBMPHeader BMPHeader;
	FILE * ptrBMPOutput;
	char cOutFileName[PATH_LEN];
	char cOutFolderName[PATH_LEN];

	SafeFileOpen(&ptrInFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrInFile);

	if (ModelHeader.CheckModel() == NORMAL_MODEL)
		printf("Internal name: %s \nTextures: %i, Texture table offset: 0x%X \n", ModelHeader.Name, ModelHeader.TextureCount, ModelHeader.TextureTableOffset);
	else { puts("Can't extract textures."); return; }

	ModelTextureTable = (sModelTextureEntry *)malloc(ModelHeader.TextureCount * sizeof(sModelTextureEntry));
	Textures = (sTexture *)malloc(sizeof(sTexture) * ModelHeader.TextureCount);

	strcpy(cOutFolderName, FileName);
	strcat(cOutFolderName, "-textures");
	strcat(cOutFolderName, DIR_DELIM);
	NewDir(cOutFolderName);

	uint BitmapOffset, BitmapSize, PaletteOffset, PaletteSize;
	bool RawExtract = false;
	char TexExtension[5];
	for (int i = 0; i < (int)ModelHeader.TextureCount; i++)
	{
		ModelTextureTable[i].UpdateFromFile(&ptrInFile, ModelHeader.TextureTableOffset, i);
		printf(" Texture #%i \n Name: %s \n Width: %i \n Height: %i \n Offset: %x \n\n", i + 1, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height, ModelTextureTable[i].Offset);

		if (RawExtract == false)
		{
			FileGetExtension(ModelTextureTable[i].Name, TexExtension, sizeof(TexExtension));
			if (!strcmp(TexExtension, ".pvr") == true) { RawExtract = true; puts("Dreamcast textures found ..."); }
		}

		if (RawExtract == false)
		{
			BitmapOffset = ModelTextureTable[i].Offset + MDL_TEXTURE_HEADER_SIZE;
			BitmapSize = ModelTextureTable[i].Height * ModelTextureTable[i].Width;
			PaletteOffset = ModelTextureTable[i].Offset + ModelTextureTable[i].Width * ModelTextureTable[i].Height;
			PaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * MDL_PALETTE_ELEMENT_SIZE;

			Textures[i].Initialize();
			Textures[i].UpdateFromFile(&ptrInFile, BitmapOffset, BitmapSize, PaletteOffset, PaletteSize, ModelTextureTable[i].Name, ModelTextureTable[i].Width, ModelTextureTable[i].Height);

			Textures[i].FlipBitmap();
			Textures[i].PaletteSwapRedAndGreen(MDL_PALETTE_ELEMENT_SIZE);
			Textures[i].PaletteAddSpacers(0x00);

			strcpy(cOutFileName, cOutFolderName);
			strcat(cOutFileName, ModelTextureTable[i].Name);
			SafeFileOpen(&ptrBMPOutput, cOutFileName, "wb");

			BMPHeader.Update(Textures[i].Width, Textures[i].Height);
			FileWriteBlock(&ptrBMPOutput, (char *)&BMPHeader, sizeof(sBMPHeader));
			FileWriteBlock(&ptrBMPOutput, (char *)Textures[i].Palette, Textures[i].PaletteSize);
			FileWriteBlock(&ptrBMPOutput, (char *)Textures[i].Bitmap, Textures[i].Width * Textures[i].Height);
		}
		else
		{
			uchar * pPVR;
			uint PVRSize;

			if (i != (int)ModelHeader.TextureCount - 1)
			{
				ModelTextureTable[i + 1].UpdateFromFile(&ptrInFile, ModelHeader.TextureTableOffset, i + 1);
				PVRSize = ModelTextureTable[i + 1].Offset - ModelTextureTable[i].Offset;
			}
			else
			{
				PVRSize = FileSize(&ptrInFile) - ModelTextureTable[i].Offset;
			}

			pPVR = (uchar *)malloc(PVRSize);
			if (pPVR == NULL) { puts("Unable to allocate memory ..."); exit(EXIT_FAILURE); }

			FileReadBlock(&ptrInFile, pPVR, ModelTextureTable[i].Offset, PVRSize);

			strcpy(cOutFileName, cOutFolderName);
			strcat(cOutFileName, ModelTextureTable[i].Name);
			SafeFileOpen(&ptrBMPOutput, cOutFileName, "wb");

			FileWriteBlock(&ptrBMPOutput, pPVR, PVRSize);
			free(pPVR);
		}

		fclose(ptrBMPOutput);
	}

	free(ModelTextureTable);
	free(Textures);
	fclose(ptrInFile);
	puts("Done!\n\n");
}

void SeqReport(const char * FileName)
{
	sModelHeader ModelHeader;
	sModelSeq * SeqTable;
	ulong SeqTableSz;
	int SeqCount;
	FILE * ptrInFile;
	FILE * ptrOutFile;
	char cOutFileName[PATH_LEN];

	SafeFileOpen(&ptrInFile, FileName, "rb");
	ModelHeader.UpdateFromFile(&ptrInFile);

	if (ModelHeader.CheckModel() == NORMAL_MODEL)
		printf("Internal name: %s \nSequences: %i \n", ModelHeader.Name, ModelHeader.SeqCount);
	else { puts("Bad model file"); return; }

	SeqCount = ModelHeader.SeqCount;
	SeqTableSz = sizeof(sModelSeq) * SeqCount;
	SeqTable = (sModelSeq *) malloc(SeqTableSz);
	FileReadBlock(&ptrInFile, SeqTable, ModelHeader.SeqTableOffset, SeqTableSz);
	fclose(ptrInFile);

	FileGetFullName(FileName, cOutFileName, sizeof(cOutFileName));
	strcat(cOutFileName, "_seq.txt");
	SafeFileOpen(&ptrOutFile, cOutFileName, "w");

	fprintf(ptrOutFile, "File: %s\nSequences: %d\n\n", FileName, SeqCount);
	fprintf(ptrOutFile, "[#]\t[File]\t[Sequence]\n", FileName, SeqCount);
	for (int sq = 0; sq < SeqCount; sq++)
		fprintf(ptrOutFile, "%d\t%d\t%s\n", sq, SeqTable[sq].Num, SeqTable[sq].Name);

	fclose(ptrOutFile);
	free(SeqTable);
	puts("Done!\n\n");
}

int main(int argc, char * argv[])
{
	char cFileExtension[5];

	puts(PROG_TITLE);

	if (argc == 1)
	{
		puts(PROG_INFO);
		UTIL_WAIT_KEY("Press any key to exit ...");
	}
	else if (argc == 2)
	{
		FileGetExtension(argv[1], cFileExtension, 5);
		printf("\nProcessing file: %s\n", argv[1]);

		if (!strcmp(".mdl", cFileExtension))
		{
			if (CheckModel(argv[1]) == NORMAL_MODEL) ConvertMDLToDOL(argv[1]);
			else if (CheckModel(argv[1]) == SEQ_MODEL || CheckModel(argv[1]) == NOTEXTURES_MODEL) ConvertSubmodel(argv[1], ".mdl", ".dol");
			else if (CheckModel(argv[1]) == DUMMY_MODEL) ConvertDummySubmodel(argv[1], ".mdl", ".dol");
			else puts("Can't recognise model file ...");
		}
		else if (!strcmp(".dol", cFileExtension))
		{
			if (CheckModel(argv[1]) == NORMAL_MODEL) ConvertDOLToMDL(argv[1]);
			else if (CheckModel(argv[1]) == SEQ_MODEL || CheckModel(argv[1]) == NOTEXTURES_MODEL) ConvertSubmodel(argv[1], ".dol", ".mdl");
			else if (CheckModel(argv[1]) == DUMMY_MODEL) ConvertDummySubmodel(argv[1], ".dol", ".mdl");
			else puts("Can't recognise model file ...");
		}
		else puts("Wrong file extension.");
	}
	else if (argc == 3 && !strcmp(argv[1], "extract") == true)
	{
		FileGetExtension(argv[2], cFileExtension, 5);
		printf("\nProcessing file: %s\n", argv[2]);

		if (!strcmp(".mdl", cFileExtension))
		{
			if (CheckModel(argv[2]) == NORMAL_MODEL) ExtractMDLTextures(argv[2]);
			else puts("Can't find texture data ...");
		}
		else if (!strcmp(".dol", cFileExtension))
		{
			if (CheckModel(argv[2]) == NORMAL_MODEL) ExtractDOLTextures(argv[2]);
			else puts("Can't find texture data ...");
		}
		else puts("Wrong file extension.");
	}
	else if (argc == 3 && !strcmp(argv[1], "seqrep") == true)
	{
		FileGetExtension(argv[2], cFileExtension, 5);
		printf("\nProcessing file: %s\n", argv[2]);

		if (!strcmp(".mdl", cFileExtension) || !strcmp(".dol", cFileExtension))
			SeqReport(argv[2]);
		else
			puts("Wrong file extension.");
	}
	// -------------------------------------------------------
	// NEW: "scale" mode - use ScaleResize for MDL->DOL
	// Usage: mdltool scale mymodel.mdl
	// -------------------------------------------------------
	else if (argc == 3 && !strcmp(argv[1], "scale") == true)
	{
		g_UseScaleResize = true;
		FileGetExtension(argv[2], cFileExtension, 5);
		printf("\nProcessing file: %s (scale resize mode)\n", argv[2]);

		if (!strcmp(".mdl", cFileExtension))
		{
			if (CheckModel(argv[2]) == NORMAL_MODEL) ConvertMDLToDOL(argv[2]);
			else if (CheckModel(argv[2]) == SEQ_MODEL || CheckModel(argv[2]) == NOTEXTURES_MODEL) ConvertSubmodel(argv[2], ".mdl", ".dol");
			else if (CheckModel(argv[2]) == DUMMY_MODEL) ConvertDummySubmodel(argv[2], ".mdl", ".dol");
			else puts("Can't recognise model file ...");
		}
		else puts("Scale mode only applies to .mdl -> .dol conversion.");
	}
	else
	{
		puts("Can't recognise arguments.");
	}

	return 0;
}
