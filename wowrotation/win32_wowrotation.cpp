#include "win32_wowrotation.h"
#include "wowrotation.h"
#include <thread>
#include <windowsx.h>
#include <Windows.h>

struct draw_item
{
	image_bitmap Image;
	int X;
	int Y;
	int DestWidth;
	int DestHeight;
	HWND Handle;
	bool Free;
};
std::vector<draw_item> DrawQueue;

struct fill_type
{
	std::string FillType;
	RECT Region;
	HBRUSH Color;
};

struct window_backgrounds
{
	HWND hwnd;
	std::vector<fill_type> Regions;
};
std::vector<window_backgrounds> WinBackgrounds;

win32_state Win32State = {};
win32_handles Win32Handles = {};
HANDLE ProcessHeap;

win32_handles
GetWindowHandles()
{
	return(Win32Handles);
}

void*
ReadEntireFile(std::string FileName)
{
	void* Result;

	HANDLE FileHandle = CreateFileA(FileName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			unsigned long int FileSize32 = (unsigned long int)FileSize.QuadPart;
			Result = (void*)AllocHeap(LPTR, FileSize32);
			if (Result)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					// read successful
				}
				else
				{
					// file read failed
					DeallocHeap(Result);
					Result = 0;
				}
			}
		}
	}

	return(Result);
}

std::vector<std::string>
GetAllFileNamesInDirectory(std::string Folder)
{
	std::vector<std::string> Names;
	WIN32_FIND_DATAA FindData;
	HANDLE FileHandle = FindFirstFileA(Folder.c_str(), &FindData);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				Names.push_back(FindData.cFileName);
			}
		} while (FindNextFileA(FileHandle, &FindData));
		FindClose(FileHandle);
	}
	return(Names);
}

