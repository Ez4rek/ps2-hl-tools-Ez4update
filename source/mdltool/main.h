// Author:	supadupaplex
// Modified:	Added scale resize mode and texture validation warnings
// License:	BSD-3-Clause (check out license.txt)

//
// This file contains all definitions and declarations
//

#ifndef MAIN_H
#define MAIN_H

////////// Includes //////////
#include <stdio.h>		// puts(), printf(), sscanf(), snprintf()
#include <string.h>		// strcpy(), strcat(), strlen(), strtok(), strncpy()
#include <malloc.h>		// malloc(), free()
#include <stdlib.h>		// exit()
#include <math.h>		// round()
#include <ctype.h>		// tolower()

////////// Definitions //////////
#define PROG_TITLE "\nPS2 HL model tool v1.15+\n"
#define PROG_INFO "\
Developed by supadupaplex, 2017-2021\n\
Modified: scale resize + texture warnings\n\
License: BSD-3-Clause (check out license.txt)\n\
\n\
How to use:\n\
1) Windows explorer - drag and drop model file on mdltool.exe\n\
2) Command line/Batch - mdltool [model_file_name]\n\
Optional features:\n\
 - extract textures: mdltool extract [filename]\n\
 - report sequences: mdltool seqrep [filename]\n\
 - use scale resize:  mdltool scale [filename]\n\
\n\
For more info check out readme.txt\n\
"
#define DOL_TEXTURE_HEADER_SIZE 0x20
#define BMP_TEXTURE_HEADER_SIZE 0x35
#define MDL_TEXTURE_HEADER_SIZE 0x00
#define EIGHT_BIT_PALETTE_ELEMENTS_COUNT 256
#define DOL_BMP_PALETTE_ELEMENT_SIZE 4
#define MDL_PALETTE_ELEMENT_SIZE 3
#define PSI_MIN_DIMENSION 8
#define NOTEXTURES_MODEL 0
#define NORMAL_MODEL 1
#define SEQ_MODEL 2
#define DUMMY_MODEL 3
#define UNKNOWN_MODEL -1
#define MDL_DEF_REF_SZ 0x68
#define MDL_FILE_REF_SZ 0x48
#define MDL_FILE_REF_SPACE 0x20

// Keywords
#define KWD_FADESTART "fadestart"
#define KWD_FADEEND "fadeend"
#define KWD_MAXPARTS "maxparts"
#define KWD_NUMGROUPS "groups"
#define KWD_GROUP "group"
#define KWD_PART "part"
#define KWD_BLANK "blank"

////////// Typedefs //////////
#include "types.h"

////////// Functions //////////
#include "fops.h"

////////// Structures //////////

// MDL/DOL model header
#pragma pack(1)				// Eliminate unwanted 0x00 bytes
struct sModelHeader
{
	char Signature[4];		// "IDST"
	ulong Version;			// 0xA - GoldSrc model
	char Name[64];			// Internal model name
	ulong FileSize;			// Model file size
	char SomeData1[88];		// Data that is not important for conversion
	ulong SeqCount;			// How many sequences
	ulong SeqTableOffset;		// Location of sequence table 
	ulong SubmodelCount;		// How many submodels
	ulong SubmodelTableOffset;	// Location of submodel table 
	ulong TextureCount;		// How many textures
	ulong TextureTableOffset;	// Texture table location
	ulong TextureDataOffset;	// Texture data location
	ulong SkinCount;		// How many skins
	ulong SkinEntrySize;		// Size of entry in skin table (measured in shorts)
	ulong SkinTableOffset;		// Location of skin table
	ulong SubmeshCount;		// How many submeshes
	ulong SubmeshTableOffset;	// Location of submesh table
	char SomeData2[32];		// Data that is not important for conversion

	void UpdateFromFile(FILE ** ptrFile)	// Update header from file
	{
		FileReadBlock(ptrFile, this, 0, sizeof(sModelHeader));
		Name[63] = '\0';
	}

	int CheckModel()			// Check model type
	{
		if (this->Signature[0] == 'I' && this->Signature[1] == 'D' && this->Signature[2] == 'S' && this->Version == 0xA)
		{
			if (this->Signature[3] == 'T')
			{
				if (this->TextureCount > 0)
					return NORMAL_MODEL;
				else
					return NOTEXTURES_MODEL;
			}
			else if (this->Signature[3] == 'Q')
			{
				return SEQ_MODEL;
			}
		}

		return UNKNOWN_MODEL;
	}

