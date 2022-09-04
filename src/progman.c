/* * * * * * * *\
	PROGMAN.C -
		Copyright (c) 1991,  Microsoft Corporation
	DESCRIPTION -
		This file is for support of program manager under NT Windows.
		This file is/was ported from progman.c (program manager).
	MODIFICATION HISTORY -
		Initial Version: x/x/90		Author Unknown, since he didn't feel like commenting the code...
		NT 32b Version: 1/16/91		Jeff Pack, Intitial port to begin.
		NT x86_64 Ver: 3/9/22		freedom, cleaned up and 64-bitted
\* * * * * * * */

// Includes
#include "progman.h"
#include "resource.h"
#include "pmanfunc.h"

// Variables
BOOL	UserIsAdmin				= FALSE;
BOOL	AccessToCommonGroups	= FALSE;
BOOL	bLoadIt					= FALSE;
BOOL	bMinOnRun				= FALSE;
BOOL	bArranging				= FALSE;
BOOL	bAutoArrange			= FALSE;
BOOL	bAutoArranging			= FALSE;
BOOL	bExitWindows			= FALSE;
BOOL	bScrolling				= FALSE;
BOOL	bSaveSettings			= TRUE;
BOOL	bLoadEvil				= FALSE;
BOOL	bMove					= FALSE;
BOOL	bInDDE					= FALSE;
BOOL	bIconTitleWrap			= TRUE;
BOOL	fInExec					= FALSE;
BOOL	fNoRun					= FALSE;
BOOL	fNoClose				= FALSE;
BOOL	fNoSave					= FALSE;
BOOL	fNoFileMenu				= FALSE;
BOOL	fExiting				= FALSE;
BOOL	fLowMemErrYet			= FALSE;
BOOL	fErrorOnExtract			= FALSE;
BOOL	bFrameSysMenu			= FALSE;

WCHAR	szNULL[]				= TEXT("");
WCHAR	szProgman[]				= TEXT("progman");

// Program Manager's Settings keys
WCHAR	szWindow[]				= TEXT("Window");
WCHAR	szOrder[]				= TEXT("UNICODE Order");
WCHAR	szAnsiOrder[]			= TEXT("Order");
WCHAR	szStartup[]				= TEXT("startup");
WCHAR	szAutoArrange[]			= TEXT("AutoArrange");
WCHAR	szSaveSettings[]		= TEXT("SaveSettings");
WCHAR	szMinOnRun[]			= TEXT("MinOnRun");
WCHAR	szFocusOnCommonGroup[]	= TEXT("FocusOnCommonGroup");

WCHAR	szProgmanHelp[]			= TEXT("PROGMAN.HLP");
WCHAR	szTitle[MAXTITLELEN + 1];
WCHAR	szMessage[MAXMESSAGELEN+1];
WCHAR	szNameField[MAX_PATH];
WCHAR	szPathField[MAX_PATH];
WCHAR	szDirField[MAX_PATH];
WCHAR	szIconPath[MAX_PATH];
WCHAR	szOriginalDirectory[MAX_PATH];
WCHAR	szWindowsDirectory[MAX_PATH];

WCHAR	szOOMExitMsg[64];
WCHAR	szOOMExitTitle[32];

/* for Program Groups in Registry */
HKEY	hkeyProgramManager	= NULL;  // progman.ini key
HKEY	hkeyPMSettings		= NULL;  // keys corresponding to progman.ini sections
HKEY	hkeyPMRestrict		= NULL;
HKEY	hkeyPMGroups		= NULL;
HKEY	hkeyPMCommonGroups	= NULL;

WCHAR	szAnsiProgramGroups[]	= TEXT("Program Groups");   // registry key for groups
HKEY	hkeyProgramGroups		= NULL;
HKEY	hkeyAnsiProgramGroups	= NULL;
HKEY	hkeyCommonGroups		= NULL;
PSECURITY_ATTRIBUTES pSecurityAttributes = NULL;
PSECURITY_ATTRIBUTES pAdminSecAttr = NULL;

HANDLE		hAccel;
HINSTANCE	hAppInstance;
HANDLE		hCommdlg	= NULL;

HICON	hDlgIcon		= NULL;
HICON	hItemIcon		= NULL;
HICON	hGroupIcon		= NULL;
HICON	hCommonGrpIcon	= NULL;
HICON	hProgmanIcon	= NULL;
HICON	hIconGlobal		= NULL;

HFONT	hFontTitle		= NULL;

HWND	hwndProgman		= NULL;
HWND	hwndMDIClient	= NULL;

HBRUSH	hbrWorkspace	= NULL;

WORD	wPendingHotKey	= 0;
DWORD	dwDDEAppId		= 0;
//HANDLE hPendingWindow      = 0;
DWORD	dwEditLevel		= 0;
WORD	wLockError		= 0;
UINT	uiActivateShellWindowMessage	= 0;
UINT	uiConsoleWindowMessage			= 0;
UINT	uiSaveSettingsMessage			= 0;   // for upedit.exe: User Profile Editor