LRESULT
GetResizeResult(HWND hWnd, LRESULT Result, LPARAM lParam)
{
	int BORDERWIDTH = 10;
	int TITLEBARWIDTH = 0;
	RECT WindowRect;
	int x, y;
	GetWindowRect(hWnd, &WindowRect);
	x = GET_X_LPARAM(lParam) - WindowRect.left;
	y = GET_Y_LPARAM(lParam) - WindowRect.top;

	if (x >= BORDERWIDTH && x <= WindowRect.right - WindowRect.left - BORDERWIDTH && y >= BORDERWIDTH && y <= TITLEBARWIDTH)
		return HTCAPTION;
	else if (x < BORDERWIDTH && y < BORDERWIDTH)
		return HTTOPLEFT;
	else if (x > WindowRect.right - WindowRect.left - BORDERWIDTH && y < BORDERWIDTH)
		return HTTOPRIGHT;
	else if (x > WindowRect.right - WindowRect.left - BORDERWIDTH && y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
		return HTBOTTOMRIGHT;
	else if (x < BORDERWIDTH && y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
		return HTBOTTOMLEFT;
	else if (x < BORDERWIDTH)
		return HTLEFT;
	else if (y < BORDERWIDTH)
		return HTTOP;
	else if (x > WindowRect.right - WindowRect.left - BORDERWIDTH)
		return HTRIGHT;
	else if (y > WindowRect.bottom - WindowRect.top - BORDERWIDTH)
		return HTBOTTOM;
	else
		return Result;
}

void
BringToTop(HWND hwnd)
{
	RECT WinRect;
	GetWindowRect(hwnd, &WinRect);
	int Width = WinRect.right - WinRect.left;
	int Height = WinRect.bottom - WinRect.top;

	SetWindowPos(hwnd, HWND_TOPMOST, WinRect.left, WinRect.top, Width, Height, SWP_SHOWWINDOW);
}

void
HandlePaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	if (!Win32State.WritingDraw && !DrawQueue.empty())
	{
		Win32State.Drawing = true;
		std::vector<draw_item> NewQueue;
		for (auto Item : DrawQueue)
		{
			if (hwnd == Item.Handle)
			{
				//TODO: maybe let DrawBitmap decide what kind of drawing call to use
				//NOTE: need to make height negative so it renders in correct orientation
				Item.Image.Header.Height = -Item.Image.Header.Height;
				StretchDIBits(hdc, Item.X, Item.Y, Item.DestWidth, Item.DestHeight, 0, 0, Item.Image.Header.Width,
					-Item.Image.Header.Height, Item.Image.BitmapPixels, (BITMAPINFO*)&Item.Image.Header, DIB_RGB_COLORS, SRCCOPY);
				Item.Image.Header.Height = -Item.Image.Header.Height;
				if (Item.Free)
					DeallocHeap((void*)Item.Image.BitmapPixels);
			}
			else
			{
				NewQueue.push_back(Item);
			}
		}
		DrawQueue = NewQueue;
	}
	else if (DrawQueue.empty())
	{
		//NOTE: lets get something to draw next time
		Win32State.WritingDraw = true;
	}

	if (hwnd == Win32Handles.RectSelector)
	{
		RECT WinRect;
		GetWindowRect(hwnd, &WinRect);

		std::string OutText = std::to_string(WinRect.right - WinRect.left) + ", " + std::to_string(WinRect.bottom - WinRect.top);
		RECT r = { 0, 0, 0, 0 };
		DrawTextA(hdc, (LPCSTR)OutText.c_str(), (int)OutText.length(), &r, DT_CALCRECT);

		int x = (WinRect.right - WinRect.left) / 2 - (r.right - r.left) / 2;
		int y = (WinRect.bottom - WinRect.top) / 2 - (r.bottom - r.top) / 2;
		SetBkMode(hdc, TRANSPARENT);
		TextOutA(hdc, x, y, (LPCSTR)OutText.c_str(), (int)OutText.length());
	}
	EndPaint(hwnd, &ps);
	
	Win32State.Drawing = false;
}

void
HandleButtonClicked(HWND ButtonHwnd)
{
	if (ButtonHwnd == Win32Handles.RectShowButton)
	{
		ShowWindow(Win32Handles.RectSelector, SW_SHOW);
	}
}

void
HandleEraseBackground(HWND hwnd, WPARAM wParam)
{
	RECT WinRect;
	for (auto WinBg : WinBackgrounds)
	{
		if (hwnd == WinBg.hwnd)
		{
			HDC hdc = (HDC)wParam;
			for (auto r : WinBg.Regions)
			{
				if (r.Region.bottom == -1)
					GetClientRect(hwnd, &WinRect);
				else
					WinRect = r.Region;

				if (r.FillType == "FillRect")
					FillRect(hdc, &WinRect, r.Color);
				else if (r.FillType == "FrameRect")
					FrameRect(hdc, &WinRect, r.Color);
			}
		}
	}
}

LRESULT CALLBACK 
MainWindowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT Result = 1;

	switch (uMsg)
	{
		case WM_SIZE:
		{
			if (hwnd == Win32Handles.RectSelector)
				Win32State.Sizing = true;
			InvalidateRect(Win32Handles.MainWindow, NULL, 1);
			Sleep(1);
		} break;
		case WM_WINDOWPOSCHANGING:
		{
			Sleep(1);
			if (hwnd == Win32Handles.RectSelector && IsWindowVisible(hwnd))
				Win32State.Sizing = true;
		} break;
		
		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;

		case WM_CLOSE:
		{
			Win32State.Quitting = true;
			DestroyWindow(hwnd);
		} break;

		case WM_ACTIVATEAPP:
		{
			if (IsWindowVisible(Win32Handles.RectSelector))
				BringToTop(Win32Handles.RectSelector);
		} break;

		case WM_PAINT:
		{
			HandlePaint(hwnd);
		} break;
		case WM_NCHITTEST:
		{
			if (hwnd == Win32Handles.RectSelector)
			{
				Result = DefWindowProc(hwnd, uMsg, wParam, lParam);
				if (Result == HTCLIENT)
				{
					Result = HTCAPTION;
					Result = GetResizeResult(hwnd, Result, lParam);
				}
			}
			Sleep(1);			
		}break;
		case WM_GETMINMAXINFO:
		{
			if (hwnd == Win32Handles.RectSelector)
			{
				MINMAXINFO* mmi = (MINMAXINFO*)lParam;
				mmi->ptMinTrackSize.x = 30;
				mmi->ptMinTrackSize.y = 30;
			}
			else
				Result = DefWindowProc(hwnd, uMsg, wParam, lParam);
			Sleep(1);
		}break;
		case WM_ERASEBKGND:
		{
			HandleEraseBackground(hwnd, wParam);
			Sleep(1);
		}break;
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				HandleButtonClicked((HWND)lParam);
			}
			Sleep(1);
		}break;
		default:
		{
			Result = DefWindowProc(hwnd, uMsg, wParam, lParam);
			Sleep(1);
		} break;
	}
	return(Result);
}