	void Rename(const char * NewName)
	{
		memset(this->Name, 0x00, sizeof(this->Name));
		strcpy(this->Name, NewName);
	}
};

// MDL/DOL sequence descriptor
#pragma pack(1)				// No padding/spacers
struct sModelSeq
{
	char Name[32];			// Sequence name
	char SomeData1[124];
	int Num;			// Sequence file number
	char SomeData2[16];
};


// Extra section of DOL model headers
#pragma pack(1)				// Eliminate unwanted 0x00 bytes
struct sDOLExtraSection
{
	ulong LODDataOffset;		// Points to a location of a LOD data section
	uchar MaxBodyParts;		// Maximum number of body parts inside one body group
	uchar NumBodyGroups;		// How many body groups are present in the model
	uchar Magic[2];			// Filled with zeroes
	ulong FadeStart;		// Model fade: start distance
	ulong FadeEnd;			// Model fade: end distance
};

// DOL model LOD table entry
#pragma pack(1)				// Eliminate unwanted 0x00 bytes
struct sDOLLODEntry
{
	ulong LODCount;			// Number of LODs for the specific body part
	ulong LODDistances[4];		// Distances at which corresponding LODs are displayed
};

// MDL/DOL texture table entry
#pragma pack(1)				// Eliminate unwanted 0x00 bytes
struct sModelTextureEntry
{
	char Name[68];			// Texture name
	ulong Width;			// Texture width
	ulong Height;			// Texture height
	ulong Offset;			// Texture offset (in bytes)

	void UpdateFromFile(FILE ** ptrFile, ulong TextureTableOffset, ulong TextureTableEntryNumber)
	{
		FileReadBlock(ptrFile, this, TextureTableOffset + TextureTableEntryNumber * sizeof(sModelTextureEntry), sizeof(sModelTextureEntry));
	}

	void Update(const char * NewName, ulong NewWidth, ulong NewHeight, ulong NewOffset)
	{
		memset(this, 0x00, sizeof(sModelTextureEntry));
		strcpy(this->Name, NewName);
		this->Width = NewWidth;
		this->Height = NewHeight;
		this->Offset = NewOffset;
	}
};

// 8-bit *.bmp header
#pragma pack(1)				// Fix unwanted 0x00 bytes in structure
struct sBMPHeader
{
	char Signature1[2];		// "BM" Signature
	ulong FileSize;			// Total file size (in bytes)
	ulong Signature2;		// 0x00000000
	ulong Offset;			// Offset
	ulong StructSize;		// BMP version
	ulong Width;			// Picture Width (in pixels)
	ulong Height;			// Picture Height (in pixels)
	ushort Signature3;
	ushort BitsPerPixel;		// How many bits per 1 pixel
	ulong Compression;		// Compression type
	ulong PixelDataSize;		// Size of bitmap
	ulong HorizontalPPM;
	ulong VerticalPPM;
	ulong ColorTabSize;
	ulong ColorTabAlloc;

	void Update(unsigned long int Width, unsigned long int Height)
	{
		this->Signature1[0] = 'B';
		this->Signature1[1] = 'M';
		this->Signature2 = 0x00000000;
		this->Offset = 0x00000436;
		this->StructSize = 0x00000028;
		this->Signature3 = 0x0001;
		this->BitsPerPixel = 0x0008;
		this->Compression = 0x00000000;
		this->HorizontalPPM = 0x00000000;
		this->VerticalPPM = 0x00000000;
		this->ColorTabSize = 0x00000100;
		this->ColorTabAlloc = 0x00000100;
		this->Width = Width;
		this->Height = Height;
		this->PixelDataSize = Height * Width;
		this->FileSize = this->PixelDataSize + this->Offset;
	}
};

// DOL texture (psi) header
struct sDOLTextureHeader
{
	char Name[16];
	ulong LODCount;
	ulong Type;
	ushort Width;
	ushort Height;
	ushort UpWidth;
	ushort UpHeight;

	void Update(const char * NewName, ushort NewWidth, ushort NewHeight)
	{
		memset(this, 0x00, sizeof(sDOLTextureHeader));
		snprintf(this->Name, sizeof(this->Name), "%s", NewName);
		this->LODCount = 0;
		this->Type = 2;
		this->Width = NewWidth;
		this->Height = NewHeight;
		this->UpWidth = NewWidth;
		this->UpHeight = NewHeight;
	}
};

