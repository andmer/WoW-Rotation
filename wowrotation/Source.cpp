#include <boost/filesystem.hpp>
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <map>
#include <ctime>
using namespace std;
using namespace cv;
using namespace boost::filesystem;

void UpdateImage(HDC hdcSource, HDC hdc, RECT winRect);
Mat GetWindowMat(RECT winRect, HDC hdcSource, HDC hdc);
int TemplateMatchAbility(Mat wowImage, map<string, Mat> templates);
vector<path> GetFilesInDirectory(String dir_path);
map<string, Mat> GetAbilityTemplates();

int xpos = 850, ypos = 620;
//int xpos = 0, ypos = 0;

int main()
{
	clock_t t;
	t = clock();

	bool running = false;
	bool pressed = false;

	//initilize hdcs
	HWND windowhwnd = FindWindowA(NULL, "World of Warcraft");

	HDC hdcSys = GetDC(NULL);
	//HDC hdcSys = GetDCEx(windowhwnd, NULL, DCX_INTERSECTRGN);
	HDC hdcMem = CreateCompatibleDC(hdcSys);

	//RECT windowRect;
	//GetClientRect(windowhwnd, &windowRect);
	//size of capture region
	RECT imageRect = { 0, 0, 40, 40 };

	GetFilesInDirectory("abilities");

	Mat windowImage = GetWindowMat(imageRect, hdcSys, hdcMem);
	map<string, Mat> abilityTemplates = GetAbilityTemplates();
	int key;
	while (true)
	{
		//cout << (clock() - t) << endl;;
		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(0x54) & 0x8000) && !pressed) //ctrl+T
		{
			pressed = true;
			running = !running;	
			system("cls");
			cout << (running ? "Running" : "Stopped") << endl;
		}
		else if (((GetAsyncKeyState(0x54) & 0x8000) == 0) && pressed)
		{
			pressed = false;
		}
		if (running && (clock() - t) > 100) //100ms since last keypress
		{
			t = clock();
			UpdateImage(hdcSys, hdcMem, imageRect);
			key = TemplateMatchAbility(windowImage, abilityTemplates);

			//imshow("title", windowImage);
			//waitKey(1000/60);
			cout << key << " " << "\r" << flush; //extra space because some keys are 3 digits and others a 2
			SendMessage(windowhwnd, WM_KEYDOWN, key, 0);
			SendMessage(windowhwnd, WM_KEYUP, key, 0);
		}
	}
}

vector<path> GetFilesInDirectory(String dir_path)
{
	vector<path> filePaths;
	struct recursive_directory_range
	{
		typedef recursive_directory_iterator iterator;
		recursive_directory_range(path p) : p_(p) {}

		iterator begin() { return recursive_directory_iterator(p_); }
		iterator end() { return recursive_directory_iterator(); }

		path p_;
	};

	for (auto it : recursive_directory_range(dir_path))
	{
		filePaths.push_back(it.path());
	}
	return(filePaths);
}

//<key id, template image>
map<string, Mat> GetAbilityTemplates()
{
	map<string, Mat> templates;
	vector<path> imgPaths = GetFilesInDirectory("abilities");
	for (auto path : imgPaths)
	{
		Mat img = imread(path.string(), 0);
		if (!img.empty())
			templates[path.stem().string()] = img;
	}
	
	return(templates);
}

int TemplateMatchAbility(Mat wowImage, map<string, Mat> templates)
{
	Mat grayWoWImage;
	cvtColor(wowImage, grayWoWImage, CV_BGR2GRAY);
	double bestMax = 0.0;
	string match = "";

	int result_cols = 0, result_rows = 0;
	if (!templates.empty())
	{
		//ability images are all same size so lets just do this once
		int result_cols = abs(grayWoWImage.cols - templates.begin()->second.cols) + 1;
		int result_rows = abs(grayWoWImage.rows - templates.begin()->second.rows) + 1;
	}

	Mat result(result_rows, result_cols, CV_32FC1);
	for (auto t : templates)
	{
		matchTemplate(grayWoWImage, t.second, result, CV_TM_CCOEFF_NORMED);

		double minVal; double maxVal; Point minLoc; Point maxLoc;
		minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc, Mat());
		
		if (maxVal > bestMax)
		{
			bestMax = maxVal;
			match = t.first;
		}
	}

	//return the key ID to press
	return(stoi(match));
}

Mat GetWindowMat(RECT winRect, HDC hdcSource, HDC hdc)
{
	HBITMAP hBitmap; // <-- The image represented by hBitmap
	Mat matBitmap; // <-- The image represented by mat
	int x_size = winRect.right - winRect.left;
	int y_size = winRect.bottom - winRect.top;

	void *ptrBitmapPixels; // <-- Pointer variable that will contain the pointer for the pixels

	// Create hBitmap with Pointer to the pixels of the Bitmap
	BITMAPINFOHEADER bi;
	//ZeroMemory(&bi, sizeof(BITMAPINFO));
	bi.biSize = sizeof(BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = x_size;
	bi.biHeight = -y_size;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;

	hBitmap = CreateDIBSection(hdcSource, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &ptrBitmapPixels, NULL, 0);
	// ^^ The output: hBitmap & ptrBitmapPixels
	// Set hBitmap in the hdcMem 
	SelectObject(hdc, hBitmap);
	
	// Set matBitmap to point to the pixels of the hBitmap
	matBitmap = Mat(y_size, x_size, CV_8UC4, ptrBitmapPixels, 0);

	return(matBitmap);
}

void UpdateImage(HDC hdcSource, HDC hdc, RECT winRect)
{
	// Now update the pixels using BitBlt
	int x_size = winRect.right - winRect.left;
	int y_size = winRect.bottom - winRect.top;
	BitBlt(hdc, 0, 0, x_size, y_size, hdcSource, xpos, ypos, SRCCOPY);
}