void
SaveToRegistry(LPCWSTR ValueName, DWORD DataType, BYTE* Data, DWORD DataBytes)
{
	HKEY RegHandle;
	LPCWSTR SubLocation = L"SOFTWARE\\wowrotation";
	long CreateResult = RegCreateKeyEx(HKEY_CURRENT_USER, SubLocation, 0, 0, 0, KEY_WRITE, 0, &RegHandle, 0);
	if (CreateResult == ERROR_SUCCESS)
	{
		long SetResult = RegSetValueEx(RegHandle, ValueName, 0, DataType, Data, DataBytes);
		RegCloseKey(RegHandle);
	}
}

BYTE*
ReadFromRegistry(LPCWSTR ValueName)
{
	DWORD DataBytes = 0;
	BYTE* Data = nullptr;
	HKEY RegHandle;
	LPCWSTR SubLocation = L"SOFTWARE\\wowrotation";
	if (RegCreateKeyEx(HKEY_CURRENT_USER, SubLocation, 0, 0, 0, KEY_ALL_ACCESS, 0, &RegHandle, 0) == ERROR_SUCCESS)
	{
		RegQueryValueEx(RegHandle, ValueName, 0, 0, 0, &DataBytes);
		Data = new BYTE[DataBytes];
		long ReadResult = RegQueryValueEx(RegHandle, ValueName, 0, 0, Data, &DataBytes);
		RegCloseKey(RegHandle);
	}
	return(Data);
}

void
SaveWindowPlacement(HWND hwnd)
{
	WINDOWPLACEMENT wp = { 0 };
	wp.length = sizeof(wp);
	if (GetWindowPlacement(hwnd, &wp))
	{
		SaveToRegistry(L"RegionPlacement", REG_BINARY, (BYTE*)&wp, wp.length);
	}
	int breakhere = 3;
}

void
HandleMouse()
{
	POINT CursorPos;
	GetCursorPos(&CursorPos);
	HWND hwnd = WindowFromPoint(CursorPos);

	if(GetAsyncKeyState(VK_RBUTTON) & 0x8000)
	{
		if (hwnd == Win32Handles.RectSelector)
		{
			SaveWindowPlacement(hwnd);
			ShowWindow(hwnd, SW_HIDE);
			FreeImageCache();
		}
	}
	
}

void
HandleKeyboard(win32_state* WinState, bool* KeyPressed)
{
	//(TODO): Make this modifiable
	int CtrlKey = GetAsyncKeyState(VK_CONTROL) & 0x8000;
	int TKey = GetAsyncKeyState(0x54) & 0x8000;
	int ShiftKey = GetAsyncKeyState(VK_SHIFT) & 0x8000;
	if (CtrlKey && TKey && !KeyPressed[0x54])
	{
		KeyPressed[0x54] = true;
		WinState->RunningRotation = !WinState->RunningRotation;
	}
	else if (!TKey && KeyPressed[0x54])
	{
		KeyPressed[0x54] = false;
	}
	WinState->PausedRotation = ShiftKey;
}

void
HandleInput(win32_state* WinState)
{
	bool KeyPressed[254] = {};
	while (!WinState->Quitting)
	{
		HandleMouse();
		HandleKeyboard(WinState, KeyPressed);
		Sleep(1);
	}
	OutputDebugStringA("Quiting input thread");
}

void
DrawBitmap(image_bitmap Image, int X, int Y, HWND Handle, bool FreeAfter)
{
	Win32State.WritingDraw = true;
	const int MaxDim = 150;
	draw_item DrawItem = {};

	int BiggerDim = Image.Header.Width >= Image.Header.Height ? Image.Header.Width : Image.Header.Height;
	double Scale = (double)MaxDim / BiggerDim;
	if (Image.Header.Width >= Image.Header.Height)
	{
		DrawItem.DestWidth = MaxDim;
		DrawItem.DestHeight = (int)(Image.Header.Height * Scale);
	}
	else
	{
		DrawItem.DestHeight = MaxDim;
		DrawItem.DestWidth = (int)(Image.Header.Width * Scale);
	}
	DrawItem.Image = Image;
	DrawItem.X = X;
	DrawItem.Y = Y;
	DrawItem.Handle = Handle;
	DrawItem.Free = FreeAfter;
	if (!Win32State.Drawing)
	{
		DrawQueue.push_back(DrawItem);	
	}
	InvalidateRect(Handle, NULL, NULL);
	Win32State.WritingDraw = false;
}

void*
AllocHeap(DWORD Flags, SIZE_T Bytes)
{
	void* Mem = HeapAlloc(ProcessHeap, Flags, Bytes);
	return(Mem);
}

