#pragma once

// this file groups all headers and code for dx12
// not optimal because it's possible for the code to be more organized and modular

// removes larger unused portions of windows API (Cryptography, dde, ...)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <d3d12.h>
// directx graphics infrastructure (dxgi)
// communicated with kernel mode driver and system hardware, futureproofed
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
// this file has helper functions
#include "d3dx12.h"



// window handle
HWND hwnd = NULL;

int Width = 800;
int Height = 600;
bool FullScreen = false;

// app name
// Long Pointer to a Const TCHAR STRing
LPCTSTR WindowName = L"WindowApp";

// app title in windowed mode
LPCTSTR WindowTitle = L"Window";

// handle instance (hinstance)
bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, int width, int height, bool fullscreen);

void mainloop();

// sends message (event)
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);