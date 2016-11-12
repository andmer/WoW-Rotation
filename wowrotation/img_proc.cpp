#include "img_proc.h"
#include "win32_wowrotation.h"
#include "kiss_fft130\_kiss_fft_guts.h"
#include "kiss_fft130\tools\kiss_fftndr.h"
#include <map>

struct cached_image
{
	size_t SumCheck = -1;
	image_bitmap Image;
	kiss_fft_cpx* DiscreteFourier = 0;
};
std::map<size_t, cached_image> ImageCache;
cached_image LastImage;

void
BGR2Grey(image_bitmap Image)
{
	unsigned char Grey;
	int tmp;

	int Width = Image.Header.Width;
	int Height = Image.Header.Height;
	int BytesPerPixel = (Image.Header.BitsPerPixel / 8);
	unsigned char* Pixel = Image.BitmapPixels;

	for (int row = 0; row < Height; row++)
	{
		for (int col = 0; col < Width; col++, Pixel+=BytesPerPixel)
		{		
			tmp = (*(Pixel + 2) << 1);
			tmp += (*(Pixel + 1) << 2) + *(Pixel + 1);
			tmp += (*Pixel);
			Grey = (tmp >> 3);

			*Pixel = Grey;
			*(Pixel + 1) = Grey;
			*(Pixel + 2) = Grey;
		}
	}
}

void
IntegralSqSum(image_bitmap Image, size_t* SumSq)
{
	int Width = Image.Header.Width;
	int Height = Image.Header.Height;
	unsigned char* BitmapPixels = Image.BitmapPixels;

	int SumRowSize = Width + 1;
	int BytesPerPixel = (Image.Header.BitsPerPixel / 8);
	int SrcRowSize = BytesPerPixel * Width;
	
	SumSq += SumRowSize + 1;
	
	for (int row = 0; row < Height; row++, SumSq++)
	{
		size_t SumRow = 0;
		for (int col = 0; col < Width; col++, SumSq++)
		{
			size_t Pix = *BitmapPixels;
			BitmapPixels += BytesPerPixel;
			SumRow += Pix*Pix;
			*SumSq = *(SumSq-SumRowSize)+SumRow;
		}
	}
	int breakpoint = 3;
}

size_t
SqSum(image_bitmap Image)
{
	int Width = Image.Header.Width;
	int Height = Image.Header.Height;
	int BytesPerPixel = (Image.Header.BitsPerPixel / 8);
	unsigned char* Pixel = Image.BitmapPixels;

	size_t SqSum = 0;
	for (int row = 0; row < Height; row++)
		for (int col = 0; col < Width; col++, Pixel += BytesPerPixel)
			SqSum += (*Pixel) * (*Pixel);
	return(SqSum);
}

int
GetDftSize(int Size)
{
	int DftSize;
	for (DftSize = 64; DftSize < INT_MAX; DftSize *= 2)
		if (DftSize >= Size) break;
	return(DftSize);
}

void*
GetDft(image_bitmap Image, int Inverse, int NCols, int NRows, kiss_fft_cpx* InverseThis = 0)
{
	int Dims[2] = { NCols, NRows };
	int NDims = 2;
	kiss_fftndr_cfg State = kiss_fftndr_alloc(Dims, NDims, Inverse, 0, 0);
	kiss_fft_scalar* FftBuffer = new kiss_fft_scalar[NCols*NRows];
	
	void* FftOut;
	if (Inverse == 0)
	{
		size_t OutSize = sizeof(kiss_fft_cpx)*(NCols*(NRows/2+1));
		FftOut = AllocHeap(LPTR, OutSize);
		unsigned char* ImageIter = Image.BitmapPixels;

		for (int y = 0; y < NRows; y++)
			for (int x = 0; x < NCols; x++)
			{
				if (x < Image.Header.Width && y < Image.Header.Height)
				{
					FftBuffer[x + y*NCols] = (float)*ImageIter;
					ImageIter += (Image.Header.BitsPerPixel / 8);
				}
				else
					FftBuffer[x + y*NCols] = 0.0;
			}

		kiss_fftndr(State, FftBuffer, (kiss_fft_cpx*)FftOut);
		int breakhere = 3;
	}
	else
	{
		size_t OutSize = sizeof(kiss_fft_scalar)*NCols*NRows;
		FftOut = AllocHeap(LPTR, OutSize);
		kiss_fftndri(State, InverseThis, (kiss_fft_scalar*)FftOut);
	}

	kiss_fft_free(State);
	delete[] FftBuffer;
	return(FftOut);
}