void
DeallocHeap(LPVOID Buffer)
{
	HeapFree(ProcessHeap, 0, Buffer);
}

void
InitOtherWindows(HINSTANCE hInstance, WNDCLASSEX WindowClass)
{
	Win32Handles.RectSelector = CreateWindowEx(WS_EX_LAYERED, WindowClass.lpszClassName, (LPCWSTR)L"RectSel", WS_POPUP | WS_BORDER,
		CW_USEDEFAULT, CW_USEDEFAULT, 260, 260, 0, 0, hInstance, 0);
	if (Win32Handles.RectSelector)
	{
		SetLayeredWindowAttributes(Win32Handles.RectSelector, 0, 150, LWA_ALPHA);
		UpdateWindow(Win32Handles.RectSelector);
		WINDOWPLACEMENT wp = *(WINDOWPLACEMENT*)ReadFromRegistry(L"RegionPlacement");
		SetWindowPlacement(Win32Handles.RectSelector, &wp);
		ShowWindow(Win32Handles.RectSelector, SW_HIDE);
	}

	HFONT ButtonFont = CreateFont(13, 0, 0, 0, FW_DONTCARE, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));
	Win32Handles.RectShowButton = CreateWindow(L"BUTTON", L"Select Region", WS_VISIBLE | WS_CHILD | BS_FLAT,
		40, 200, 105, 30, Win32Handles.MainWindow, 0, hInstance, 0);
	if (Win32Handles.RectShowButton)
	{
		SendMessage(Win32Handles.RectShowButton, WM_SETFONT, (WPARAM)ButtonFont, 1);
	}
}

void
InitBackgroundRegions()
{
	window_backgrounds RectSelector = {};
	RECT SelRect = { -1, -1, -1, -1 };
	RectSelector.hwnd = Win32Handles.RectSelector;
	RectSelector.Regions.push_back(fill_type{ "FillRect", SelRect, CreateSolidBrush(RGB(230, 100, 100))});
	WinBackgrounds.push_back(RectSelector);

	window_backgrounds MainWinBackground = {};
	RECT MainRect = { -1, -1, -1, -1 };
	MainWinBackground.hwnd = Win32Handles.MainWindow;
	MainWinBackground.Regions.push_back(fill_type{ "FillRect", MainRect, CreateSolidBrush(RGB(30, 30, 30))});
	RECT SelBoundBox = RECT{ 10, 10, 170, 170 };
	MainWinBackground.Regions.push_back(fill_type{ "FillRect", SelBoundBox , CreateSolidBrush(RGB(25, 25, 25))});
	MainWinBackground.Regions.push_back(fill_type{ "FrameRect", SelBoundBox, CreateSolidBrush(RGB(0, 0, 0))});
	RECT TemplateBoundBox = RECT{ 180, 10, 340, 170 };
	MainWinBackground.Regions.push_back(fill_type{ "FillRect", TemplateBoundBox, CreateSolidBrush(RGB(25, 25, 25)) });
	MainWinBackground.Regions.push_back(fill_type{ "FrameRect", TemplateBoundBox, CreateSolidBrush(RGB(0, 0, 0)) });
	WinBackgrounds.push_back(MainWinBackground);
}

int CALLBACK 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX WindowClass = {0};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWindowCallback;
	WindowClass.hInstance = hInstance;
	//WindowClass.hIcon;
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	WindowClass.lpszClassName = L"WoWRotationWindowClass";

	if (RegisterClassEx(&WindowClass))
	{
		Win32Handles.MainWindow = CreateWindowEx(0, WindowClass.lpszClassName, (LPCWSTR)L"WoWRotation", WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, 365, 300, 0, 0, hInstance, 0);

		if (Win32Handles.MainWindow)
		{	
			InitOtherWindows(hInstance, WindowClass);
			InitBackgroundRegions();
			InvalidateRect(Win32Handles.MainWindow, 0, 1);
			ProcessHeap = GetProcessHeap();
			BOOL MessageResult;
			MSG Message;

			std::thread RotationThread(MainRotationLoop, &Win32State);
			RotationThread.detach();
			std::thread InputThread(HandleInput, &Win32State);
			InputThread.detach();

			while ((MessageResult = GetMessage(&Message, 0, 0, 0)) != 0)
			{
				if (MessageResult == -1)
				{
					//TODO: Handle error?
				}
				else
				{
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}
			}
		}
	}

	return(0);
}