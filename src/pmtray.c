/* * * * * * * *\
	PMTRAY.C -
		Adapted from various sources, made by Freedom
	DESCRIPTION -
		This file creates the notification area for Progman.
\* * * * * * * */

// Includes
#include "progman.h"
#include "resource.h"
#include "pmtray.h"

// Variables
BOOL bRet;
HWND hwndTray;

// Functions

// Tray Window Procedure
LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_DESTROY:
			// PostQuitMessage(0);
			ShowWindow(hwndTray, 0);
			UpdateWindow(hwndTray);
			break;

		case WM_CLOSE:
			// DestroyWindow(hwnd);
			ShowWindow(hwndTray, 0);
			UpdateWindow(hwndTray);
			break;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

// Tray Main Function
VOID TrayMain() {
	MSG			msgTray;
	WNDCLASSEX	wcTray;

	// Create the Main Window
	wcTray.cbSize = sizeof(WNDCLASSEX);
	wcTray.style = 0;
	wcTray.lpfnWndProc = TrayWndProc;
	wcTray.cbClsExtra = 0;
	wcTray.cbWndExtra = 0;
	wcTray.hInstance = hAppInstance;
	wcTray.hIcon = LoadIcon(hAppInstance, MAKEINTRESOURCE(PROGMANICON));
	wcTray.hIconSm = LoadIcon(hAppInstance, MAKEINTRESOURCE(PROGMANICON));
	wcTray.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcTray.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
	wcTray.lpszMenuName = NULL;
	wcTray.lpszClassName = TEXT("TrayNotifyWnd");

	if (!RegisterClassEx(&wcTray))
		return 0;

	hwndTray = CreateWindowEx(WS_EX_TOOLWINDOW, TEXT("TrayNotifyWnd"), NULL,
		WS_POPUP | WS_SYSMENU | WS_THICKFRAME | WS_CAPTION | WS_VISIBLE,
		60, 60, 100, 100, NULL, NULL, hAppInstance, NULL);

	if (hwndTray == NULL)
		return 0;

	ShowWindow(hwndTray, 1);

	SetWindowPos(hwndTray, HWND_NOTOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	UpdateWindow(hwndTray);

	// Messaging Loop
	while ((bRet = GetMessage(&msgTray, NULL, 0, 0)) != 0) {
		if (bRet == -1) {
			if (msgTray.message == WM_QUIT) {
				ExitProcess(0);
				return (int)msgTray.wParam;
			}
		}
		else {
			TranslateMessage(&msgTray);
			DispatchMessage(&msgTray);
		}
	}
	return msgTray.wParam;
}