//TODO: Should make sure this is the correct way to do cross correlation (test against sums of products)
kiss_fft_scalar*
CrossCorr(image_bitmap Image, image_bitmap TemplateImage)
{
	//NOTE: Sizes are powers of 2
	int SizeX = GetDftSize(Image.Header.Width);
	int SizeY = GetDftSize(Image.Header.Height);
	//NOTE: DFTs are cached
	size_t ImCheck = SqSum(Image);
	kiss_fft_cpx* ImageDft;
	kiss_fft_cpx* TemplateDft;
	size_t TmCheck = SqSum(TemplateImage);
	if (LastImage.SumCheck == ImCheck)
	{
		ImageDft = LastImage.DiscreteFourier;
	}
	else
	{
		LastImage.SumCheck = ImCheck;
		ImageDft = (kiss_fft_cpx*)GetDft(Image, 0, SizeX, SizeY);
		if (LastImage.DiscreteFourier != 0)
			DeallocHeap((void*)LastImage.DiscreteFourier);
		LastImage.DiscreteFourier = ImageDft;
	}
	auto TemplateFound = ImageCache.find(TmCheck);
	if (TemplateFound != ImageCache.end())
	{
		TemplateDft = ImageCache[TmCheck].DiscreteFourier;
	}
	else
	{
		TemplateDft = (kiss_fft_cpx*)GetDft(TemplateImage, 0, SizeX, SizeY);
		cached_image ci = {};
		ci.SumCheck = TmCheck;
		ci.Image = TemplateImage;
		ci.DiscreteFourier = TemplateDft;
		ImageCache[TmCheck] = ci;
	}

	kiss_fft_cpx* CorrBuffer = new kiss_fft_cpx[SizeX*(SizeY/2+1)];
	for (int i = 0; i < (SizeX*(SizeY / 2 + 1)); ++i)
	{
		kiss_fft_cpx c = { 0, 0 };
		kiss_fft_cpx conj = { ImageDft[i].r, ImageDft[i].i * -1 };
		C_MUL(c, TemplateDft[i], conj);

		CorrBuffer[i] = c;
	}

	kiss_fft_scalar* Correlations = (kiss_fft_scalar*)GetDft({ 0 }, 1, SizeX, SizeY, CorrBuffer);
	for (int i = 0; i < (SizeX*SizeY); ++i)
	{
		Correlations[i] /= (SizeX*SizeY);
	}

	delete[] CorrBuffer;
	return(Correlations);
}

void
SwapPictures(image_bitmap& FirstImage, image_bitmap& SecondImage)
{
	unsigned char* tPixels = FirstImage.BitmapPixels;
	FirstImage.BitmapPixels = SecondImage.BitmapPixels;
	SecondImage.BitmapPixels = tPixels;

	dib_header tHeader = FirstImage.Header;
	FirstImage.Header = SecondImage.Header;
	SecondImage.Header = tHeader;

	std::string tString = FirstImage.ImageName;
	FirstImage.ImageName = SecondImage.ImageName;
	SecondImage.ImageName = tString;

	HBITMAP tHandle = FirstImage.BitmapHandle;
	FirstImage.BitmapHandle = SecondImage.BitmapHandle;
	SecondImage.BitmapHandle = tHandle;
}