// -------------------------------------------------------
// Helper: check if a value is a power of 2 (and >= PSI_MIN_DIMENSION)
// -------------------------------------------------------
static inline bool IsPowerOf2Valid(uint v)
{
	return (v >= PSI_MIN_DIMENSION) && ((v & (v - 1)) == 0);
}

// Model texture data
#pragma pack(1)				// Eliminate unwanted 0x00 bytes
struct sTexture
{
	char Name[64];			// Texture name
	ulong Width;			// Texture width (in pixels)
	ulong Height;			// Texture height (in pixels)
	uchar * Palette;		// Texture palette
	ulong PaletteSize;		// Texture palette size
	uchar * Bitmap;			// Pointer to bitmap

	void Initialize()
	{
		strcpy(this->Name, "New_Texture");
		this->Width = 0;
		this->Height = 0;
		this->Palette = NULL;
		this->PaletteSize = 0;
		this->Bitmap = NULL;
	}

	void UpdateFromFile(FILE ** ptrFile, ulong FileBitmapOffset, ulong FileBitmapSize, ulong FilePaletteOffset, ulong FilePaletteSize, const char * NewName, ulong NewWidth, ulong NewHeight)
	{
		free(Palette);
		free(Bitmap);

		Palette = (uchar *) malloc(FilePaletteSize);
		Bitmap = (uchar *) malloc(FileBitmapSize);
		if (Palette == NULL || Bitmap == NULL)
		{
			UTIL_WAIT_KEY("Unable to allocate memory ...");
			exit(EXIT_FAILURE);
		}

		FileReadBlock(ptrFile, (char *) Palette, FilePaletteOffset, FilePaletteSize);
		FileReadBlock(ptrFile, (char *) Bitmap, FileBitmapOffset, FileBitmapSize);

		strcpy(this->Name, NewName);
		this->Width = NewWidth;
		this->Height = NewHeight;
		this->PaletteSize = FilePaletteSize;
	}

	// -------------------------------------------------------
	// ORIGINAL: tile resize (repeats content to fill new size)
	// -------------------------------------------------------
	void TileResize(ulong NewWidth, ulong NewHeight)
	{
		char * NewBitmap;

		NewBitmap = (char *)malloc(NewWidth * NewHeight);
		if (NewBitmap == NULL)
		{
			UTIL_WAIT_KEY("Unable to allocate memory ...");
			exit(EXIT_FAILURE);
		}

		uint OldX = 0, OldY = 0;
		for (int NewY = 0; NewY < (int)NewHeight; NewY++)
		{
			OldX = 0;
			for (int NewX = 0; NewX < (int)NewWidth; NewX++)
			{
				NewBitmap[(NewWidth * NewY) + NewX] = this->Bitmap[(this->Width * OldY) + OldX];
				if (++OldX >= this->Width) OldX = 0;
			}
			if (++OldY >= this->Height) OldY = 0;
		}

		free(this->Bitmap);
		this->Bitmap = (uchar *) NewBitmap;
		this->Width = NewWidth;
		this->Height = NewHeight;
	}

	// -------------------------------------------------------
	// NEW: scale resize (nearest-neighbour proportional stretch)
	// Avoids seam artefacts when original size is not a multiple
	// of the target power-of-2. Recommended for conversion.
	// -------------------------------------------------------
	void ScaleResize(ulong NewWidth, ulong NewHeight)
	{
		char * NewBitmap;

		NewBitmap = (char *) malloc(NewWidth * NewHeight);
		if (NewBitmap == NULL)
		{
			UTIL_WAIT_KEY("Unable to allocate memory ...");
			exit(EXIT_FAILURE);
		}

		for (ulong NewY = 0; NewY < NewHeight; NewY++)
		{
			// Map new row -> old row using integer fixed-point arithmetic
			// to avoid floating-point rounding drift across thousands of pixels
			ulong OldY = (NewY * this->Height) / NewHeight;
			for (ulong NewX = 0; NewX < NewWidth; NewX++)
			{
				ulong OldX = (NewX * this->Width) / NewWidth;
				NewBitmap[(NewWidth * NewY) + NewX] = this->Bitmap[(this->Width * OldY) + OldX];
			}
		}

		free(this->Bitmap);
		this->Bitmap = (uchar *) NewBitmap;
		this->Width = NewWidth;
		this->Height = NewHeight;
	}

