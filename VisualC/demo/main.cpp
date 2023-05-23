#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <schrift.h>

#include "util/arg.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static HWND sHwnd;
static COLORREF redColor = RGB(255, 0, 0);
static COLORREF blueColor = RGB(0, 0, 255);
static COLORREF greenColor = RGB(0, 255, 0);
SFT sft;

int width;
int height;

void SetWindowHandle(HWND hwnd)
{
	sHwnd = hwnd;
}

void SetPixel(int x, int y, COLORREF& color)
{
	if (sHwnd == NULL)
	{
		exit(0);
	}

	HDC hdc = GetDC(sHwnd);
	SetPixel(hdc, x, y, color);
	ReleaseDC(sHwnd, hdc);
	return;
}

void RenderText()
{
	unsigned long cp;
	SFT_Glyph gid, prevGid = 0;
	SFT_Image image;
	SFT_GMetrics mtx;
	SFT_Kerning kerning;

	// Text start position in pixels
	int xOffset = 10;
	int yOffet = 100;

	// TODO: use stored text to dispaly
	for (cp = 32; cp < 127; ++cp)
	{
		if (sft_lookup(&sft, cp, &gid) < 0)
			continue;
		if (sft_gmetrics(&sft, gid, &mtx) < 0)
			continue;

		image.width = mtx.minWidth;
		image.height = mtx.minHeight;
		image.pixels = malloc((size_t)image.width * (size_t)image.height);

		sft_render(&sft, gid, image);

		sft_kerning(&sft, prevGid, gid, &kerning);

		xOffset += (int)(mtx.leftSideBearing + kerning.xShift);

		for (int h = 0; h < image.height; h++)
		{
			for (int w = 0; w < image.width; w++)
			{
				char pixelWeight = 255 - ((char*)image.pixels)[image.width * h + w];
				COLORREF grayColor = RGB(pixelWeight, pixelWeight, pixelWeight);
				SetPixel(xOffset + w, yOffet + (int)mtx.yOffset + h, grayColor);
			}
		}

		xOffset += (int)mtx.advanceWidth;

		free(image.pixels);

		prevGid = gid;
	}
}

static void Die(const char* msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	// Load and setup font
	SFT_Font* font;
	const char* filename;
	double size;

	filename = "../../resources/FiraGO-Regular.ttf";
	size = 20.0;

	if (!(font = sft_loadfile(filename)))
		Die("Can't load font file.");

	memset(&sft, 0, sizeof sft);
	sft.font = font;
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y;

	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	WNDCLASS wc = { };

	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

	RegisterClass(&wc);

	// Create the window.
	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Font rendering program",		// Window text
		WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,   // Window style, To prevent maximing ~WS_MAXIMIZEBOX AND window resizing ~WS_THICKFRAME

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// TODO: use window size to render text inside window dimensions
	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	// Run the message loop.
	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	sft_freefont(font);

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		SetWindowHandle(hwnd);
		RenderText();

		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