//NOTE: Only matches the first channel of each pixel eg. Blue in bitmaps
size_t
MatchTemplateSqDiff(image_bitmap Image, image_bitmap TemplateImage)
{
	if (TemplateImage.Header.Height > Image.Header.Height || TemplateImage.Header.Width > Image.Header.Width)
		SwapPictures(Image, TemplateImage);

	//NOTE: This means the template can NEVER fit into the main image
	if (Image.Header.Height < TemplateImage.Header.Height || Image.Header.Width < TemplateImage.Header.Width)
		return(_UI64_MAX); 

	kiss_fft_scalar* Corrs = CrossCorr(Image, TemplateImage);

	void* p = AllocHeap(HEAP_ZERO_MEMORY,
		sizeof(size_t)*(Image.Header.Width*Image.Header.Height + (Image.Header.Width) + Image.Header.Height + 1));
	size_t* SumSq = (size_t*)p;

	IntegralSqSum(Image, SumSq);
	size_t TemplateSum = SqSum(TemplateImage);

	int StepRow = Image.Header.Width + 1;
	int MaxCols = Image.Header.Width - TemplateImage.Header.Width + 1;
	int MaxRows = Image.Header.Height - TemplateImage.Header.Height + 1;
	int DftXPad = GetDftSize(MaxCols) - MaxCols;

	long double SqDiff = 0.0;
	long double BestSqDiff = LDBL_MAX;
	long double LargestSqDiff = 0.0;
	for (int row = 0, CorrI = 0; row < MaxRows; row++, SumSq += TemplateImage.Header.Width, CorrI += DftXPad)
	{
		for (int col = 0; col < MaxCols; col++, SumSq++, CorrI++)
		{
			long long int ImageSqSum = *(SumSq + (StepRow * TemplateImage.Header.Height) + TemplateImage.Header.Width) + // bottom right
				*(SumSq) - // top left
				*(SumSq + TemplateImage.Header.Width) - // top right
				*(SumSq + (StepRow*TemplateImage.Header.Height)); // bottom left
			long double c = 2 * Corrs[CorrI];

			SqDiff = ImageSqSum + TemplateSum - c;
			if (BestSqDiff > SqDiff)
				BestSqDiff = SqDiff;	
			if (LargestSqDiff < SqDiff)
				LargestSqDiff = SqDiff;
			if (SqDiff < 0)
				bool t = true;
		}
	}
	DeallocHeap(p);
	DeallocHeap((void*)Corrs);
	
	return((size_t)BestSqDiff);
}

image_bitmap
ResizeImageNN(image_bitmap Image, int Width, int Height)
{
	unsigned char* ResizedImagePixels = (unsigned char*)AllocHeap(LPTR, Width*Height*(Image.Header.BitsPerPixel / 8));
	int XRatio = (int)((Image.Header.Width << 16) / Width) + 1;
	int YRatio = (int)((Image.Header.Height << 16) / Height) + 1;

	int col2, row2;
	int BytesPerPixel = Image.Header.BitsPerPixel / 8;
	for (int col = 0; col < Width; ++col)
	{
		for (int row = 0; row < Height; ++row)
		{
			col2 = ((col*XRatio) >> 16);
			row2 = ((row*YRatio) >> 16);
			*(ResizedImagePixels + (row*Width*BytesPerPixel) + (col * BytesPerPixel)) = *(Image.BitmapPixels + (row2*Image.Header.Width*BytesPerPixel) + (col2*BytesPerPixel));
			*(ResizedImagePixels + (row*Width*BytesPerPixel) + (col * BytesPerPixel) + 1) = *(Image.BitmapPixels + (row2*Image.Header.Width*BytesPerPixel) + (col2*BytesPerPixel) + 1);
			*(ResizedImagePixels + (row*Width*BytesPerPixel) + (col * BytesPerPixel) + 2) = *(Image.BitmapPixels + (row2*Image.Header.Width*BytesPerPixel) + (col2*BytesPerPixel) + 2);
		}
	}
	image_bitmap ResizedImage = Image;
	ResizedImage.BitmapPixels = ResizedImagePixels;
	ResizedImage.Header.Width = Width;
	ResizedImage.Header.Height = Height;

	return(ResizedImage);
}

void
RemoveAlphaBlend(image_bitmap Image, RGBQUAD Color, float alpha)
{
	unsigned char* ImageIter = Image.BitmapPixels;
	int BytesPerPixel = Image.Header.BitsPerPixel / 8;
	alpha /= 255.0;
	for (int i = 0; i < (Image.Header.Width*Image.Header.Height); ++i, ImageIter+=BytesPerPixel)
	{
		*ImageIter = (unsigned char)(((*ImageIter) - (alpha*Color.rgbBlue)) / (1 - alpha));
		*(ImageIter+1) = (unsigned char)(((*(ImageIter + 1)) - (alpha*Color.rgbGreen)) / (1-alpha));
		*(ImageIter + 2) = (unsigned char)(((*(ImageIter + 2)) - (alpha*Color.rgbRed)) / (1-alpha));
	}
}

//NOTE: This is called when a new size for our capture region is set
void
FreeImageCache()
{
	for (auto i : ImageCache)
		DeallocHeap((void*)i.second.DiscreteFourier);
	ImageCache.clear();
}