// Need this so progman.c can include it in
// a meaningful way.

#ifndef RC_INVOKED
// #include <nt.h>
// #include <ntrtl.h>
// #include <nturtl.h>
#endif

#include "progman.h"
// #include <winuserp.h>

// WINUSERP.H
BOOL
WINAPI
RegisterTasklist(
    IN HWND hWndTasklist);

// ntddk
typedef enum _PROCESSINFOCLASS {
	ProcessBasicInformation,
	ProcessQuotaLimits,
	ProcessIoCounters,
	ProcessVmCounters, // VM_COUNTERS_EX
	ProcessTimes,
	ProcessBasePriority,
	ProcessRaisePriority,
	ProcessDebugPort,
	ProcessExceptionPort,
	ProcessAccessToken,
	ProcessLdtInformation,
	ProcessLdtSize,
	ProcessDefaultHardErrorMode,
	ProcessIoPortHandlers,          // Note: this is kernel mode only
	ProcessPooledUsageAndLimits,
	ProcessWorkingSetWatch,
	ProcessUserModeIOPL,
	ProcessEnableAlignmentFaultFixup,
	ProcessPriorityClass,
	ProcessWx86Information,
	ProcessHandleCount,
	ProcessAffinityMask,
	ProcessPriorityBoost,
	ProcessDeviceMap,
	ProcessSessionInformation,
	ProcessForegroundInformation,
	ProcessWow64Information,
	ProcessImageFileName,
	ProcessLUIDDeviceMapsEnabled,
	ProcessBreakOnTermination,
	ProcessDebugObjectHandle,
	ProcessDebugFlags,
	ProcessHandleTracing,
	MaxProcessInfoClass                             // MaxProcessInfoClass should always be the last enum
} PROCESSINFOCLASS;

// Taskman prototyes

INT_PTR APIENTRY TaskmanDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam);

void APIENTRY GetPathInfo(LPTSTR szPath, LPTSTR*pszFileName, LPTSTR*pszExt, WORD *pich, BOOL *pfUnc);
BOOL TestTokenForAdmin(HANDLE Token);

INT TMMessageBox(HWND hWnd,WORD idTitle,WORD idMessage,PTSTR psz,WORD wStyle);

// Taskman global variables

HWND ghwndTMDialog;
BOOL fTMActive;
INT dxTaskman;
INT dyTaskman;
INT dxScreen;
INT dyScreen;

#define MAXTASKNAMELEN      512
#define MAXMSGBOXLEN        513

#define PWRTASKMANDLG       10
#define WMPTASKMANDLG       11