	// -------------------------------------------------------
	// Warn if texture dimensions are not power-of-2.
	// Returns true if the texture needs resizing.
	// -------------------------------------------------------
	bool WarnIfBadDimensions()
	{
		bool bad = false;
		if (!IsPowerOf2Valid(this->Width))
		{
			printf("  [WARNING] Texture \"%s\": width %u is not a power of 2. Will be resized.\n", this->Name, this->Width);
			bad = true;
		}
		if (!IsPowerOf2Valid(this->Height))
		{
			printf("  [WARNING] Texture \"%s\": height %u is not a power of 2. Will be resized.\n", this->Name, this->Height);
			bad = true;
		}
		return bad;
	}

	void FlipBitmap()
	{
		char * NewBitmap;

		NewBitmap = (char *)malloc(this->Width * this->Height);
		if (NewBitmap == NULL)
		{
			UTIL_WAIT_KEY("Unable to allocate memory ...");
			exit(EXIT_FAILURE);
		}

		for (int y = 0; y < (int)this->Height; y++)
			for (int x = 0; x < (int)this->Width; x++)
				NewBitmap[(this->Width * y) + x] = this->Bitmap[(this->Width * ((this->Height - 1) - y)) + x];

		free(this->Bitmap);
		this->Bitmap = (uchar *) NewBitmap;
	}

	void PaletteReformat(uint PaletteElementSize)
	{
		uchar Remainder;
		uchar Temp;

		for (ulong i = 0; i < this->PaletteSize; i++)
		{
			Remainder = i % (0x20 * PaletteElementSize);

			if (((0x10 * PaletteElementSize) <= Remainder) && (Remainder < (0x18 * PaletteElementSize)))
			{
				Temp = this->Palette[i];
				this->Palette[i] = this->Palette[i - (0x08 * PaletteElementSize)];
				this->Palette[i - (0x08 * PaletteElementSize)] = Temp;
			}
		}
	}

	void PaletteRemoveSpacers()
	{
		char * NewPalette;
		ulong NewPaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * MDL_PALETTE_ELEMENT_SIZE;

		if (this->PaletteSize == EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE)
		{
			NewPalette = (char *)malloc(NewPaletteSize);
			if (NewPalette == NULL)
			{
				UTIL_WAIT_KEY("Unable to allocate memory ...");
				exit(EXIT_FAILURE);
			}

			int ByteCounterOld = 0;
			int ByteCounterNew = 0;
			for (ByteCounterOld = 0; ByteCounterOld < (int)this->PaletteSize; ByteCounterOld++)
			{
				if ((ByteCounterOld + 1) % 4 != 0)
				{
					NewPalette[ByteCounterNew] = this->Palette[ByteCounterOld];
					ByteCounterNew++;
				}
			}

			free(this->Palette);
			this->Palette = (uchar *) NewPalette;
			this->PaletteSize = NewPaletteSize;
		}
	}

	void PaletteAddSpacers(char Spacer)
	{
		char * NewPalette;
		ulong NewPaletteSize = EIGHT_BIT_PALETTE_ELEMENTS_COUNT * DOL_BMP_PALETTE_ELEMENT_SIZE;

		if (this->PaletteSize == EIGHT_BIT_PALETTE_ELEMENTS_COUNT * MDL_PALETTE_ELEMENT_SIZE)
		{
			NewPalette = (char *)malloc(NewPaletteSize);
			if (NewPalette == NULL)
			{
				UTIL_WAIT_KEY("Unable to allocate memory ...");
				exit(EXIT_FAILURE);
			}

			int ByteCounterOld = 0;
			int ByteCounterNew = 0;
			for (ByteCounterNew = 0; ByteCounterNew < (int)NewPaletteSize; ByteCounterNew++)
			{
				if ((ByteCounterNew + 1) % 4 != 0)
				{
					NewPalette[ByteCounterNew] = this->Palette[ByteCounterOld];
					ByteCounterOld++;
				}
				else
				{
					NewPalette[ByteCounterNew] = Spacer;
				}
			}

			free(this->Palette);
			this->Palette = (uchar *) NewPalette;
			this->PaletteSize = NewPaletteSize;
		}
	}

	void PaletteSwapRedAndGreen(int ElementSize)
	{
		char Temp;

		for (int Element = 0; Element < EIGHT_BIT_PALETTE_ELEMENTS_COUNT; Element++)
		{
			Temp = this->Palette[Element * ElementSize];
			this->Palette[Element * ElementSize] = this->Palette[Element * ElementSize + 2];
			this->Palette[Element * ElementSize + 2] = Temp;
		}
	}
};

#endif // MAIN_H
