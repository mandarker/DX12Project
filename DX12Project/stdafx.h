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

bool Running = true;

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

const int frameBufferCount = 3;

ID3D12Device* device;

// used to switch between render buffers
IDXGISwapChain3* swapChain;

ID3D12CommandQueue* commandQueue;

ID3D12DescriptorHeap* rtvDescriptorHeap;

ID3D12Resource* renderTargets[frameBufferCount];

// have enough allocators for buffer * threads
// currently only one thread, but should work on multithreading in the near future
ID3D12CommandAllocator* commandAllocator[frameBufferCount];

ID3D12GraphicsCommandList* commandList;

// should have as many as allocators (buffer * threads)
// locked until command list is executed
ID3D12Fence* fence[frameBufferCount];

HANDLE fenceEvent;

// incremented every frame
UINT64 fenceValue[frameBufferCount];

// current render target view
int frameIndex;

int rtvDescriptorSize;

bool InitD3D();

void Update();

void UpdatePipeline();

void Render();

void Cleanup();

void WaitForPreviousFrame();

#define SAFE_RELEASE(x) if( x != NULL ) { x->Release(); x = NULL; }