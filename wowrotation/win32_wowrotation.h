#pragma once
#include <vector>
#include <string>
#include "img_proc.h"

struct win32_state
{
	bool Drawing = false;
	bool WritingDraw = false;
	bool Quitting = false;
	bool Sizing = false;

	bool RunningRotation = false;
	int PausedRotation = 0;
};

struct win32_handles
{
	HWND MainWindow;
	HWND RectSelector;
	HWND RectShowButton;
};

win32_handles GetWindowHandles();
bool Quitting();
void* ReadEntireFile(std::string FileName);
void DrawBitmap(image_bitmap Image, int X, int Y, HWND Handle, bool FreeAfter = false);
std::vector<std::string> GetAllFileNamesInDirectory(std::string Folder);
void* AllocHeap(DWORD Flags, SIZE_T Bytes);
void DeallocHeap(LPVOID Buffer);