int	nGroups = 0;
int	dyBorder;
int	iDlgIconId;
int	iDlgIconIndex;
int	cxIconSpace;
int	cyIconSpace;
int	cxOffset;
int	cyOffset;
int	cxArrange;
int	cyArrange;
int	cxIcon;
int	cyIcon;

PGROUP	pFirstGroup		= NULL;
PGROUP	pCurrentGroup	= NULL;
PGROUP	pActiveGroup	= NULL;
PGROUP	*pLastGroup		= &pFirstGroup;
PGROUP	pExecingGroup	= NULL;

PITEM	pExecingItem	= NULL;

RECT	rcDrag		= { 0,0,0,0 };
HWND	hwndDrag	= 0;

WORD	wNewSelection;

UINT	uiHelpMessage;		// stuff for help
UINT	uiBrowseMessage;	// stuff for help
WORD	wMenuID			= 0;
HANDLE	hSaveMenuHandle	= 0L;				/*Save hMenu into one variable*/
WORD	wSaveFlags		= 0;				/*Save flags into another*/
HANDLE	hSaveMenuHandleAroundSendMessage;	/*Save hMenu into one variable*/
WORD	wSaveFlagsAroundSendMessage;		/*Save flags into another*/
WORD	wSaveMenuIDAroundSendMessage;
DWORD	dwContext		= 0L;
HHOOK	hhkMsgFilter	= NULL;
BOOL	bUseANSIGroups	= FALSE;

extern BOOL bInNtSetup;
extern VOID TMMain(void);
HANDLE hTMThread = NULL;

BOOL FAR PASCAL CheckHotKey(WPARAM wParam, LPARAM lParam);

/*** main --         Program entry point (was WinMain).
 *
 *
 *
 * int APIENTRY main(int argc, char *argv[], char *envp[])
 *
 * ENTRY -         int argc                - argument count.
 *                        char *argv[]        - argument list.
 *                        char *envp[]        - environment.
 *
 * EXIT  -           TRUE if success, FALSE if not.
 * SYNOPSIS -
 * WARNINGS -
 * EFFECTS  -
 *
 */

int __cdecl wmain(
	int argc,
	wchar_t *argv[],
	wchar_t *envp[])
{
	MSG msg;
	HANDLE hInst;
	LPTSTR  lpszCmdLine = NULL;
	int    nCmdShow = SW_SHOWNORMAL;
	DWORD dwThreadID;
	DWORD dwEvent;

	hInst = GetModuleHandle(NULL);

	if (argc > 1) {
		//
		// Get the command line, sans program name.
		//
		lpszCmdLine = SkipProgramName(GetCommandLine());

	}

	/*
	 * Initialize the window classes and other junk.
	 */
	if (!AppInit(hInst, lpszCmdLine, nCmdShow)) {
		return FALSE;
	}

	//
	// Don't start the taskman thread if progman is started from NTSETUP.
	//

	if (!bInNtSetup) {
		HKEY hkeyWinlogon;
		DWORD dwType;
		WCHAR szBuffer[MAX_PATH];
		DWORD cbBuffer;
		BOOL  bUseDefaultTaskman = TRUE;

		//
		// Check if a replacement taskman exits.  First open the Taskman
		// entry in winlogon's settings.
		//

		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					 TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"),
					 0,
					 KEY_READ,
					 &hkeyWinlogon) == ERROR_SUCCESS) {

			//
			// Query the taskman name.
			//

			cbBuffer = sizeof(szBuffer);
			if (RegQueryValueEx(hkeyWinlogon,
								TEXT("Taskman"),
								0,
								&dwType,
								(LPBYTE)szBuffer,
								&cbBuffer) == ERROR_SUCCESS) {

				//
				// The taskman entry exits.  Confirm that it is not NULL.
				//

				if (szBuffer[0] != TEXT('\0')) {

					//
					// Try to spawn the taskman replacement.  If
					// the spawning succeeds, ExecProgram will return 0.
					//

					if (ExecProgram (szBuffer, NULL, NULL, FALSE,
									 0, 0, FALSE) == 0) {
						bUseDefaultTaskman = FALSE;
					}
				}
			}

			//
			// Close the registry key.
			//

			RegCloseKey (hkeyWinlogon);
		}

		//
		// Check to see if we should spawn the default taskman.
		//

		if (bUseDefaultTaskman) {
			hTMThread = CreateThread(NULL, (DWORD)0,
									(LPTHREAD_START_ROUTINE)TMMain,
									(LPVOID)NULL, 0, &dwThreadID);
		}
	}

	//
	// Messaging Loop.
	//

	while (TRUE) {

		while (PeekMessage(&msg, (HWND)NULL, 0, 0, PM_REMOVE)) {

			if (msg.message == WM_QUIT) {
			   ExitProcess(0);
			   return (int)msg.wParam;
			}

			/*
			 * First test if this is a hot key.
			 *
			 */

			if (msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN) {
				if (CheckHotKey(msg.wParam, msg.lParam))
					continue;
			}

			/*
			 * Since we use RETURN as an accelerator we have to manually
			 * restore ourselves when we see VK_RETURN and we are minimized.
			 */
			if (hwndProgman != NULL && msg.message == WM_SYSKEYDOWN && msg.wParam == VK_RETURN &&
					IsIconic(hwndProgman)) {
				ShowWindow(hwndProgman, SW_NORMAL);

			} else {
				if ((hwndMDIClient == NULL ||
						!TranslateMDISysAccel(hwndMDIClient, &msg)) &&
						(hwndProgman == NULL ||
						!TranslateAccelerator(hwndProgman, hAccel, &msg))) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}

		dwEvent = MsgWaitForMultipleObjects(2, gahEvents, FALSE,
			INFINITE, QS_ALLINPUT);

		if (dwEvent < (WAIT_OBJECT_0 + 2)) {
		   HandleGroupKeyChange((dwEvent == (WAIT_OBJECT_0 + 1)));
		}
	}

	return msg.wParam;
}

