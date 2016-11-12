#pragma once
#include <Windows.h>
#include <string>

#pragma pack(push, 1)
struct bitmap_header
{
	unsigned short int FileType;
	unsigned long int FileSize;
	unsigned short int Reserved1;
	unsigned short int Reserved2;
	unsigned long int BitmapOffset;
	unsigned long int Size;
	long int Width;
	long int Height;
	unsigned short int Planes;
	unsigned short int BitsPerPixel;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct dib_header
{
	DWORD Size = sizeof(BITMAPINFOHEADER);
	LONG  Width;
	LONG  Height;
	WORD  Planes = 1;
	WORD  BitsPerPixel;
	DWORD Compression = BI_RGB;
	DWORD ImageSize = 0;
	LONG  XPelsPerMeter = 0;
	LONG  YPelsPerMeter = 0;
	DWORD ClrUsed = 0;
	DWORD ClrImportant = 0;
};
#pragma pack(pop)

struct image_bitmap 
{
	HBITMAP BitmapHandle;
	dib_header Header;
	unsigned char* BitmapPixels;
	std::string ImageName;
};

struct bitmap_contexts
{
	image_bitmap CapturedImage;
	HDC HdcSource;
	HDC HdcDest;
	RECT Region;
};

bitmap_contexts InitWindowBitmaps(bitmap_contexts Image);
void UpdateBitmap(bitmap_contexts& Image);
void BGR2Grey(image_bitmap Image);
size_t MatchTemplateSqDiff(image_bitmap Image, image_bitmap TemplateImage);
image_bitmap ResizeImageNN(image_bitmap Image, int Width, int Height);
void RemoveAlphaBlend(image_bitmap Image, RGBQUAD Color, float alpha);
void FreeImageCache();