#include "stdafx.h"

bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, int width, int height, bool fullscreen) {
	if (fullscreen) {
		// monitor handler
		// gets handle to display monitor that has largest area of intersection with "window"
		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		// sets cbSize to struct size in order for GetMonitorInfo to work
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		width = mi.rcMonitor.right - mi.rcMonitor.left;
		height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	// different from WNDCLASS since it has size of structure as well as member for icon
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WindowName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Error Registering class", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		// Get Window Long
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop() {
	MSG msg;
	// fill size of dedicated memory with zeroes
	ZeroMemory(&msg, sizeof(MSG));

	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Exit?", L"Exit?", MB_YESNO | MB_ICONQUESTION) == IDYES)
				DestroyWindow(hwnd);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		// default window procedure
		// not exactly necessary unless window procedure is modified in any way
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
	if (!InitializeWindow(hInstance, nShowCmd, Width, Height, FullScreen)) {
		MessageBox(0, L"Window Initialization - Failed", L"Error", MB_OK);
		return 0;
	}

	mainloop();
	
	return 0;
}