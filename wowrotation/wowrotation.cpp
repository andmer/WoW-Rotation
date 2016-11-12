#include "wowrotation.h"
#include "img_proc.h"
#include <ctime>

std::vector<image_bitmap>
InitTemplateImages()
{
	std::string PathToImages = "abilities/";
	std::vector<std::string> BMNames = GetAllFileNamesInDirectory(PathToImages + "*.bmp");

	std::vector<image_bitmap> TemplateImages;
	for (auto Name : BMNames)
	{
		void* Data = ReadEntireFile(PathToImages + Name);

		if (Data)
		{
			bitmap_header BMHeader = *(bitmap_header *)Data;
			unsigned char* Pixels = (unsigned char*)Data + BMHeader.BitmapOffset;

			//NOTE: make our origin in top left by flipping vertically
			int BytesPerPix = BMHeader.BitsPerPixel / 8;
			int LastRow = (BytesPerPix * BMHeader.Width) * (BMHeader.Height - 1);
			for (int x = 0; x < BMHeader.Width; ++x)
			{
				for (int y = 0; y < (BMHeader.Height / 2); ++y)
				{
					int Row = (BytesPerPix * BMHeader.Width) * y;
					int Col = BytesPerPix * x;
					int Index = Row + Col;
					std::swap(*(Pixels + Index), *(Pixels + LastRow + Col - Row));
					std::swap(*(Pixels + Index + 1), *(Pixels + LastRow + Col - Row + 1));
					std::swap(*(Pixels + Index + 2), *(Pixels + LastRow + Col - Row + 2));
				}
			}

			dib_header DibHeader = {};
			DibHeader.Width = BMHeader.Width;
			DibHeader.Height = BMHeader.Height;
			DibHeader.BitsPerPixel = BMHeader.BitsPerPixel;

			image_bitmap Img = {};
			Img.BitmapPixels = Pixels;
			Img.Header = DibHeader;
			Img.ImageName = Name;
			//BGR2Grey(Img);

			//DeallocHeap(Data);
			TemplateImages.push_back(Img);
		}
	}


	return(TemplateImages);
}

bitmap_contexts
InitMainImage()
{
	bitmap_contexts MainImage = {};

	MainImage.HdcSource = GetDC(GetDesktopWindow());
	MainImage = InitWindowBitmaps(MainImage);

	return(MainImage);
}

void
DisplayMainImage(bitmap_contexts& BMContext, win32_handles Win32Handles)
{
	UpdateBitmap(BMContext);

	image_bitmap BMCopy = {};
	BMCopy.BitmapHandle = BMContext.CapturedImage.BitmapHandle;
	BMCopy.Header = BMContext.CapturedImage.Header;
	BMCopy.ImageName = BMContext.CapturedImage.ImageName;
	BMCopy.BitmapPixels = (unsigned char*)AllocHeap(LPTR, BMCopy.Header.Width*BMCopy.Header.Height*(BMCopy.Header.BitsPerPixel / 8));
	memcpy(BMCopy.BitmapPixels, BMContext.CapturedImage.BitmapPixels, BMCopy.Header.Width*BMCopy.Header.Height*(BMCopy.Header.BitsPerPixel / 8));

	if (IsWindowVisible(Win32Handles.RectSelector))
	{
		RemoveAlphaBlend(BMCopy, RGBQUAD{ 100, 100, 230, 0 }, 150.0);
	}
	DrawBitmap(BMCopy, 15, 15, Win32Handles.MainWindow, true);
}

int
GetNextKey(image_bitmap& Image, std::vector<image_bitmap> TemplateImages, win32_handles Win32Handles)
{
	
	if (IsWindowVisible(Win32Handles.RectSelector))
		return(-1);
	//BGR2Grey(Image);

	image_bitmap BestMatch;
	size_t BestScore = _UI64_MAX;
	for (auto i : TemplateImages)
	{
		image_bitmap ResizedI = ResizeImageNN(i, Image.Header.Width, Image.Header.Height);
		size_t Diff = MatchTemplateSqDiff(Image, ResizedI);
		if (BestScore > Diff)
		{
			BestScore = Diff;
			BestMatch = i;
		}
		DeallocHeap((void*)ResizedI.BitmapPixels);
	}
	DrawBitmap(BestMatch, 185, 15, Win32Handles.MainWindow);
	size_t ExtDot = BestMatch.ImageName.find_last_of(".");
	int Key = -1;
	if (ExtDot != std::string::npos)
	{
		Key = stoi(BestMatch.ImageName.substr(0, ExtDot));
	}
	return(Key);
}

int
MainRotationLoop(win32_state* Win32State)
{
	clock_t Time;
	Time = clock();
	int PressInterval = 100; //NOTE: about 100ms but seems to vary

	HWND WoWHandle = FindWindowA(NULL, "World of Warcraft");
	win32_handles Win32Handles = GetWindowHandles();

	bitmap_contexts MainImage = InitMainImage();
	GetWindowRect(Win32Handles.RectSelector, &MainImage.Region);

	std::vector<image_bitmap> TemplateImages = InitTemplateImages();
	bool Running = false;
	bool KeyDown = false;

	while (!Win32State->Quitting)
	{
		if (Win32State->Sizing)
		{
			Win32State->Sizing = false;
			GetWindowRect(Win32Handles.RectSelector, &MainImage.Region);
		}

		if ((clock() - Time) > PressInterval)
		{
			Time = clock();
			DisplayMainImage(MainImage, Win32Handles);
			if (Win32State->RunningRotation && !Win32State->PausedRotation)
			{
				int Key = GetNextKey(MainImage.CapturedImage, TemplateImages, Win32Handles);
				if (Key >= 0 && WoWHandle)
				{
					SendMessage(WoWHandle, WM_KEYDOWN, Key, 0);
					SendMessage(WoWHandle, WM_KEYUP, Key, 0);
				}
			}
		}
		Sleep(1);
	}
	OutputDebugStringA("Quiting rotation thread");

	return(0);
}