/*** MyMessageBox --
 *
 *
 * int APIENTRY MyMessageBox(HWND hWnd, WORD idTitle, WORD idMessage, LPSTR lpsz, WORD wStyle)
 *
 * ENTRY -         HWND        hWnd
 *                        WORD        idTitle
 *                        WORD        idMessage
 *                        LPSTR        lpsz
 *                        WORD        wStyle
 *
 * EXIT  -        int         xx                        - Looks like -1 is error, otherwise result.
 *
 * SYNOPSIS -  ???
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

int APIENTRY MyMessageBox(HWND hWnd, WORD idTitle, WORD idMessage, LPTSTR psz, WORD wStyle)
{
	WCHAR szTempField[MAXMESSAGELEN];
	int iMsgResult = NULL;

	if (bInDDE){
		return(1);
	}
	if (!LoadString(hAppInstance, idTitle, szTitle, CharSizeOf(szTitle))){
		goto MessageBoxOOM;
	}
	if (idMessage < 32){
		if (!LoadString(hAppInstance, IDS_UNKNOWNMSG, szTempField, CharSizeOf(szTempField))){
			goto MessageBoxOOM;
		}
		wsprintf(szMessage, szTempField, idMessage);
	}
	else{
		if (!LoadString(hAppInstance, idMessage, szTempField, CharSizeOf(szTempField)))
			goto MessageBoxOOM;

		if (psz)
			wsprintf(szMessage, szTempField, (LPTSTR)psz);
		else
			lstrcpy(szMessage, szTempField);
	}

	if (hWnd){
		hWnd = GetLastActivePopup(hWnd);
	}

	iMsgResult = MessageBox(hWnd, szMessage, szTitle, wStyle );

	if (iMsgResult == -1){

MessageBoxOOM:
		MessageBox(GetLastActivePopup(hwndProgman), szOOMExitMsg, szOOMExitTitle, MB_SYSTEMMODAL | MB_ICONHAND | MB_OK);
	}

	return(iMsgResult);
}


/*** MessageFilter --
 *
 *
 * int APIENTRY MessageFilter(int nCode, WPARAM wParam, LPMSG lpMsg)
 *
 * ENTRY -         int                nCode
 *                        WPARAM        wParam
 *                        WORD        idMessage
 *                        LPMSG        lpMsg
 *
 * EXIT  -        int         xx                        - Looks like 0 is error, otherwise 1 is success
 *
 * SYNOPSIS -  ???
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

LRESULT APIENTRY MessageFilter(int nCode, WPARAM wParam, LPARAM lParam)

{
	LPMSG lpMsg = (LPMSG)lParam;
	if (nCode < 0){
		goto DefHook;
	}
	if (nCode == MSGF_MENU) {

		if (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_F1) {
			/* Window of menu we want help for is in loword of lParam.*/
			PostMessage(hwndProgman, uiHelpMessage, MSGF_MENU, (LPARAM)lpMsg->hwnd);
			return(1);
		}

	} else if (nCode == MSGF_DIALOGBOX) {

		if (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_F1) {
			/* Dialog box we want help for is in loword of lParam */
			PostMessage(hwndProgman, uiHelpMessage, MSGF_DIALOGBOX, (LPARAM)lpMsg->hwnd);
			return(1);
		}

	}
	else {
DefHook:
		return DefHookProc(nCode, wParam, lParam, &hhkMsgFilter);
	}
	return(0);
}
