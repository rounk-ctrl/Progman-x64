/*
 * pmdlgs.c - program manager dialog procedures
 *
 *  Copyright (c) 1991,  Microsoft Corporation
 *
 *  DESCRIPTION
 *
 *                This file is for support of program manager under NT Windows.
 *                This file is/was ported from pmdlgs.c (program manager).
 *
 *  MODIFICATION HISTORY
 *      Initial Version: x/x/90        Author Unknown, since he didn't feel
 *                                                                like lettings us know...
 *
 *      NT 32b Version:        2/1/91        Jeff Pack
 *                                                                Intitial port to begin.
 *
 *
 */

// Includes
#include <Shlwapi.h>
#include "progman.h"
#include "pmanfunc.h"
#include "resource.h"

// GET_WM_* Definitions, pwin32.h
#define GET_WM_COMMAND_ID(wp, lp)               LOWORD(wp)
#define GET_WM_COMMAND_CMD(wp, lp)              HIWORD(wp)

#define CBCUSTFILTER 40         // Max size of custom filter.

BOOL fFirstPass;                // For IconDlgProc so we can fill the list
								// with stuff from progman if the first
								// file given has no icons.
BOOL fNewIcon;                  // Flags if the icon has been explicitely
								// set.

extern BOOL bHandleProgramGroupsEvent;

WCHAR szDotPIF[] = TEXT(".pif"); // PIF STUFF PLEASE FIX LNK SHORTCUT ETC ETC
WCHAR szCommdlg[] = TEXT("comdlg32.dll");       // The name of the common dialog.
typedef BOOL (APIENTRY *OPENFILENAME_PROC)(LPOPENFILENAME);      // Commdlgs GetOpenFileName routine.
OPENFILENAME_PROC lpfnGOFN;

/* from pmgseg.c */
DWORD PASCAL SizeofGroup(LPGROUPDEF lpgd);
void RemoveBackslashFromKeyName(LPTSTR lpKeyName);

VOID CentreWindow(HWND hwnd);
VOID FreeIconList(HANDLE hIconList, int iKeepIcon);

#define CHECK_BINARY_EVENT_NAME TEXT("CheckBinaryEvent")
#define CHECK_BINARY_ID           1
BOOL    bCheckBinaryType;
HANDLE  hCheckBinaryEvent;
UINT    uiCheckBinaryTimeout;
BOOL    bCheckBinaryDirtyFlag;
void    CheckBinaryThread (LPVOID);
extern WCHAR szCheckBinaryType[];
extern WCHAR szCheckBinaryTimeout[];
BOOL OFNHookProc(HWND uP1, UINT uP2, WPARAM uP3, LPARAM uP4);


//#ifdef JAPAN
//BOOL CheckPortName(HWND,LPTSTR );
//BOOL PortName(LPTSTR );
//#endif

BOOL APIENTRY InQuotes(LPTSTR sz)
{
	if (*sz == TEXT('"') && *(sz + lstrlen(sz) - 1) == TEXT('"')) {
		return(TRUE);
	}
	return(FALSE);
}

//-------------------------------------------------------------------------
// Removes leading and trailing spaces.
VOID APIENTRY RemoveLeadingSpaces(LPTSTR sz)
{
	WCHAR*pChr2;
	LPTSTR lpTmp;

	while(*sz == TEXT(' ')) {
		// First Char is a space.
		// Move them all down one.
		for (pChr2 = sz; *pChr2; pChr2++) {
			*pChr2 = *(pChr2+1);
		}
	}
	if (!*sz) {
		return;
	}
	lpTmp = sz + lstrlen(sz) - 1;
	while (*lpTmp && *lpTmp == TEXT(' ')) {
		*lpTmp-- = TEXT('\0');
	}
}

/*** GroupFull  -  Check for full group. Returns FALSE if the group isn't full.
 *
 *
 * BOOL PASCAL GroupFull(PGROUP pGroup)
 *
 * ENTRY -    PGROUP pGroup
 *
 * EXIT  -    BOOL  - returns TRUE if group is full
 *
 * SYNOPSIS -
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
BOOL PASCAL GroupFull(PGROUP pGroup)
{
	WORD i=0;
	PITEM pItem;
	BOOL ret;

	// Count the items in the group.
	for (pItem = pGroup->pItems;pItem;pItem = pItem->pNext)
		i++;

	if (i >= CITEMSMAX) {
		MyMessageBox(hwndProgman, IDS_GROUPFILEERR, IDS_TOOMANYITEMS, NULL,
		  MB_OK | MB_ICONEXCLAMATION|MB_SYSTEMMODAL);
		ret = TRUE;
	}
	else
		ret = FALSE;

	return ret;
}


/*** ValidPathDrive
 *
 *
 * BOOL APIENTRY ValidPathDrive(LPTSTR lpPath)
 *
 * ENTRY -    LPTSTR lpPath  - pointer to path
 *
 * EXIT  -    BOOL  - returns TRUE if drive path is valid
 *
 * SYNOPSIS - Check the drive letter of the given path (if it has one).
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
BOOL APIENTRY PASCAL ValidPathDrive(LPTSTR lpPath)
{
	int nValid;
	WCHAR szDriveRoot[4] = TEXT("?:\\");

	// Store first letter of path.
	*szDriveRoot = *lpPath;
	lpPath = CharNext(lpPath);
	if (*lpPath == TEXT(':')) {
		//
		// It's got a drive letter. Test the drive type with the root directory.
		//
		nValid = GetDriveType(szDriveRoot);
		if ( nValid == 1 || nValid == 0) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
	return TRUE;
}


/*** StripArgs        -- strip everything after a space
 *
 *
 * VOID APIENTRY StripArgs(LPTSTR szCmmdLine)
 *
 * ENTRY -  LPTSTR  szCmmdLine  -   Pointer to command line
 *          int     index
 *
 *
 * EXIT  -        VOID
 *
 * SYNOPSIS -  This searches for first space, and places NULL there, stripping
 *                                everything after that
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
void NEAR PASCAL StripArgs(LPTSTR szCmmdLine)
{
	WCHAR*pch;

	//
	// first skip leading spaces
	//
	for (pch = szCmmdLine; *pch && *pch == TEXT(' '); pch = CharNext(pch))
		 ;

	//
	// check if we have a quote, if so look for second quote.
	//
	if (*pch == TEXT('"')) {
		for (pch++; *pch; pch = CharNext(pch)) {
			if (*pch == TEXT('"')) {
				// Found it, limit string at this point.
				pch++;
				*pch = TEXT('\0');
				break;
			}
		}
	}
	else {
		// Search forward to find the first space in the cmmd line.
		for (; *pch; pch = CharNext(pch)) {
			if (*pch == TEXT(' ')) {
				// Found it, limit string at this point.
				*pch = TEXT('\0');
				break;
			}
		}
	}
}


/*** GetPathInfo       -- tokenizes string to path/name/extension components
 *
 *
 * VOID APIENTRY GetPathInfo(LPTSTR szPath, LPTSTR *pszLastSlash, LPTSTR *pszExt, DWORD *pich)
 *
 * ENTRY -  LPTSTR      szPath              -        pointer to path stuff
 *          LPTSTR      *pszLastSlash       -        last slash in path
 *          LPTSTR      *pszExt             -        pointer to extension
 *          WORD        *plich              -        index of last slash
 *
 *
 * EXIT  -  VOID
 *
 * SYNOPSIS -  This searches for first space, and places NULL there, stripping
 *                                everything after that
 *
 * WARNINGS -  BUG BUG , NOT LFN aware!
 * EFFECTS  -
 *
 */
void APIENTRY GetPathInfo(LPTSTR szPath,
	LPTSTR *pszFileName,
	LPTSTR *pszExt,
	WORD *pich,
	BOOL *pfUnc)
{
	WCHAR*pch;          // Temp variable.
	WORD ich = 0;       // Temp.
	BOOL InQuotes;

	*pszExt = NULL;         // If no extension, return NULL.
	*pszFileName = szPath;  // If no seperate filename component, return path.
	*pich = 0;
	*pfUnc = FALSE;         // Default to not UNC style.

	//
	// Check if path is in quotes.
	//
	if (InQuotes = (*szPath == TEXT('"'))) {
	   szPath++;
	}

	// Check for UNC style paths.
	if (*szPath == TEXT('\\') && *(szPath+1) == TEXT('\\'))
		*pfUnc = TRUE;

	// Search forward to find the last backslash or colon in the path.
	// While we're at it, look for the last dot.
	for (pch = szPath; *pch; pch = CharNext(pch)) {

		if ((*pch == TEXT(' ')) && (!InQuotes)) {
			// Found a space - stop here.
			break;
		}
		if (*pch == TEXT('"')) {
			// Found a the second quote - stop here.
			pch++;
			break;
		}
		if (*pch == TEXT('\\') || *pch == TEXT(':')) {
			// Found it, record ptr to it and it's index.
			*pszFileName = pch+1;
			*pich = ich + (WORD)1;
		}
		if (*pch == TEXT('.')) {
			// Found a dot.
			*pszExt = pch;
		}
		ich++;
	}

	/* Check that the last dot is part of the last filename. */
	if (*pszExt < *pszFileName)
		*pszExt = NULL;
}


/*** TagExtension
 *
 *
 * void APIENTRY TagExtension(PSTR pszPath, UINT cbPath)
 *
 * ENTRY -    PSTR pszPath  -
 *            UINT cbPath   - length of path in bytes
 *
 * EXIT  -
 *
 * SYNOPSIS -  Checks a string to see if it has an extension.
 *             If it doesn't then .exe will be appended.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
void FAR PASCAL TagExtension(LPTSTR szPath, UINT cbPath)
{
	DWORD dummy;
	LPTSTR szFileName;
	LPTSTR pszExt;
	LPTSTR pch;

	GetPathInfo(szPath, &szFileName, &pszExt, (WORD*) &dummy, (BOOL*) &dummy);
	if (!pszExt && (sizeof(WCHAR) * (lstrlen(szPath) + 5)) < cbPath) {
		// No extension, tag on a ".exe"
		// but first check if the path is in quotes
		pch = szPath + lstrlen(szPath);
		if (InQuotes(szPath)) {
			*pch = TEXT('\0');
			lstrcat(szPath, TEXT(".exe\""));
		}
		else {
			lstrcat(szPath, TEXT(".exe"));
		}
	}
}

// PIF stuff added 1/9/92

#define PTITLELEN      30
#define PPATHLEN       63
#define PATHMAX        64
#define COMMAX         64
#define PIFEDITMAXPIFL 1024L

typedef struct tagPIFFILE
  {
	WCHAR Reserved1[2];
	WCHAR PTITLE[PTITLELEN];
	WORD MAXMEMWORD;
	WORD MINMEMWORD;
	WCHAR PPATHNAME[PPATHLEN];
	WCHAR MSFLAGS;
	WCHAR Reserved2;
	WCHAR INITIALDIR[PATHMAX];
	WCHAR INITIALCOM[COMMAX];
	WCHAR SCREENTYPE;
	WCHAR SCREENPAGES;
	WCHAR INTVECLOW;
	WCHAR INTVECHIGH;
	WCHAR ROWS;
	WCHAR COLUMNS;
	WCHAR ROWOFFS;
	WCHAR COLOFFS;
	WORD SYSTEMMEM;
	WCHAR SHAREDPROG[64];
	WCHAR SHAREDDATA[64];
	WCHAR BEHAVBYTE;
	WCHAR SYSTEMFLAGS;
  } PIFFILE ;

void NEAR PASCAL GetTheString(LPTSTR pDst, LPTSTR pSrc, UINT iLen)
{
	WCHAR cTemp;

	// Ensure there is NULL termination, and then copy the description
	cTemp = pSrc[iLen];
	pSrc[iLen] = TEXT('\0');
	lstrcpy(pDst, pSrc);
	pSrc[iLen] = cTemp;

	// Strip off trailing spaces
	for (pSrc=NULL; *pDst; pDst=CharNext(pDst))
		if (*pDst != TEXT(' '))
			pSrc = pDst;
	if (pSrc)
		*CharNext(pSrc) = TEXT('\0');
}

void NEAR PASCAL GetStuffFromPIF(LPTSTR szPath, LPTSTR szName, LPTSTR szDir)
{
	WCHAR szTemp[MAX_PATH];
	DWORD dummy;
	LPTSTR pszExt;
	LPTSTR pszFileName;
	PIFFILE pfTemp;
	HANDLE fh;
	DWORD dwBytesRead ;


	// Do nothing if the user has filled in these fields
	if (*szName && *szDir)
		return;

	// Check for the ".pif" extension
	lstrcpy(szTemp, szPath);
	StripArgs(szTemp);
	GetPathInfo(szTemp, &pszFileName, &pszExt, (WORD*) &dummy,
					  (BOOL*) &dummy);
	if (!pszExt || lstrcmpi(pszExt, szDotPIF))
		return;

  /* There is no real way to verify the PIF format, like the COM format,
   * so we are assuming that the extension is our verification
   */
	fh = CreateFile(szTemp,
				GENERIC_READ,FILE_SHARE_READ, NULL,
				OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0)  ;

	if ( fh == INVALID_HANDLE_VALUE)
		return;

	ReadFile(fh,(LPTSTR)&pfTemp,sizeof(pfTemp),&dwBytesRead,NULL) ;
	if (dwBytesRead == sizeof(pfTemp)
		 && SetFilePointer(fh, 0L, NULL, FILE_END) < PIFEDITMAXPIFL) {
		if (!*szName) {
			GetTheString(szName, pfTemp.PTITLE, CharSizeOf(pfTemp.PTITLE));
			DoEnvironmentSubst(szName, CharSizeOf(szNameField));
		}

		if (!*szDir) {
			GetTheString(szDir, pfTemp.INITIALDIR, CharSizeOf(pfTemp.INITIALDIR));
		}
	}
	CloseHandle(fh);
}



/*** GetDirectoryFromPath
 *
 *
 * VOID APIENTRY GetDirectoryFromPath (PSTR szFilePath, PSTR szDir)
 *
 * ENTRY -    PSTR szFilePath   -  Full path to a file.
 *            PSTR szDir        -  Directory returned in here, the buffer
 *                                 is assumed to be as big as szFilePath.
 *
 * EXIT  -
 *
 * SYNOPSIS - Given a full path, returns the directory of the file specified
 *            by the path.  If the path is UNC style or the path is just a
 *            filename then the directory returned will be a NULL.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
VOID FAR PASCAL GetDirectoryFromPath(LPTSTR szFilePath, LPTSTR szDir)
{
	LPTSTR pFileName;
	LPTSTR pExt;
	WORD ich;
	BOOL fUnc;

	*szDir = TEXT('\0');

	/* Get info about file path. */
	GetPathInfo(szFilePath, &pFileName, &pExt, &ich, &fUnc);

	/* UNC paths don't (conceptually to Progman) have a directory component. */
	if (fUnc)
		return;

	/* Does it have a directory component ? */
	if (pFileName != szFilePath) { // Yep.
		/* copy path to temp. */
		if (*szFilePath == TEXT('"')) {
			szFilePath++;
		}
		lstrcpy(szDir, szFilePath);
		/* check path style. */

		if (ich <= 3 && *(szDir+1) == TEXT(':')){

			/*
			 * The path is "c:\foo.c" or "c:foo.c" style.
			 * Don't remove the last slash/colon, just the filename.
			 */
			szDir[pFileName-szFilePath] = TEXT('\0');
		}
		else if (ich == 1) {
			/*
			 * something like "\foo.c"
			 * Don't remove the last slash/colon, just the filename.
			 */
			szDir[pFileName-szFilePath] = TEXT('\0');
		}
		else {
			/*
			 * The filepath is a full normal path.
			 * Could be something like "..\foo.c" or ".\foo.c" though.
			 * Stomp on the last slash to get just the path.
			 */
			szDir[pFileName-szFilePath-1] = TEXT('\0');
		}
	}

	/* else just a filename with no path. */
}

/*** GetFilenameFromPath
 *
 *
 * PSTR APIENTRY GetFilenameFromPath (PSTR szPath)
 *
 * ENTRY -    PSTR szPath  -
 *
 * EXIT  -    PSTR  -
 *
 * SYNOPSIS -  Given a full path returns a ptr to the filename bit.
 *             Unless it's a UNC style path in which case it returns the path.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
VOID FAR PASCAL GetFilenameFromPath(LPTSTR szPath, LPTSTR szFilename)
{
	DWORD dummy;
	LPTSTR pFileName;
	LPTSTR pExt;
	BOOL fUNC;


	GetPathInfo(szPath, &pFileName, &pExt, (WORD*) &dummy, &fUNC);

	/* If it's a UNC then the 'filename' part is the whole thing. */
	if (fUNC || (szPath == pFileName))
		lstrcpy(szFilename, szPath);
	else {
		if (*szPath == TEXT('"')) {
			*szFilename++ = TEXT('"');
		}
		lstrcpy(szFilename, pFileName);
	}
}

/*** HandleDosApps
 *
 *
 * void HandleDosApps (PSTR sz)
 *
 * ENTRY -    PSTR sz  -  full path sans arguments
 *
 * EXIT  -
 *
 * SYNOPSIS - Takes a full path to a file and checks if it's a dos app mentioned
 *            in the pif.inf file.
 *            If it isn't in the pif.inf file then the the routine returns without
 *            doing anything.
 *            If it is then it looks for a pif file with the same base name
 *            in the same directory.
 *            If there isn't one then it checks the windows directory.
 *            If that fails it calls setup to create a new one giving the full
 *            path to the app in question. - NOT supported in NTSetup.
 *            The procedure doesn't alter the input string.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91  JohanneC  - ported from Windows 3.1
 *
 */
void FAR PASCAL HandleDosApps(LPTSTR sz)
{
	WCHAR szPath[MAX_PATH];  // Copy of path so we can stomp all over
									// it.
	LPTSTR pszFileName;               // The file filename part.
	LPTSTR pszExt;                    // The extension.
	WORD ich;                       // Index to filename.
	WCHAR szPifSection[32];          // Section in file to search,
	WCHAR szPifIniFile[16];          // Ini file to check.
	WCHAR szSystemDir[MAX_PATH]; // Path to system dir.
	WCHAR szReturnString[2];         // Mini buffer to check return from GPPS.
	//OFSTRUCT of;                    // OF struct.
	WCHAR szExecSetup[MAX_PATH]; // String used to WinExec setup to
										// create the pif for this app.
	BOOL dummy;                     // Dummy variable.
	DWORD   dwResult ;
	WCHAR   szPathFieldTemp[MAXITEMPATHLEN+1] ;
	LPTSTR  FilePart ;

	/* Get system dir. */
	GetSystemDirectory(szSystemDir, CharSizeOf(szSystemDir));

	/* Load ini file info. */
	LoadString(hAppInstance, IDS_PIFINIFILE, szPifIniFile, CharSizeOf(szPifIniFile));
	LoadString(hAppInstance, IDS_PIFSECTION, szPifSection, CharSizeOf(szPifSection));
	LoadString(hAppInstance, IDS_EXECSETUP, szExecSetup, CharSizeOf(szExecSetup));

	/* Set up path to inf file. */
	if (lstrlen(szSystemDir) > 3) {
		lstrcat(szSystemDir, TEXT("\\"));
	}
	lstrcat(szSystemDir, szPifIniFile);

	/* Copy path */
	lstrcpy(szPath, sz);

	/* Get info about the path. */
	GetPathInfo(szPath, &pszFileName, &pszExt, &ich, &dummy);

#ifdef DEBUG
	OutputDebugString(TEXT("\n\rLooking in apps.ini for "));
	OutputDebugString(pszFileName);
	OutputDebugString(TEXT("\n\r"));
#endif

	/* Init the default to null. */
	szReturnString[0] = TEXT('\0');

	/*
	 * Check in pif.ini file.
	 * GPPS([section], keyname, szDef, szResultString, cbResString, FileName);
	 */
	GetPrivateProfileString(szPifSection, pszFileName, szReturnString,
		szReturnString, CharSizeOf(szReturnString), szSystemDir);
	if (szReturnString[0] == TEXT('\0')) {
		/* It's not there. */
#ifdef DEBUG
		OutputDebugString(TEXT("App not in inf file"));
		OutputDebugString(TEXT("\n\r"));
#endif
		return;
	}

#ifdef DEBUG
	OutputDebugString(TEXT("App in inf file"));
	OutputDebugString(TEXT("\n\r"));
#endif

	/*
	 * It's in the pif file, there should be a .pif for it somewhere.
	 * Change extension to .pif
	 */
	if (pszExt) {
		// copy .pif\0 over the existing extension.
		lstrcpy(pszExt, TEXT(".pif"));
	}
	else {
		// cat .pif\0 onto the end.
		lstrcat(szPath,TEXT(".pif"));
	}

	// Check given directory first.
#ifdef DEBUG

	OutputDebugString(TEXT("Checking "));
	OutputDebugString(szPath);
	OutputDebugString(TEXT("\n\r"));
#endif

//SearchPath!!!
	dwResult = SearchPath(NULL,szPath,NULL,MAXITEMPATHLEN+1,
				szPathFieldTemp,&FilePart) ;
	if (dwResult == 0 && GetLastError() != 0x20) {
	//if (OpenFile(szPath, &of, OF_EXIST) == (HFILE)-1 && of.nErrCode != 0x20) {
		// It's not there.

#ifdef DEBUG
		OutputDebugString(TEXT("Checking "));
		OutputDebugString(pszFileName);
		OutputDebugString(TEXT("\n\r"));
#endif

		// Check path.
		dwResult = SearchPath(NULL,pszFileName,NULL,MAXITEMPATHLEN+1,
				szPathFieldTemp,&FilePart) ;
		if (dwResult == 0 && GetLastError() != 0x20) {
		//if (! (OpenFile(pszFileName, &of, OF_EXIST) == (HFILE)-1 && of.nErrCode !=0x20) ) {
			// Found it on path - winoldapp app will find it,
#ifdef DEBUG
			OutputDebugString(TEXT("Found it on path. "));
			OutputDebugString(szPathFieldTemp);
			OutputDebugString(TEXT("\n\r"));
#endif
		}
		else {
#ifdef LATER
			//
			// NT Setup does not support this.
			// sunilp said so 9-9-92
			//

			// It's not anywhere so get setup to create it.
			lstrcat(szExecSetup, sz);

			// Exec setup - REVIEW silently ignore any errors from win exec.
			WinExec(szExecSetup, SW_SHOWNORMAL);
#endif
		}
	}
}



/*** FixUpNulls        -- replace "#" with NULL
 *
 *
 * VOID APIENTRY FixUpNulls(PSTR p)
 *
 * ENTRY -         PSTR        p                        -        pointer to string to replaces chars
 *
 *
 * EXIT  -        VOID
 *
 * SYNOPSIS -  seraches for any "#" chars, and replaces them with NULL
 *
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
void NEAR PASCAL FixupNulls(LPTSTR p)
{
	while (*p) {
		if (*p == TEXT('#'))
			*p = TEXT('\0');
		p++;
	}
}

/*** PMHelp        --
 *
 *
 * VOID APIENTRY PMHelp(HWND hwnd)
 *
 * ENTRY -         HWND        hwnd
 *
 *
 * EXIT  -        VOID
 *
 * SYNOPSIS -
 *
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
void FAR PASCAL PMHelp(HWND hwnd)
{
	SetCurrentDirectory(szOriginalDirectory);
	if (!WinHelp(hwnd, szProgmanHelp, HELP_CONTEXT, dwContext)) {
		MyMessageBox(hwnd, IDS_APPTITLE, IDS_WINHELPERR, NULL, MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL);
	}
	SetCurrentDirectory(szWindowsDirectory);
}


/*** MyDialogBox        --
 *
 *
 * WORD APIENTRY MyDialogBox(WORD idd, HWND hwndParent, FARPROC lpfnDlgProc)
 *
 * ENTRY -  WORD        idd
 *          HWND        hwnd
 *          FARPROC     lpfnDlgProc
 *
 * EXIT  -  WORD        xxx
 *
 * SYNOPSIS -
 *
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
WORD APIENTRY MyDialogBox(WORD idd, HWND hwndParent, DLGPROC lpfnDlgProc)
{
  WORD          wRet;
  DWORD dwSave = dwContext;

  dwContext = IDH_DLGFIRST + idd;

  wRet = (WORD)DialogBox(hAppInstance, (LPTSTR) MAKEINTRESOURCE(idd), hwndParent,
		lpfnDlgProc);
  dwContext = dwSave;
  return(wRet);
}


/*** ChooserDlgProc --         Dialog Procedure for chooser
 *
 *
 *
 * ChooserDlgProc(HWND hwnd, UINT uiMsg, DWORD wParam, LPARAM lParam)
 *
 * ENTRY -  HWND hhwnd      - handle to dialog box.
 *          UINT uiMsg       - message to be acted upon.
 *          WPARAM wParam   - value specific to uiMsg.
 *          LPARAM lParam   - value specific to uiMsg.
 *
 * EXIT  -  True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
INT_PTR APIENTRY ChooserDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  register WORD wTempSelection;

  switch (uiMsg)
	{
  case WM_INITDIALOG:
		  if (!AccessToCommonGroups) {
			  EnableWindow(GetDlgItem(hwnd, IDD_COMMONGROUP), FALSE);
		  }
		  wNewSelection = SelectionType();
		  if (wNewSelection == TYPE_ITEM)
			  wTempSelection = IDD_ITEM;
		  else {
			  wTempSelection = IDD_PERSGROUP;
			  wNewSelection = TYPE_PERSGROUP;
	  }

		  /* Grey out illegal items. */
		  if (!pCurrentGroup || !GroupCheck(pCurrentGroup)) {
			  /* Group is RO - can't create new items. */
			  EnableWindow(GetDlgItem(hwnd,IDD_ITEM), FALSE);
			  wTempSelection = IDD_PERSGROUP;
			  wNewSelection = TYPE_PERSGROUP;
		  }

		  if (dwEditLevel == 1) {
			  /* Not allowed to create new groups. */
			  EnableWindow(GetDlgItem(hwnd,IDD_PERSGROUP), FALSE);
			  wTempSelection = IDD_ITEM;
			  wNewSelection = TYPE_ITEM;
		  }

		  CheckRadioButton(hwnd, IDD_ITEM, IDD_COMMONGROUP, wTempSelection);
		  break;

	  case WM_COMMAND:
		  switch(GET_WM_COMMAND_ID(wParam, lParam))
			{
			  case IDD_HELP:
						goto DoHelp;

			  case IDD_ITEM:
				  if (IsWindowEnabled(GetDlgItem(hwnd,IDD_ITEM)))
					  wNewSelection = TYPE_ITEM;
				  break;

			  case IDD_PERSGROUP:
				  if (IsWindowEnabled(GetDlgItem(hwnd,IDD_PERSGROUP)))
					  wNewSelection = TYPE_PERSGROUP;
				  break;

			  case IDD_COMMONGROUP:
				  if (IsWindowEnabled(GetDlgItem(hwnd,IDD_COMMONGROUP)))
					  wNewSelection = TYPE_COMMONGROUP;
				  break;

			  case IDOK:
				  EndDialog(hwnd, TRUE);
				  break;

			  case IDCANCEL:
				  EndDialog(hwnd, FALSE);
				  break;

			  default:
				  return(FALSE);
			}
		  break;

	  default:

		  if (uiMsg == uiHelpMessage) {
DoHelp:
			  PMHelp(hwnd);

			  return TRUE;
		  } else
			  return FALSE;
	}
  UNREFERENCED_PARAMETER(lParam);
  return(TRUE);
}


/*** MoveItemDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY MoveItemDlgProc(HWND hwnd, UINT uiMsg, DWORD wParam, LONG lParam)
 *
 * ENTRY -  HWND hwnd       - handle to dialog box.
 *          UINT uiMsg       - message to be acted upon.
 *          WPARAM wParam   - value specific to uiMsg.
 *          LPARAM lParam   - value specific to uiMsg.
 *
 * EXIT  -     True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

INT_PTR APIENTRY MoveItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)

{
  int               iSel;
  HWND              hwndCB;
  register PGROUP   pGroup;

  hwndCB = GetDlgItem(hwnd, IDD_GROUPS);

  switch (uiMsg) {
	  case WM_INITDIALOG:
	  {
		  LPITEMDEF  lpid;
		  LPGROUPDEF lpgd;
		  int i=0;

		  lpgd = LockGroup(pCurrentGroup->hwnd);

		  if (lpgd == 0L)
			  goto MoveDlgExit;

		  lpid = ITEM(lpgd,pCurrentGroup->pItems->iItem);

		  SetDlgItemText(hwnd, IDD_ITEM, (LPTSTR) PTR(lpgd, lpid->pName));
		  SetDlgItemText(hwnd, IDD_GROUP, (LPTSTR) PTR(lpgd, lpgd->pName));

		  UnlockGroup(pCurrentGroup->hwnd);

		  pGroup = pFirstGroup;
		  while (pGroup) {
			  if (IsGroupReadOnly(pGroup->lpKey, pGroup->fCommon))
				  pGroup->fRO = TRUE;
			  else
				  pGroup->fRO = FALSE;
			  if (!pGroup->fRO) {

				  if (pGroup != pCurrentGroup) {
					  GetWindowText(pGroup->hwnd, (LPTSTR)szMessage, MAXGROUPNAMELEN + 1);
					  iSel = (int)SendMessage(hwndCB, CB_ADDSTRING, 0, (LPARAM)(LPTSTR)szMessage);
					  SendMessage(hwndCB, CB_SETITEMDATA, iSel, (LPARAM)(LPTSTR)pGroup);
					  i++;
				  }
			  }
			  pGroup = pGroup->pNext;
		  }
		  SendMessage(hwndCB, CB_SETCURSEL, 0, 0L);
		  if (i==0) {
			  /* No items in list box - Kill the OK button. */
			  EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
		  }
		  break;
	  }

	  case WM_COMMAND:
		  switch(GET_WM_COMMAND_ID(wParam, lParam)) {
			  case IDD_HELP:
				  goto DoHelp;

			  case IDOK:
			  {
				  int       nIndex;
				  PGROUP    pGTemp;

				  /* Get the pointer to the selected group. */
				  nIndex = (int)SendMessage(hwndCB, CB_GETCURSEL, 0, 0L);
				  pGroup = (PGROUP)SendMessage(hwndCB, CB_GETITEMDATA, nIndex, 0L);

				  /* Check pointer. */
				  if (!pGroup)
					  goto ExitCase;

				  pGTemp = pCurrentGroup;

				  /*
				   * Only do this operation if the user actually had a
				   * group selected.  CB_GETITEMDATA will return -1 if
				   * an item wasn't selected.  This happens if the user
				   * tries to do a move when there's only one group around.
				   */
				  if (pGroup != (PGROUP)(-1)) {
					  if (DuplicateItem(pGTemp,pGTemp->pItems,pGroup,NULL)) {

						  // OK to delete the original.
						  DeleteItem(pGTemp,pGTemp->pItems);
// It's messy to change the focus when using the keyboard.
// SendMessage(hwndMDIClient, WM_MDIACTIVATE, (WPARAM)(pGroup->hwnd), 0L);
						  CalcGroupScrolls(pGroup->hwnd);
					  }
				  }

ExitCase:
				  EndDialog(hwnd, TRUE);
				  break;
			  }

			  case IDCANCEL:
MoveDlgExit:
				  EndDialog(hwnd, FALSE);
				  break;

			  default:
				  return(FALSE);
		  }
		  break;

	  default:

		  if (uiMsg == uiHelpMessage)
DoHelp:
		  {
			  DWORD dwSave = dwContext;

			  dwContext = IDH_MOVEDLG;
			  PMHelp(hwnd);
			  dwContext = dwSave;
			  return TRUE;
		  } else
			  return FALSE;
	}
  UNREFERENCED_PARAMETER(lParam);
  return(TRUE);
}


/*** CopyItemDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY CopyItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
 *
 * ENTRY -         HWND hhwnd                 - handle to dialog box.
 *                        UINT uiMsg                  - message to be acted upon.
 *                 WPARAM wParam        - value specific to uiMsg.
 *                 LPARAM lParam        - value specific to uiMsg.
 *
 * EXIT  -           True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

INT_PTR APIENTRY CopyItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)

{
	int               iSel;
	HWND              hwndCB;
	register PGROUP   pGroup;
	LPITEMDEF         lpid;
	LPGROUPDEF        lpgd;

	hwndCB = GetDlgItem(hwnd, IDD_GROUPS);

	switch (uiMsg) {
	case WM_INITDIALOG:
	{
		int i=0;

		LoadString(hAppInstance, IDS_COPYDLGTITLE, (LPTSTR)szTitle, MAXTITLELEN);
		SetWindowText(hwnd, szTitle);
		LoadString(hAppInstance, IDS_COPYDLGTITLE1, (LPTSTR)szTitle, MAXTITLELEN);
		SetDlgItemText(hwnd, IDD_MOVETITLE1, szTitle);

		lpgd = LockGroup(pCurrentGroup->hwnd);

		if (lpgd == 0L)
			goto CopyDlgExit;

		lpid = ITEM(lpgd,pCurrentGroup->pItems->iItem);

		SetDlgItemText(hwnd, IDD_ITEM, (LPTSTR) PTR(lpgd, lpid->pName));
		SetDlgItemText(hwnd, IDD_GROUP, (LPTSTR) PTR(lpgd, lpgd->pName));

		UnlockGroup(pCurrentGroup->hwnd);

		pGroup = pFirstGroup;
		while (pGroup) {
			if (IsGroupReadOnly(pGroup->lpKey, pGroup->fCommon))
				pGroup->fRO = TRUE;
			else
				pGroup->fRO = FALSE;

			if (!pGroup->fRO) {

				GetWindowText(pGroup->hwnd, (LPTSTR)szMessage, MAXGROUPNAMELEN + 1);
				iSel = (int)SendMessage(hwndCB, CB_ADDSTRING, 0, (LPARAM)(LPTSTR)szMessage);
				SendMessage(hwndCB, CB_SETITEMDATA, iSel, (LPARAM)(LPTSTR)pGroup);
				i++;
			}
			pGroup = pGroup->pNext;
		}
		SendMessage(hwndCB, CB_SETCURSEL, 0, 0L);
		if (i==0) {
			// No items in list box - Kill the OK button.
			EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
		}
		break;
	}

	case WM_COMMAND:
		switch(GET_WM_COMMAND_ID(wParam, lParam)) {
		case IDD_HELP:
			goto DoHelp;

		case IDOK:
		{
			int            nIndex;

			/* Get the pointer to the selected group. */
			nIndex = (int)SendMessage(hwndCB, CB_GETCURSEL, 0, 0L);
			pGroup = (PGROUP)SendMessage(hwndCB, CB_GETITEMDATA, nIndex, 0L);

			/* Check pointer. */
			if (!pGroup)
				goto ExitCase;

			DuplicateItem(pCurrentGroup,pCurrentGroup->pItems,
								pGroup,NULL);
// Don't change focus to destination on a copy, very messy if you have a
// group maximised and you want to copy a whole bunch of stuff to another
// group using the keyboard.
// SendMessage(hwndMDIClient, WM_MDIACTIVATE, (WPARAM)(pGroup->hwnd), 0L);

			/* Redo scroll bars for destination. */
			CalcGroupScrolls(pGroup->hwnd);
			/* Redo scroll bars for source. */
			CalcGroupScrolls(pCurrentGroup->hwnd);

ExitCase:
			EndDialog(hwnd, TRUE);
			break;
		}

		case IDCANCEL:
CopyDlgExit:
			EndDialog(hwnd, FALSE);
			break;

		default:
			return(FALSE);
		}
		break;

	default:

		if (uiMsg == uiHelpMessage)
DoHelp:
		{
			DWORD dwSave = dwContext;

			dwContext = IDH_COPYDLG;
			PMHelp(hwnd);
			dwContext = dwSave;
			return TRUE;
		} else
			return FALSE;
	}
	UNREFERENCED_PARAMETER(lParam);
	return(TRUE);
}

/*** SaveRecentFileList --    Save the list of recently used files
 *
 * void APIENTRY SaveRecentFileList (HWND hwnd, LPTSTR szCurrentFile);
 *
 *
 *
 * ENTRY - HWND   hwnd            - handle to dialog box.
 *         LPTSTR szCurrentFile   - pointer to selected filename
 *         WORD   idControl       - control id
 *
 * EXIT  -
 * SYNOPSIS -
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */


void APIENTRY SaveRecentFileList (HWND hwnd, LPTSTR szCurrentFile, WORD idControl)
{
	HKEY  hKey;
	DWORD dwDisp;
	DWORD dwDataType, dwMaxFiles=INIT_MAX_FILES, dwMaxFilesSize, dwCount;
	WCHAR szFileEntry[20];
	DWORD dwEnd=0;
	DWORD dwFileNum=0;
	DWORD dwDup;
	static WCHAR szRecentFilePath[MAX_PATH];

	//
	// Open registry key
	//

	if ( RegCreateKeyEx (HKEY_CURRENT_USER, FILES_KEY, 0, 0,
							 REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
							 NULL, &hKey, &dwDisp) != ERROR_SUCCESS) {
		return;
	}


	//
	// Query the max number of files to save first.
	//

	dwMaxFilesSize = sizeof (DWORD);

	RegQueryValueEx (hKey, MAXFILES_ENTRY, NULL, &dwDataType,
					(LPBYTE)&dwMaxFiles, &dwMaxFilesSize);

	//
	// If the user request 0 entries, then exit now.
	//
	if (dwMaxFiles == 0) {
		RegCloseKey (hKey);
		return;
	}

	//
	// Find out how many items are in the list box.
	//

	dwEnd = (DWORD)SendDlgItemMessage (hwnd, idControl, CB_GETCOUNT, 0, 0);

	//
	// If the max number of items we want to save is less than the
	// number of entries, then change the ending point.
	//

	if (dwMaxFiles < dwEnd) {
		dwEnd = dwMaxFiles;
	}

	//
	// Add the first entry (the current file)
	//

	wsprintf (szFileEntry, FILE_ENTRY, dwFileNum++);
	dwMaxFilesSize = MAXITEMPATHLEN+1;

	RegSetValueEx (hKey, szFileEntry, 0, REG_SZ, (CONST BYTE *)szCurrentFile,
				   sizeof (WCHAR) * (lstrlen (szCurrentFile)+1));


	//
	// Check for a duplicate string.
	//

	dwDup = (DWORD)SendDlgItemMessage (hwnd, idControl, CB_FINDSTRING,
									   (WPARAM) -1, (LPARAM) szCurrentFile);

	//
	// If we already have dwMaxFiles in the list and we don't have any
	// duplicates, then we only want to save dwMaxFiles - 1 entries
	// (drop the last entry).
	//
	//

	if ( (dwEnd == dwMaxFiles) && (dwDup == CB_ERR) ) {
		dwEnd--;
	}

	//
	// Now loop through the remaining entries
	//

	for (dwCount=0; dwCount < dwEnd; dwCount++) {

		//
		// Check to see if we are at the duplicate entry.  If
		// so skip on to the next item.
		//

		if ((dwDup != CB_ERR) && (dwCount == dwDup)) {
			continue;
		}

		//
		// Get an entry out of the listbox.
		//

		SendDlgItemMessage (hwnd, idControl, CB_GETLBTEXT, (WPARAM) dwCount,
							(LPARAM) szRecentFilePath);

		//
		// If we get a NULL string, break out of the loop.
		//

		if (!(*szRecentFilePath) || !szRecentFilePath) {
			break;
		}

		//
		// Build the entry name
		//

		wsprintf (szFileEntry, FILE_ENTRY, dwFileNum);
		dwMaxFilesSize = MAXITEMPATHLEN+1;

		//
		// Save the entry
		//

		RegSetValueEx (hKey, szFileEntry, 0, REG_SZ,(CONST BYTE *) szRecentFilePath,
					   sizeof (WCHAR) * (lstrlen (szRecentFilePath)+1));

		//
		// Increment our current file number
		//

		dwFileNum++;
	}

	//
	// Close the key
	//

	RegCloseKey (hKey);

}

/*** CheckBinaryThread --
 *
 *
 *
 * void CheckBinaryThread (HWND, INT)
 *
 * ENTRY -  HWND hDlg       - handle to dialog box.
 *          INT  idCheckBox - Checkbox control id
 *
 * EXIT  -  void
 *
 *  05-18-94 Eric Flo  - created
 *
 */

void CheckBinaryThread (LPVOID hwndDlg)
{
	HWND   hwnd = (HWND) hwndDlg;
	WCHAR  szFullPath[MAX_PATH];
	LPTSTR FilePart ;
	DWORD  dwBinaryType;


	while (bCheckBinaryType) {

		WaitForSingleObject (hCheckBinaryEvent, INFINITE);

		if (bCheckBinaryType) {

			DoEnvironmentSubst(szPathField, CharSizeOf(szPathField));
			StripArgs(szPathField);
			TagExtension(szPathField, sizeof(szPathField));
			SheRemoveQuotes(szPathField);
			if (SearchPath(NULL, szPathField, NULL, MAXITEMPATHLEN+1,
						   szFullPath, &FilePart) &&
						   GetBinaryType(szFullPath, &dwBinaryType) &&
						   dwBinaryType == SCS_WOW_BINARY) {
				CheckDlgButton(hwnd, IDD_NEWVDM, 0);
				EnableWindow(GetDlgItem(hwnd,IDD_NEWVDM), TRUE);

			} else {
				CheckDlgButton(hwnd, IDD_NEWVDM, 1);
				EnableWindow(GetDlgItem(hwnd,IDD_NEWVDM), FALSE);
			}

		}

	}

	//
	// Close our event handle and exit the thread.
	//

	CloseHandle (hCheckBinaryEvent);
	ExitThread (0);
}



/*** ValidatePath --
 *
 *
 *
 * DWORD APIENTRY ValidatePath(HWND, PSTR, PSTR, PSTR)
 *
 * ENTRY -  HWND hDlg       - handle to dialog box.
 *          PSTR szPath     - path to item.
 *          PSTR szDir      -  path to working directory.
 *          PSTR szExePath  -  path to associated exe.
 *
 * EXIT  -  DWORD       - returns PATH_VALID is path is valid.
 *                        if path is not valid returns PATH_INVALID_OK
 *                        if user hits OK to invalid message box,
 *                        otherwise returns PATH_VALID
 *
 * SYNOPSIS -
 *
 * WARNINGS -
 * EFFECTS  -
 *
 *  08-08-91 JohanneC  - updated to Windows 3.1
 *
 */

DWORD APIENTRY ValidatePath(HWND hDlg, LPTSTR szPath, LPTSTR szDir, LPTSTR szExePath)
{
  int cDrive;
  WCHAR szTemp[MAX_PATH];
  int err;
  BOOL bOriginalDirectory;  // using original directory
  DWORD dwRet = PATH_VALID;
  BOOL fOK = TRUE;      // If the path or the dir check fail
						// then don't bother with the assoc check.

  /* Check working directory. */
  GetCurrentDirectory(MAXITEMPATHLEN, szTemp);
  SetCurrentDirectory(szOriginalDirectory);

  //CharToOem(szDir, szDir);
  /* Allow dir field to be NULL without error. */
  if (szDir && *szDir) {

	  /* Remove leading spaces. */
	  // RemoveLeadingSpaces(szDir);
	  SheRemoveQuotes(szDir);


	  //
	  // Test if the directory szDir is valid.
	  //
	  if (!ValidPathDrive(szDir) ||  !SetCurrentDirectory(szDir)) {
		  if (MyMessageBox(hDlg, IDS_BADPATHTITLE, IDS_BADPATHMSG3, NULL, MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION) == IDCANCEL)
			  goto ExitFalse;
		  fOK = FALSE;
	  }
  }

  //
  // Before searching for the executable, check the validity of the szPath
  // drive if there's one specified.
  //
  if (!ValidPathDrive(szPath)) {
	  if (MyMessageBox(hDlg, IDS_BADPATHTITLE, IDS_BADPATHMSG, szPath, MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION) == IDCANCEL)
		  goto ExitFalse;
	  fOK = FALSE;
  }
  else {
	  err = (int)((DWORD_PTR)FindExecutable(szPath, szDir, szExePath));
  }

  /* Check if the file exists if there is no associated exe. */
  if (fOK && !*szExePath) {
	  if (err == SE_ERR_FNF) {
		  /* File doesn't exist. */
		  if (MyMessageBox(hDlg, IDS_BADPATHTITLE, IDS_BADPATHMSG, szPath, MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION) == IDCANCEL)
			  goto ExitFalse;
	  }
	  else {
		  /* Association is bogus. */
		  if (MyMessageBox(hDlg, IDS_BADPATHTITLE, IDS_BADPATHMSG2, szPath, MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION) == IDCANCEL)
			  goto ExitFalse;
		  else
		  dwRet = PATH_INVALID_OK;
	  }
  }

  /* Warn people against using removable or remote paths. */

  /* HACK:  Hard code UNC path format.  Yuck. */
  if (szPath[0] == TEXT('\\') && szPath[1] == TEXT('\\'))
	  goto VPWarnNet;

  if (szPath[1] != TEXT(':') ) {
	  cDrive = (int)((DWORD_PTR)CharUpper((LPTSTR)szTemp[0]));
	  cDrive = cDrive - (INT)TEXT('A');
	  bOriginalDirectory = TRUE;
  }
  else {
	  cDrive = (int)((DWORD_PTR)CharUpper((LPTSTR)szPath[0]));
	  cDrive = cDrive - (INT)TEXT('A');
	  bOriginalDirectory = FALSE;
  }

  /* Change back to old directory. */
  SetCurrentDirectory(szTemp);
  //OemToChar(szDir, szDir);

  if (IsRemovableDrive(cDrive)) {
	  if (MyMessageBox(hDlg, IDS_REMOVEPATHTITLE, IDS_PATHWARNING, NULL, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2) == IDNO)
		  return(PATH_INVALID);
	  else
		  return(dwRet);
  }

  if (IsRemoteDrive(cDrive) && !bOriginalDirectory) {
VPWarnNet:
	  if (MyMessageBox(hDlg, IDS_NETPATHTITLE, IDS_PATHWARNING, NULL, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2) == IDNO)
		  return(PATH_INVALID);
  }
  return(dwRet);

ExitFalse:
  // Change back to old directory.
  SetCurrentDirectory(szTemp);
  //OemToChar(szDir, szDir);
  return(PATH_INVALID);
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  IconFileExists() -                                                      */
/*                                                                          */
/* Checks if the file exists, if it doesn't it tries tagging on .exe and    */
/* if that fails it reports an error. The given path is environment expanded. */
/* If it needs to put up an error box, it changes the cursor back.          */
/* Path s assumed to be MAXITEMPATHLEN long.                                */
/* The main reason for moving this out of the DlgProc was because we're     */
/* running out of stack space on the call to the comm dlg.                  */
/*                                                                          */
/*--------------------------------------------------------------------------*/
BOOL NEAR PASCAL IconFileExists(HWND hWnd, HCURSOR hCur, LPTSTR szPath)
{
	WCHAR   szExtended[MAX_PATH];  // Path with .exe if needed.
	BOOL   ret = TRUE;
	//OFSTRUCT of;
	HCURSOR hCursor;
	DWORD   dwResult ;
	WCHAR   szPathFieldTemp[MAX_PATH] ;
	LPTSTR  FilePart ;

	/* Check Files existance. */
	DoEnvironmentSubst(szPath, MAX_PATH);
	StripArgs(szPath);
	if (*szPath == TEXT('"')) {
		SheRemoveQuotes(szPath);
	}
	lstrcpy(szExtended, szPath);

	// use SearchPath instead of MOpenFile(OF_EXIST) to see if a file exists

	dwResult = SearchPath(NULL,szPath,NULL,MAXITEMPATHLEN+1,
				szPathFieldTemp,&FilePart) ;

	if (dwResult == 0 && GetLastError() != 0x20) {

		dwResult = SearchPath(NULL,szExtended,TEXT(".exe"),MAXITEMPATHLEN+1,
				szPathFieldTemp,&FilePart) ;
		if (dwResult == 0 && GetLastError() != 0x20) {

			ShowCursor(FALSE);
			hCursor = SetCursor(hCur);
			/*
			 * File doesn't exist,tidier if we report the error with the
			 * unextended path only.
			 */
			MyMessageBox(hWnd, IDS_NOICONSTITLE, IDS_BADPATHMSG,
										szPath, MB_OK | MB_ICONEXCLAMATION);
			ret = FALSE;
			ShowCursor(TRUE);
			SetCursor(hCursor);
		}
		else {
			lstrcpy(szPath, FilePart);
		}
	}
	return ret;
}

/*** GetRidOfIcon --
 *
 *
 *
 * VOID APIENTRY GetRidOfIcon(VOID)
 *
 * ENTRY -         VOID
 *
 * EXIT  -        VOID
 *
 * SYNOPSIS -
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

VOID APIENTRY GetRidOfIcon(VOID)
{

	if (hDlgIcon && (hDlgIcon != hItemIcon) &&
					(hDlgIcon != hProgmanIcon) &&
					(hDlgIcon != hGroupIcon))
		DestroyIcon(hDlgIcon);
	hDlgIcon = NULL;
}

/*** GetCurrentIcon --
 *
 *
 *
 * HICON APIENTRY GetCurrentIcon(VOID)
 *
 * ENTRY -         VOID
 *
 * EXIT  -        HICON
 *
 * SYNOPSIS -
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

HICON APIENTRY GetCurrentIcon(VOID)
{
	WCHAR szExpanded[MAX_PATH];
	HANDLE hModule;
	HANDLE h;
	PBYTE p;
	int cb;


// BUG BUG this was just added and I don't know if it's OK
	if (hDlgIcon) {
		DestroyIcon(hDlgIcon);
		hDlgIcon = NULL;
	}
	WCHAR szShortName[MAX_PATH];
	GetShortPathName(szIconPath, szExpanded, MAX_PATH);
	//lstrcpy(szExpanded, szIconPath);
	DoEnvironmentSubst(szExpanded, CharSizeOf(szExpanded));
	StripArgs(szExpanded);
	TagExtension(szExpanded, sizeof(szExpanded));
	SheRemoveQuotes(szExpanded);
	/*
	if (hModule = LoadLibrary(szExpanded)) {
		h = FindResource(hModule, (LPTSTR) MAKEINTRESOURCE(iDlgIconId), (LPTSTR) MAKEINTRESOURCE(RT_ICON));
		if (h) {
			cb = SizeofResource(hModule, h);
			h = LoadResource(hModule, h);
			p = LockResource(h);
			hDlgIcon = CreateIconFromResource(p, cb, TRUE, 0x00030000);
			UnlockResource(h);
			FreeResource(h);
		}
		FreeLibrary(hModule);
		if (hDlgIcon) {
			return(hDlgIcon);
		}
	} else {
	*/
		hDlgIcon = ExtractIcon(hAppInstance, szExpanded, iDlgIconIndex);
		if (hDlgIcon && hDlgIcon != (HANDLE)1) {
			return(hDlgIcon);
		}
	//}

	iDlgIconId = 0;
	if (hDlgIcon == NULL) {
		if (h = FindResource(hAppInstance, (LPTSTR) MAKEINTRESOURCE(ITEMICON), RT_GROUP_ICON)) {
			h = LoadResource(hAppInstance, h);
			p = LockResource(h);
			iDlgIconId = (WORD)LookupIconIdFromDirectory(p, TRUE);
			iDlgIconIndex = ITEMICONINDEX;
			UnlockResource(h);
			FreeResource(h);
		}
	} if (hDlgIcon == (HANDLE)1) {
		if (h = FindResource(hAppInstance, (LPTSTR) MAKEINTRESOURCE(DOSAPPICON), RT_GROUP_ICON)) {
			h = LoadResource(hAppInstance, h);
			p = LockResource(h);
			iDlgIconId = (WORD)LookupIconIdFromDirectory(p, TRUE);
			iDlgIconIndex = DOSAPPICONINDEX;
			UnlockResource(h);
			FreeResource(h);
		}
	}

	h = FindResource(hAppInstance, (LPTSTR) MAKEINTRESOURCE(iDlgIconId), (LPTSTR) MAKEINTRESOURCE(RT_ICON));
	if (h) {
		cb = (WORD)SizeofResource(hAppInstance, h);
		h = LoadResource(hAppInstance, h);
		if (h != NULL) {
			p = LockResource(h);
			hDlgIcon = CreateIconFromResource(p, cb, TRUE, 0x00030000);
			UnlockResource(h);
			FreeResource(h);
		}
	}
	/*
	if (hModule)
		FreeLibrary(hModule);
	*/
	return(hDlgIcon);
}


/*--------------------------------------------------------------------------*/
/*									                                        */
/*  CheckHotKeyInUse() - Return of TRUE means no-dup or user says OK anyway.*/
/*      fNewItem is TRUE if your about to create a new item, FALSE if your  */
/*      just editing an old one.                                            */
/*									                                        */
/*--------------------------------------------------------------------------*/

BOOL NEAR PASCAL CheckHotKeyInUse(WORD wHotKey, BOOL fNewItem)
{
	PGROUP pGroup;
	PITEM pItem;
	LPGROUPDEF lpgd;
	LPITEMDEF lpid;
	WCHAR szTemp1[64];
	WCHAR szTemp2[MAXMESSAGELEN+1];
	int ret;

	for (pGroup = pFirstGroup; pGroup; pGroup = pGroup->pNext) {
		for (pItem = pGroup->pItems; pItem; pItem = pItem->pNext) {
			/*
			 * If we're editing an existing item in then ignore the one
			 * we're editing (the first item in the current group).
			 */
			if (!fNewItem && pGroup == pCurrentGroup && pItem == pCurrentGroup->pItems)
				continue;

			if (wHotKey && (wHotKey == GroupFlag(pGroup,pItem,(WORD)ID_HOTKEY)))
				goto Duplicate;
		}
	}

	return TRUE;

Duplicate:
	if (!(lpgd = LockGroup(pGroup->hwnd)))
		return TRUE;      // Crash out without error.

	lpid = ITEM(lpgd, pItem->iItem);

	if (!LoadString(hAppInstance, IDS_ITEMINGROUP, szTemp1, CharSizeOf(szTemp1)))
		return TRUE;

	wsprintf(szTemp2, szTemp1, (LPTSTR) PTR(lpgd, lpid->pName),  (LPTSTR) PTR(lpgd, lpgd->pName));

	ret  = MyMessageBox(hwndProgman,
						IDS_DUPHOTKEYTTL, IDS_DUPHOTKEYMSG, szTemp2,
						MB_OKCANCEL|MB_DEFBUTTON2|MB_ICONEXCLAMATION);
	UnlockGroup(pGroup->hwnd);
	if (ret == IDOK)
		return TRUE;
	else
		return FALSE;
}

/* NOTE:
 *         'szIconPath' has the icon's path.
 *         'iDlgIconId' is set to the icon's id.
 *         'iDlgIconIndex' is set to the icon's index.
 *
 *         'szNameField' is used as a temporary variable.
 *         'szPathField' is used as a temporary variable.
 */

/*** NewItemDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY NewItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
 *
 * ENTRY -         HWND hhwnd                 - handle to dialog box.
 *                 UINT uiMsg                 - message to be acted upon.
 *                 WPARAM wParam              - value specific to uiMsg.
 *                 LPARAM lParam              - value specific to uiMsg.
 *
 * EXIT  -           True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
INT_PTR APIENTRY NewItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
static BOOL bIsWOWApp = FALSE;
	DWORD dwThreadID;
	HANDLE hThread = NULL;
	HWND  hIcon;
	DWORD dwBinaryInfo, cbData, dwDataType;
	BOOL  bDoit;



	switch (uiMsg) {
		case WM_INITDIALOG:
			if (GroupFull(pCurrentGroup)) {
				EndDialog(hwnd, FALSE);
				break;
			}

			SendDlgItemMessage(hwnd, IDD_NAME, EM_LIMITTEXT, MAXITEMNAMELEN, 0L);
			SendDlgItemMessage(hwnd, IDD_COMMAND, EM_LIMITTEXT, MAXITEMPATHLEN, 0L);
			SendDlgItemMessage(hwnd, IDD_DIR, EM_LIMITTEXT, MAXITEMPATHLEN, 0L);
			szNameField[0] = TEXT('\0');
			szPathField[0] = TEXT('\0');
			szDirField[0] = TEXT('\0');
			szIconPath[0] = TEXT('\0');
			iDlgIconId = 0;
			iDlgIconIndex = 0;
			if (hDlgIcon)
				DestroyIcon(hDlgIcon);
			hDlgIcon = NULL;
			EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDD_ICON), FALSE);
			CheckDlgButton(hwnd, IDD_NEWVDM, 1);
			EnableWindow(GetDlgItem(hwnd, IDD_NEWVDM), FALSE);
			fNewIcon = FALSE;

			//
			//  Set the inital state for the thread which checks the binary
			//  type.
			//

			//
			// Query if the binary type checking is enabled.
			//

			cbData = sizeof(dwBinaryInfo);
			if (RegQueryValueEx(hkeyPMSettings, szCheckBinaryType, 0, &dwDataType,
					(LPBYTE)&dwBinaryInfo, &cbData) == ERROR_SUCCESS) {
				bCheckBinaryType = (BOOL) dwBinaryInfo;
			} else {
				bCheckBinaryType = BINARY_TYPE_DEFAULT;
			}

			//
			// Query the binary type checking timeout value.
			//

			cbData = sizeof(dwBinaryInfo);
			if (RegQueryValueEx(hkeyPMSettings, szCheckBinaryTimeout, 0, &dwDataType,
					(LPBYTE)&dwBinaryInfo, &cbData) == ERROR_SUCCESS) {
				uiCheckBinaryTimeout = (UINT) dwBinaryInfo;
			} else {
				uiCheckBinaryTimeout = BINARY_TIMEOUT_DEFAULT;
			}

			//
			// Create the worker thread, and the signal event.  If appropriate.
			//

			if (bCheckBinaryType) {
				hCheckBinaryEvent = CreateEvent (NULL, FALSE, FALSE,
					CHECK_BINARY_EVENT_NAME);
				hThread = CreateThread (NULL, 0,
					(LPTHREAD_START_ROUTINE) CheckBinaryThread,
					(LPVOID) hwnd, 0, &dwThreadID);

				bCheckBinaryDirtyFlag = FALSE;
			}

			if (!bCheckBinaryType || !hThread || !hCheckBinaryEvent) {
				//
				// If this statement is true, either binary type checking 
				// is disabled or we failed to create the event or the thread.
				// In this case, enable the separate memory checkbox, and let
				// the user decide on his own.
				//

				CheckDlgButton(hwnd, IDD_NEWVDM, 0);
				EnableWindow(GetDlgItem(hwnd,IDD_NEWVDM), TRUE);

				//
				// Clean up either item that succeeded
				//
				if (hCheckBinaryEvent) {
					CloseHandle (hCheckBinaryEvent);
				}

				if (hThread) {
					TerminateThread (hThread, 0);
				}

				//
				// Setting this variable to NULL will prevent the second
				// thread from trying to check the binary type.
				//

				hCheckBinaryEvent = NULL;
			}
			break;

		case WM_TIMER:
			if (hCheckBinaryEvent && bCheckBinaryDirtyFlag) {
				bCheckBinaryDirtyFlag = FALSE;
				SetEvent (hCheckBinaryEvent);
			}
			break;

		case WM_COMMAND:
			switch(GET_WM_COMMAND_ID(wParam, lParam)) {
			case IDD_HELP:
				goto DoHelp;

			case IDD_COMMAND:
			{
				if (GET_WM_COMMAND_CMD(wParam, lParam) != EN_UPDATE)
					break;

				if (bCheckBinaryType) {
					bCheckBinaryDirtyFlag = TRUE;
					SetTimer (hwnd, CHECK_BINARY_ID, uiCheckBinaryTimeout, NULL);
				}
				bDoit = (GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAXITEMPATHLEN+1) > 0);
				EnableWindow(GetDlgItem(hwnd, IDOK), bDoit);
				if ( (hIcon = GetDlgItem(hwnd, IDD_ICON)) ) {
					EnableWindow(hIcon, bDoit);
				}

				break;
			}

			case IDD_BROWSE:
			{
				OPENFILENAME ofn;
				DWORD dwSave = dwContext;
				WCHAR szPathField[MAX_PATH];

				dwContext = IDH_PROPBROWSEDLG;
				GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAX_PATH);
				GetDlgItemText(hwnd, IDD_DIR, szDirField, MAX_PATH);

				// Initialize OPENFILENAME
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFile = szPathField;
				ofn.lpstrFile[0] = '\0';
				ofn.nMaxFile = sizeof(szPathField);
				ofn.lpstrFilter = TEXT("Programs\0*.exe;*.pif;*.com;*.bat;*.cmd\0All Files (*.*)\0*.*\0");
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

				if (GetOpenFileName(&ofn) == TRUE) {
					// quotes around path if it contains spaces
					CheckEscapes(szPathField, MAX_PATH);

					// give the dialog what it needs
					SetDlgItemText(hwnd, IDD_COMMAND, szPathField);
					EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);
					EnableWindow(GetDlgItem(hwnd, IDD_ICON), TRUE);

					// set the default button to OK
					PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwnd, IDOK), TRUE);
				}

#if LEGACYOPENFILEDLG
					// LEGACY OPEN FILE DIALOG
				/* Get PathField using browser dlg. */
				if (GetFileNameFromBrowse(hwnd, szPathField, sizeof(szPathField), szDirField, TEXT("exe"),
						TEXT("Programs\0*.exe;*.pif;*.com;*.bat;*.cmd\0All Files (*.*)\0*.*\0"), TEXT("Open"))) {
					// OK.
					//
					// if filename or directory have spaces, put the path
					// between quotes.
					//
					CheckEscapes(szPathField, MAX_PATH);

					SetDlgItemText(hwnd, IDD_COMMAND, szPathField);
					EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);
					EnableWindow(GetDlgItem(hwnd, IDD_ICON), TRUE);
					/* Set default button to OK. */
					PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwnd, IDOK), TRUE);
				}
#endif
				dwContext = dwSave;
				break;
			}

			case IDD_ICON:
			{
				WCHAR szTempField[MAX_PATH];

				GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAX_PATH);

				GetDlgItemText(hwnd, IDD_DIR, szDirField, MAX_PATH);

				// Expand env variables.
				DoEnvironmentSubst(szPathField, MAX_PATH)
					&& DoEnvironmentSubst(szDirField, MAX_PATH);

				// Get a full path to the icon.
				StripArgs(szPathField);
					//
					// Change to original directory in case the use entered a
					// relative path.
					//
				SetCurrentDirectory(szOriginalDirectory);
				FindExecutable(szPathField, szDirField, szTempField);
				SetCurrentDirectory(szWindowsDirectory);

				/* Discard the old Icon Path. */
				lstrcpy(szIconPath, szTempField);
				iDlgIconId = 0;
				iDlgIconIndex = 0;

				if (fNewIcon = PickIconDlg(hwnd, szIconPath, MAX_PATH, iDlgIconIndex)) {
					// Set default button to OK.
					hDlgIcon = ExtractIcon(hAppInstance, szIconPath, iDlgIconIndex);
					if (hDlgIcon) {
						iDlgIconId = GetIconIdFromIndex(szIconPath, iDlgIconIndex);
					}
					else {
						hDlgIcon = NULL;
						iDlgIconId = 0;
						iDlgIconIndex = 0;
					}
					// make sure we have a valid icon
					PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwnd, IDOK), TRUE);
					SendDlgItemMessage(hwnd, IDD_CURICON, STM_SETICON, (WPARAM)hDlgIcon, 0L);
					if (hIconGlobal) {
						DestroyIcon(hIconGlobal);
						hIconGlobal = NULL;
					}
				}
				else { /* Cancel/ESC was selected, reset Icon Path to NULL. */
					*szIconPath = TEXT('\0');
					iDlgIconId = 0;
					iDlgIconIndex = 0;
				}

				break;
			}

			case IDOK:
			{
				WORD hk;
				WCHAR szHackField[MAX_PATH];
				DWORD dwRet;
				DWORD dwFlags = CI_ACTIVATE | CI_SET_DOS_FULLSCRN;

				if(!GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAX_PATH)) {
					szPathField[0] = TEXT('\0');
					break;
				}

				//RemoveLeadingSpaces(szPathField);
				hk = (WORD) SendDlgItemMessage(hwnd, IDD_HOTKEY, WM_GETHOTKEY,0,0L);
				if (!CheckHotKeyInUse(hk, TRUE))
					break;

				GetDlgItemText(hwnd, IDD_DIR, szDirField, MAXITEMPATHLEN+1);

				//RemoveLeadingSpaces(szDirField)
					/* Expand env variables. */
				DoEnvironmentSubst(szPathField, MAXITEMPATHLEN+1);
				DoEnvironmentSubst(szDirField, MAXITEMPATHLEN+1);

				/* Now remove the arguments from the command line. */
				StripArgs(szPathField);

				dwRet = ValidatePath(hwnd, szPathField, szDirField, szHackField);
				if (dwRet == PATH_INVALID) {
					break;
				}
				else if (dwRet == PATH_INVALID_OK) {
					dwFlags |= CI_NO_ASSOCIATION;
				}

				/* Special case DOS apps. */
				HandleDosApps(szHackField);


				/* If the user hasn't supplied a description then build one. */
				if (!GetDlgItemText(hwnd, IDD_NAME, szNameField, MAX_PATH)) {
					szNameField[0] = TEXT('\0');
				}

				/* Get the original command line with arguments. */
				GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAX_PATH);

				/* Get original (unexpanded) directory. */
				if (!GetDlgItemText(hwnd, IDD_DIR, szDirField, MAX_PATH)) {
					szDirField[0] = TEXT('\0');
				}
				else {
					LPTSTR lpEnd;
					WCHAR  chT;

					// Remove trailing spaces (inside of quote if applicable)
					lpEnd = szDirField + lstrlen (szDirField) - 1;
					chT = *lpEnd;

					if ( (chT == TEXT('\"')) || (chT == TEXT(' ')) ) {
						// MarkTa fix for spaces at the end of a filename
						// Remove the spaces by moving the last character forward.
						while (*(lpEnd-1) == TEXT(' '))
							lpEnd--;

						*lpEnd = chT;
						*(lpEnd+1) = TEXT ('\0');

						// In case the character we saved was a space, we can
						// NULL terminate it because now we know the next character
						// to the left is valid, and the character to the right is
						// already NULL.

						if (*lpEnd == TEXT(' '))
							*lpEnd = TEXT('\0');
					}
				}

				GetStuffFromPIF(szPathField, szNameField, szDirField);

				if (!*szNameField) {
					BuildDescription(szNameField, szPathField);
				}

				/* If there's no default directory then add one. */
				if (!*szDirField) {
					GetDirectoryFromPath(szPathField, szDirField);
				}

				if (!InQuotes(szDirField)) {
					//
					// if szDirField needs quotes and the work dir is too
					// long than we need to truncate it.
					//
					if (lstrlen(szDirField) >= MAXITEMPATHLEN-2) {
						WCHAR chT;

						chT = szDirField[MAXITEMPATHLEN-2];
						szDirField[MAXITEMPATHLEN-2] = 0;
						CheckEscapes(szDirField, MAXITEMPATHLEN+1);
						if (*szDirField != TEXT('"')) {
							szDirField[MAXITEMPATHLEN-2] = chT;
						}
					}
					else {
						CheckEscapes(szDirField, MAXITEMPATHLEN+1);
					}
				}

				if (IsWindowEnabled(GetDlgItem(hwnd, IDD_NEWVDM)) &&
						IsDlgButtonChecked(hwnd, IDD_NEWVDM)) {
					dwFlags |= CI_SEPARATE_VDM;
				}

				/* Create new item using UNICODE strings. */
				CreateNewItem(pCurrentGroup->hwnd,
					szNameField, szPathField,
					szIconPath, szDirField,
					hk, (BOOL)IsDlgButtonChecked(hwnd, IDD_LOAD),
					(WORD)iDlgIconId, iDlgIconIndex,
					hDlgIcon, NULL, dwFlags);
				}


			  // Update scroll bars for new item
				if ((bAutoArrange) && (!bAutoArranging))
					ArrangeItems(pCurrentGroup->hwnd);
				else if (!bAutoArranging)
					CalcGroupScrolls(pCurrentGroup->hwnd);

				// fall through...

			case IDCANCEL:

				if (bCheckBinaryType) {
					//
					// Setting this variable to false, and signaling the event
					// will cause the binary checking thread to terminate.
					//

					bCheckBinaryType = FALSE;
					SetEvent (hCheckBinaryEvent);
					KillTimer (hwnd, CHECK_BINARY_ID);
				}

				EndDialog(hwnd, GET_WM_COMMAND_ID(wParam, lParam) == IDOK);
				GetRidOfIcon();
				break;

			default:
				return(FALSE);
			}
	break;

	default:

		if (uiMsg == uiHelpMessage || uiMsg == uiBrowseMessage) {
DoHelp:
			PMHelp(hwnd);

			return TRUE;
		} else
			return FALSE;
	}
	UNREFERENCED_PARAMETER(lParam);
	return(TRUE);
}

/*** NewGroupDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY NewGroupDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
 *
 * ENTRY -         HWND hhwnd                 - handle to dialog box.
 *                        UINT uiMsg                  - message to be acted upon.
 *                        DWORD wParam        - value specific to uiMsg.
 *                        LPARAM lParam       - value specific to uiMsg.
 *
 * EXIT  -           True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

INT_PTR APIENTRY NewGroupDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bDoit;
	HWND hwndGroup;

	switch (uiMsg) {

	case WM_INITDIALOG:
		if (wNewSelection == TYPE_COMMONGROUP) {
			WCHAR szCommonGroupTitle[64];

			if (LoadString(hAppInstance, IDS_COMMONGROUPPROP,
						   szCommonGroupTitle, CharSizeOf(szCommonGroupTitle))) {
				SetWindowText(hwnd, szCommonGroupTitle);
			}
		}
		SendDlgItemMessage(hwnd, IDD_NAME, EM_LIMITTEXT, MAXGROUPNAMELEN, 0L);
		break;

	case WM_COMMAND:
		switch(GET_WM_COMMAND_ID(wParam, lParam)) {
		HCURSOR hCursor;

		case IDD_NAME:
		case IDD_PATH:
			/* Allow OK if either of the two fields have anything in them. */
			bDoit = (GetDlgItemText(hwnd, IDD_NAME, szPathField, CharSizeOf(szPathField)) > 0);
			EnableWindow(GetDlgItem(hwnd, IDOK), bDoit);
			break;

		case IDD_HELP:
			goto DoHelp;

		case IDOK:
			if(!GetDlgItemText(hwnd, IDD_NAME, szNameField, MAXITEMNAMELEN + 1)) {
			   szNameField[0] = TEXT('\0');
			   break;
			}

			hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
			ShowCursor(TRUE);

			  hwndGroup = CreateNewGroup(szNameField, (wNewSelection == TYPE_COMMONGROUP));

			ShowCursor(FALSE);
			SetCursor(hCursor);
			if (hwndGroup) {
				EndDialog(hwnd, TRUE);
			}
			break;

		case IDCANCEL:
			EndDialog(hwnd, FALSE);
			break;

		default:
			return FALSE;
		}
		break;

	default:
		if (uiMsg == uiHelpMessage) {
DoHelp:
			PMHelp(hwnd);

			return TRUE;
		} else {
			return FALSE;
		}
	}

	return TRUE;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  EditBrowseOK() -                                                        */
/*                                                                          */
/*--------------------------------------------------------------------------*/
BOOL NEAR PASCAL EditBrowseOK(HWND hDlg)
{
	DWORD dwSave = dwContext;
	WCHAR szPathField[MAX_PATH];
	BOOL ret;

	dwContext = IDH_PROPBROWSEDLG;
	GetDlgItemText(hDlg, IDD_COMMAND, szPathField, MAX_PATH);
	GetDlgItemText(hDlg, IDD_DIR, szDirField, MAX_PATH);

	/* Get PathField using browser dlg. */
	if (GetFileNameFromBrowse(hDlg, szPathField, sizeof(szPathField), szDirField, TEXT("exe"),
			TEXT("Programs\0*.exe;*.pif;*.com;*.bat;*.cmd\0All Files (*.*)\0*.*\0"), TEXT("Open"))) {
		/* OK. */
		//
		// if filename or directory have spaces, put the path
		// between quotes.
		//
		CheckEscapes(szPathField, MAX_PATH);

		SetDlgItemText(hDlg, IDD_COMMAND, szPathField);
		EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDD_ICON), TRUE);
		/* Set default button to OK. */
		PostMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDOK), TRUE);
		ret = TRUE;
	}
	else
		ret = FALSE;
	dwContext = dwSave;
	return ret;
}

/*** EditItemDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY EditItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
 *
 * ENTRY -         HWND hhwnd                 - handle to dialog box.
 *                        UINT uiMsg                  - message to be acted upon.
 *                 WPARAM wParam              - value specific to uiMsg.
 *                 LPARAM lParam              - value specific to uiMsg.
 *
 * EXIT  -           True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */

/* NOTE:
 *         'szIconPath' has the icon's path.
 *         'iDlgIconId' is set to the icon's id.
 *         'iDlgIconIndex' is set to the icon's index.
 *
 *         'szNameField' is used as a temporary variable.
 *         'szPathField' is used as a temporary variable.
 */
INT_PTR APIENTRY EditItemDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	LPITEMDEF	lpid;
	LPGROUPDEF	lpgd;
	WCHAR	szTempField[MAX_PATH];
	DWORD	dwFlags = CI_ACTIVATE;
	DWORD	dwBinaryType;
	WCHAR	szFullPath[MAX_PATH];
	LPTSTR	FilePart;
	static	BOOL bIsWOWApp = FALSE;
	static	WORD wHotKey;
	static	WCHAR szDescription[MAX_PATH];
	DWORD	dwThreadID;
	HANDLE	hThread = NULL;
	HWND	hIcon;
	DWORD	dwBinaryInfo, cbData, dwDataType;
	BOOL	bDoit;


	switch (uiMsg) {

	case WM_INITDIALOG:
		/*
		 * Get current item information.
		 */
		pActiveGroup = pCurrentGroup;
		lpgd = (LPGROUPDEF)GlobalLock(pActiveGroup->hGroup);
		lpid = LockItem(pActiveGroup,pActiveGroup->pItems);
		if (lpid == 0L)
			goto EditDlgExit;
		fNewIcon = FALSE;

		SendDlgItemMessage(hwnd, IDD_NAME, EM_LIMITTEXT, MAXITEMNAMELEN, 0L);
		SetDlgItemText(hwnd, IDD_NAME, (LPTSTR) PTR(lpgd, lpid->pName));
		lstrcpy(szDescription, (LPTSTR) PTR(lpgd, lpid->pName));

		GetItemCommand(pActiveGroup, pActiveGroup->pItems, szPathField, szDirField);

		/* Keep a copy of what the old item was refering to.
		 * A full expanded OEM path to the executable
		 */
		DoEnvironmentSubst(szPathField, MAXITEMPATHLEN+1);
		DoEnvironmentSubst(szDirField, MAXITEMPATHLEN+1);
		StripArgs(szPathField);
		// Find exe will toast on ansi strings.
		//
		// Change to original Directory in case the user entered a relative path
		//
		//
		SetCurrentDirectory(szOriginalDirectory);
		FindExecutable(szPathField, szDirField, szNameField);
		SetCurrentDirectory(szWindowsDirectory);

		lstrcpy(szIconPath, (LPTSTR) PTR(lpgd, lpid->pIconPath));
		if (!*szIconPath) {
			/* use default icon path */
			lstrcpy(szIconPath, szNameField);
		}
		iDlgIconId = lpid->iIcon;
		iDlgIconIndex = lpid->wIconIndex;

		TagExtension(szPathField, sizeof(szPathField));
		SheRemoveQuotes (szPathField);
		if (SearchPath(NULL,
						szPathField,
						NULL,
						MAXITEMPATHLEN+1,
						szFullPath,
						&FilePart) &&
			GetBinaryType(szFullPath, &dwBinaryType) &&
								  dwBinaryType == SCS_WOW_BINARY) {
				bIsWOWApp = TRUE;
				if (GroupFlag(pActiveGroup,pActiveGroup->pItems,(WORD)ID_NEWVDM)) {
					CheckDlgButton(hwnd, IDD_NEWVDM, 1);
				}
		}
		else {
			CheckDlgButton(hwnd, IDD_NEWVDM, 1);
			EnableWindow(GetDlgItem(hwnd, IDD_NEWVDM), FALSE);
		}

		/* Re-get the fields so that the dlg is initialized with
		 * the environment variables intact.
		 */
		GetItemCommand(pActiveGroup,pActiveGroup->pItems,szPathField,szDirField);

		SendDlgItemMessage(hwnd, IDD_COMMAND, EM_LIMITTEXT, MAXITEMPATHLEN, 0L);
		SetDlgItemText(hwnd, IDD_COMMAND, szPathField);

		SendDlgItemMessage(hwnd, IDD_DIR, EM_LIMITTEXT, MAXITEMPATHLEN, 0L);
		SetDlgItemText(hwnd, IDD_DIR, szDirField);

		if (GroupFlag(pActiveGroup,pActiveGroup->pItems,(WORD)ID_MINIMIZE))
			CheckDlgButton(hwnd, IDD_LOAD, TRUE);

		SendDlgItemMessage(hwnd,IDD_HOTKEY,WM_SETHOTKEY,
						   (WPARAM)GroupFlag(pActiveGroup,pActiveGroup->pItems,(WORD)ID_HOTKEY),
						   0L);

		GetCurrentIcon();        // use szIconPath from above
		GlobalUnlock(pActiveGroup->hGroup);

		UnlockGroup(pActiveGroup->hwnd);
		SendDlgItemMessage(hwnd, IDD_CURICON, STM_SETICON, (WPARAM)hDlgIcon, 0L);

		if (pActiveGroup->fRO || dwEditLevel >= 4) {
			/* if readonly, we can only view... */
			EnableWindow(GetDlgItem(hwnd,IDOK), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_ICON), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_HOTKEY), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_NAME), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_LOAD), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_NEWVDM), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_COMMAND), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_BROWSE), FALSE);
		}
		else if (dwEditLevel == 3) {
			EnableWindow(GetDlgItem(hwnd,IDD_COMMAND), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_BROWSE), FALSE);
		}
		//
		//  Set the inital state for the thread which checks the binary
		//  type.
		//

		//
		// Query if the binary type checking is enabled.
		//

		cbData = sizeof(dwBinaryInfo);
		if (RegQueryValueEx(hkeyPMSettings, szCheckBinaryType, 0, &dwDataType,
					 (LPBYTE)&dwBinaryInfo, &cbData) == ERROR_SUCCESS) {
			bCheckBinaryType = (BOOL) dwBinaryInfo;
		} else {
			bCheckBinaryType = BINARY_TYPE_DEFAULT;
		}

		//
		// Query the binary type checking timeout value.
		//

		cbData = sizeof(dwBinaryInfo);
		if (RegQueryValueEx(hkeyPMSettings, szCheckBinaryTimeout, 0, &dwDataType,
					 (LPBYTE)&dwBinaryInfo, &cbData) == ERROR_SUCCESS) {
			uiCheckBinaryTimeout = (UINT) dwBinaryInfo;
		} else {
			uiCheckBinaryTimeout = BINARY_TIMEOUT_DEFAULT;
		}

		//
		// Create the worker thread, and the signal event.  If appropriate.
		//

		if (bCheckBinaryType) {
			hCheckBinaryEvent = CreateEvent (NULL, FALSE, FALSE,
											 CHECK_BINARY_EVENT_NAME);

			hThread = CreateThread (NULL, 0,
								   (LPTHREAD_START_ROUTINE) CheckBinaryThread,
								   (LPVOID) hwnd, 0, &dwThreadID);

			bCheckBinaryDirtyFlag = FALSE;
		}

		if (!hCheckBinaryEvent || !hThread || !bCheckBinaryType) {
			//
			// If this statement is true, either we failed to create
			// the event, the thread, or binary type checking is disabled.
			// In this case, enable the separate memory checkbox, and let
			// the user decide on his own.
			//

			CheckDlgButton(hwnd, IDD_NEWVDM, 0);
			EnableWindow(GetDlgItem(hwnd,IDD_NEWVDM), TRUE);

			//
			// Clean up either item that succeeded
			//
			if (hCheckBinaryEvent) {
				CloseHandle (hCheckBinaryEvent);
			}

			if (hThread) {
				TerminateThread (hThread, 0);
			}

			//
			// Setting this variable to NULL will prevent the second
			// thread from trying to check the binary type.
			//

			hCheckBinaryEvent = NULL;
		}

		break;

	case WM_TIMER:
		if (hCheckBinaryEvent && bCheckBinaryDirtyFlag) {
			bCheckBinaryDirtyFlag = FALSE;
			SetEvent (hCheckBinaryEvent);
		}
		break;


	case WM_COMMAND:
	{
		switch(GET_WM_COMMAND_ID(wParam, lParam)) {
		case IDD_HELP:
			goto DoHelp;

		case IDD_COMMAND:
		{
			if (GET_WM_COMMAND_CMD(wParam, lParam) != EN_UPDATE)
				break;

			if (bCheckBinaryType) {
				bCheckBinaryDirtyFlag = TRUE;
				SetTimer (hwnd, CHECK_BINARY_ID, uiCheckBinaryTimeout, NULL);
			}
			bDoit = (GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAXITEMPATHLEN+1) > 0);
			EnableWindow(GetDlgItem(hwnd, IDOK), bDoit);
			if ( (hIcon = GetDlgItem(hwnd, IDD_ICON)) ) {
				EnableWindow(hIcon, bDoit);
			}

			break;
		}

		case IDD_BROWSE:
			EditBrowseOK(hwnd);
			break;

		case IDD_ICON:
		{
			LPITEMDEF lpid;
			WCHAR szShortPath[MAX_PATH];
			if (!GetDlgItemText(hwnd, IDD_COMMAND, szShortPath, MAXITEMPATHLEN+1))
				break;
			GetShortPathName(szShortPath, szPathField, MAX_PATH);
//#ifdef JAPAN
//            if (CheckPortName(hwnd,szPathField))
//                break;
//#endif

			GetDlgItemText(hwnd, IDD_DIR, szShortPath, MAXITEMPATHLEN+1);
			GetShortPathName(szShortPath, szDirField, MAX_PATH);
			/* Expand env variables. */
			DoEnvironmentSubst(szPathField, MAXITEMPATHLEN+1)
			  && DoEnvironmentSubst(szDirField, MAXITEMPATHLEN+1);

			/* Get a full path to the icon. */
			StripArgs(szPathField);
			SetCurrentDirectory(szOriginalDirectory);
			FindExecutable(szPathField, szDirField, szShortPath);
			GetShortPathName(szShortPath, szTempField, MAX_PATH);
			SetCurrentDirectory(szWindowsDirectory);


			/*
			 * If the icon path hasn't been explicitly set then
			 * use a default one.
			 */
			if (!fNewIcon) {
				/* Has the items path been changed? */
				if (lstrcmpi(szNameField, szTempField)) {
					/* Yup, it's changed, discard the old Icon Path. */
					lstrcpy(szIconPath, szTempField);
					iDlgIconId = 0;
					iDlgIconIndex = 0;
				}
				else {
					/* The path hasn't changed so use the old icon path. */
					lpid = LockItem(pActiveGroup,pActiveGroup->pItems);
					   // BUG BUG! should have LockItem unlock the group
					   // before returning. JohanneC 7/5/91
					UnlockGroup(pActiveGroup->hwnd);
					if (lpid == 0L)
						goto EditDlgExit;
					lpgd = LockGroup(pActiveGroup->hwnd);
					lstrcpy(szIconPath, (LPTSTR) PTR(lpgd, lpid->pIconPath));
					UnlockGroup(pActiveGroup->hwnd);
				}
			}

			/* Check if we have a default icon. */
			if (!*szIconPath) {
				/* Invalid path, use the executable's path. */
				lstrcpy(szIconPath, szTempField);
			}

			if (fNewIcon = PickIconDlg(hwnd, szIconPath, MAX_PATH, iDlgIconIndex)) {
				// Set default button to OK.
				hDlgIcon = ExtractIcon(hAppInstance, &szIconPath, iDlgIconIndex);
				if (hDlgIcon) {
					iDlgIconId = GetIconIdFromIndex(szIconPath, iDlgIconIndex);
				}
				else {
					hDlgIcon = NULL;
					iDlgIconId = 0;
					iDlgIconIndex = 0;
				}
				// make sure we have a valid icon
				PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwnd, IDOK), TRUE);
				SendDlgItemMessage(hwnd, IDD_CURICON, STM_SETICON, (WPARAM)hDlgIcon, 0L);
				if (hIconGlobal) {
					DestroyIcon(hIconGlobal);
					hIconGlobal = NULL;
				}
			}
			else {
				/* They hit cancel, get the old path back. */
				lpid = LockItem(pActiveGroup, pActiveGroup->pItems);
				// BUG BUG! should have LockItem unlock the group
				// before returning. JohanneC 7/5/91
				UnlockGroup(pActiveGroup->hwnd);
				lpgd = LockGroup(pActiveGroup->hwnd);
				lstrcpy(szIconPath, (LPTSTR) PTR(lpgd, lpid->pIconPath));
				UnlockGroup(pActiveGroup->hwnd);
			}
			break;
		}

		case IDOK:
		{
			RECT rc;
			WORD hk;
			BOOL bARTmp; //AutoArrangeFlag
			BOOL fNewItem = FALSE;
			DWORD dwRet;

			hk = (WORD) SendDlgItemMessage(hwnd,IDD_HOTKEY, WM_GETHOTKEY,0,0L);
			if (!CheckHotKeyInUse(hk, FALSE))
				break;

			/* Get the non-parm part of the new command line. */
			GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAXITEMPATHLEN+1);
			GetDlgItemText(hwnd, IDD_DIR, szDirField, MAXITEMPATHLEN+1);

			/* Expand env variables. */
			DoEnvironmentSubst(szPathField, MAXITEMPATHLEN+1)
				  && DoEnvironmentSubst(szDirField, MAXITEMPATHLEN+1);

			/* Remove arguments before validating path. */
			StripArgs(szPathField);

			dwRet = ValidatePath(hwnd, szPathField, szDirField, szTempField);
			if (dwRet == PATH_INVALID) {
				break;
			}
			else if (dwRet == PATH_INVALID_OK) {
				dwFlags |= CI_NO_ASSOCIATION;
			}


			if (bCheckBinaryType) {
				//
				// Setting this variable to false, and signaling the event
				// will cause the binary checking thread to terminate.
				//

				bCheckBinaryType = FALSE;
				SetEvent (hCheckBinaryEvent);
				KillTimer (hwnd, CHECK_BINARY_ID);
			}

			EndDialog(hwnd, TRUE);

			/*
			 * If the item refers to a different exe then we need
			 * to update a few things.
			 */
			if (lstrcmpi(szNameField, szTempField)) {
				/* Things have changed. */
				if (!fNewIcon) {
					/*
					 * The icon hasn't been expicitely set.
					 * So use the new exe path for
					 * the icon.
					 */
					// lstrcpy(szIconPath, szPathField);
					szIconPath[0] = TEXT('\0'); /* reset icon path */
					iDlgIconId = 0;
					iDlgIconIndex = 0;
				}
				fNewItem = TRUE;
				/* Check if the new thing is a DOS app. */
				HandleDosApps(szTempField);
			}
			else {
				/* test if icon has changed. If not, reset iconpath.*/
				if (!fNewIcon) {
					lpgd = LockGroup(pActiveGroup->hwnd);
					lpid = LockItem(pActiveGroup,pActiveGroup->pItems);
					lstrcpy(szIconPath, (LPTSTR) PTR(lpgd, lpid->pIconPath));
					   // LockItem locks the group, must unlock
					   // after returning. JohanneC
					UnlockGroup(pActiveGroup->hwnd);
					UnlockGroup(pActiveGroup->hwnd);
				}
			}

			CopyRect(&rc, &pActiveGroup->pItems->rcIcon);

			/* Re-get the new command line. */
			GetDlgItemText(hwnd, IDD_COMMAND, szPathField, MAXITEMPATHLEN+1);

			/* Use unexpanded path for the item info. */
			if (!GetDlgItemText(hwnd, IDD_DIR, szDirField, MAXITEMPATHLEN+1)) {
				szDirField[0] = TEXT('\0');
			} else {
				LPTSTR lpEnd;
				WCHAR  chT;

				// Remove trailing spaces (inside of quote if applicable)
				lpEnd = szDirField + lstrlen (szDirField) - 1;
				chT = *lpEnd;

				if ( (chT == TEXT('\"')) || (chT == TEXT(' ')) ) {
				   // MarkTa fix for spaces at the end of a filename
				   // Remove the spaces by moving the last character forward.
				   while (*(lpEnd-1) == TEXT(' '))
					   lpEnd--;

				   *lpEnd = chT;
				   *(lpEnd+1) = TEXT ('\0');

				   // In case the character we saved was a space, we can
				   // NULL terminate it because now we know the next character
				   // to the left is valid, and the character to the right is
				   // already NULL.

				   if (*lpEnd == TEXT(' '))
					 *lpEnd = TEXT('\0');
				}
			}

			if (!InQuotes(szDirField)) {

					//
					// if szDirField needs quotes and the work dir is too
					// long than we need to truncate it.
					//
					if (lstrlen(szDirField) >= MAX_PATH - 2) {
						WCHAR chT;

						chT = szDirField[MAX_PATH - 2];
						szDirField[MAX_PATH - 2] = 0;
						CheckEscapes(szDirField, MAX_PATH);
						if (*szDirField != TEXT('"')) {
							szDirField[MAX_PATH - 2] = chT;
						}
					}
					else {
						CheckEscapes(szDirField, MAX_PATH);
					}
			}

			/* Check if the Desc line has been filled. */
			if (!GetDlgItemText(hwnd, IDD_NAME, szNameField, MAXITEMNAMELEN+1)) {
				BuildDescription(szNameField, szPathField);
				SetDlgItemText(hwnd, IDD_NAME, szNameField);
			}

			/*
			 * Stop ARing for a mo because we're going to do a
			 * delete and the an add and there's no point in doing
			 * the auto arrange stuff twice.
			 */
			bARTmp = bAutoArrange;
			bAutoArrange = FALSE;
			DeleteItem(pActiveGroup,pActiveGroup->pItems);
			bAutoArrange = bARTmp;
			if (fNewItem || lstrcmpi(szDescription, szNameField)) {
				dwFlags |= CI_SET_DOS_FULLSCRN;
			}
			if (IsWindowEnabled(GetDlgItem(hwnd, IDD_NEWVDM)) &&
								IsDlgButtonChecked(hwnd, IDD_NEWVDM)) {
				dwFlags |= CI_SEPARATE_VDM;
			}

			CreateNewItem(pActiveGroup->hwnd,
						  szNameField,
						  szPathField,
						  szIconPath,
						  szDirField,
						  hk,
						  (BOOL)IsDlgButtonChecked(hwnd, IDD_LOAD),
						  (WORD)iDlgIconId,
						  (WORD)iDlgIconIndex,
						  hDlgIcon,
						  (LPPOINT)&rc.left,
						  dwFlags);

			GetRidOfIcon();
			pActiveGroup = NULL;
			break;
		}

		case IDCANCEL:
EditDlgExit:
			if (bCheckBinaryType) {
				//
				// Setting this variable to false, and signaling the event
				// will cause the binary checking thread to terminate.
				//

				bCheckBinaryType = FALSE;
				SetEvent (hCheckBinaryEvent);
				KillTimer (hwnd, CHECK_BINARY_ID);
			}

			pActiveGroup = NULL;
			EndDialog(hwnd, FALSE);
			GetRidOfIcon();
			break;

		default:
			return(FALSE);
		}
		break;
	}

	default:

		if (uiMsg == uiHelpMessage || uiMsg == uiBrowseMessage) {
DoHelp:
			PMHelp(hwnd);
			return TRUE;
		} else
			return FALSE;
	}
	UNREFERENCED_PARAMETER(lParam);
	return(TRUE);
}


/*** EditGroupDlgProc --         Dialog Procedure
 *
 *
 *
 * INT_PTR APIENTRY EditGroupDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
 *
 * ENTRY -  HWND  hwnd          - handle to dialog box.
 *          UINT uiMsg           - message to be acted upon.
 *          WPARAM wParam       - value specific to uiMsg.
 *          LPARAM lParam       - value specific to uiMsg.
 *
 * EXIT  -  True if success, False if not.
 * SYNOPSIS -  Dialog box message processing function.
 *
 * WARNINGS -
 * EFFECTS  -
 *
 */
INT_PTR APIENTRY EditGroupDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	LPGROUPDEF lpgd;
	static WCHAR     szGroupName[MAXGROUPNAMELEN + 1];

	switch (uiMsg) {
	case WM_INITDIALOG:
		pActiveGroup = pCurrentGroup;

		lpgd = LockGroup(pActiveGroup->hwnd);

		if (lpgd == 0L) {
			EndDialog(hwnd, FALSE);
			break;
		}

		if (pActiveGroup->fCommon) {
			WCHAR szCommonGroupTitle[64];

			if (LoadString(hAppInstance, IDS_COMMONGROUPPROP,
						   szCommonGroupTitle, CharSizeOf(szCommonGroupTitle))) {
				SetWindowText(hwnd, szCommonGroupTitle);
			}
		}

		lstrcpy(szGroupName, (LPTSTR) PTR(lpgd, lpgd->pName));
		SetDlgItemText(hwnd, IDD_NAME, (LPTSTR) PTR(lpgd, lpgd->pName));
		UnlockGroup(pActiveGroup->hwnd);

		SendDlgItemMessage(hwnd, IDD_NAME, EM_LIMITTEXT, MAXGROUPNAMELEN, 0L);

		if (pActiveGroup->fRO || dwEditLevel >= 1) {
			EnableWindow(GetDlgItem(hwnd,IDOK), FALSE);
			EnableWindow(GetDlgItem(hwnd,IDD_NAME), FALSE);
		}
		break;

	case WM_COMMAND:
		switch(GET_WM_COMMAND_ID(wParam, lParam)) {

		case IDD_HELP:
			goto DoHelp;

		case IDOK:
		{
			INT err = 0;
#if 0
			HKEY hkey;
			HKEY hkeyGroups;
			WCHAR szKeyName[MAXGROUPNAMELEN + 1];
			PSECURITY_ATTRIBUTES pSecAttr;

			if (!hkeyProgramGroups || !hkeyPMGroups) {
				err = TRUE;
				goto Exit;
			}
#endif

			GetDlgItemText(hwnd, IDD_NAME, szNameField, MAXITEMNAMELEN+1);
			/* maybe should strip leading and trailing blanks? */
		//
		// If all spaces or null string do not change it, but if WIn3.1 does.
		//

			if (lstrcmp(szGroupName, szNameField)) {

				ChangeGroupTitle(pActiveGroup->hwnd, szNameField, pActiveGroup->fCommon);
			}
#if 0
				//
				// stop handling of Program Groups key changes.
				//
				bHandleProgramGroupsEvent = FALSE;
				//
				// Determine if we're about to change the name of Personal
				// group and a Common group.
				//
				if (pActiveGroup->fCommon) {
					hkeyGroups = hkeyCommonGroups;
					pSecAttr = pAdminSecAttr;
				}
				else {
					hkeyGroups = hkeyProgramGroups;
					pSecAttr = pSecurityAttributes;
				}

				//
				// If the group name contains backslashes (\) remove them
				// from the key name. Can not have key names with backslash
				// in the registry.
				//
				lstrcpy(szKeyName, szNameField);
				RemoveBackslashFromKeyName(szKeyName);
				/* create a new key, and delete the old key */
				if (err = RegCreateKeyEx(hkeyGroups, szKeyName, 0, 0, 0,
						 DELETE | KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_NOTIFY,
						 pSecAttr, &hkey, NULL)) {
					goto Exit;
				}


				lpgd = (LPGROUPDEF)GlobalLock(pActiveGroup->hGroup);
				if (err = RegSetValueEx(hkey, NULL, 0, REG_BINARY, (LPTSTR)lpgd,
													  SizeofGroup(lpgd))) {
					RegCloseKey(hkey);
					goto Exit;
				}

				RegDeleteKey(hkeyGroups, pActiveGroup->lpKey);

				//
				// Now that it all worked, do the actually name change:
				// Change the group window title and the group key name.
				//
				ChangeGroupTitle(pActiveGroup->hwnd, szNameField, pActiveGroup->fCommon);

				LocalFree((HANDLE)pActiveGroup->lpKey);
				pActiveGroup->lpKey = (LPTSTR)LocalAlloc(LPTR, sizeof(WCHAR)*(lstrlen(szKeyName) + 1));
				lstrcpy(pActiveGroup->lpKey, szKeyName);

				if (bSaveSettings) {
					RegFlushKey(hkey);
				}
				RegCloseKey(hkey);

				if (!pActiveGroup->fCommon) {
					WriteGroupsSection();
				}
			}
Exit:
			//
			// reset handling of Program Groups key changes.
			//
			ResetProgramGroupsEvent(pActiveGroup->fCommon);
			bHandleProgramGroupsEvent = TRUE;
			if (err) {
				MyMessageBox(hwnd,IDS_CANTRENAMETITLE,
							IDS_CANTRENAMEMSG,NULL, MB_OK|MB_ICONEXCLAMATION);
			}
			GlobalUnlock(pActiveGroup->hGroup);
#endif
			pActiveGroup = NULL;

			EndDialog(hwnd, TRUE);
			break;
		}

		case IDCANCEL:
			pActiveGroup = NULL;
			EndDialog(hwnd, FALSE);
			break;

		default:
			return(FALSE);
		}
		break;

	default:

		if (uiMsg == uiHelpMessage) {
DoHelp:
			PMHelp(hwnd);

			return TRUE;
		} else
			return FALSE;
	}
	UNREFERENCED_PARAMETER(lParam);
	return(TRUE);
}

/******************************************************************************

   UpdateGroupsDlgProc

   10-18-93  Created by Johannec

******************************************************************************/
INT_PTR APIENTRY
UpdateGroupsDlgProc(
	HWND    hDlg,
	UINT    message,
	WPARAM  wParam,
	LPARAM  lParam
	)
{
	switch (message) {

	case WM_INITDIALOG:
		//
		// Position ourselves
		//
		CentreWindow(hDlg);

		return(TRUE);

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:

			 // fall through...

		case IDCANCEL:
			 EndDialog(hDlg, LOWORD(wParam) == IDOK);
			 break;
		case IDD_HELP:
			 goto DoHelp;
			 break;
		}
		break;

	default:

		if (message == uiHelpMessage)
DoHelp:
			{
			DWORD dwSave = dwContext;

			dwContext = IDH_UPDATEGRPDLG;

			PMHelp(hDlg);

			dwContext = dwSave;
			return TRUE;
		} else
			return FALSE;
	}

	// We didn't process the message
	return(FALSE);
}

/***************************************************************************\
* CentreWindow
*
* Purpose : Positions a window so that it is centred in its parent
*
* !!! WARNING this code is duplicated in winlogon\winutil.c
*
* History:
* 12-09-91 Davidc       Created.
\***************************************************************************/
VOID
CentreWindow(
	HWND    hwnd
)
{
	RECT    rect;
	RECT    rectParent;
	HWND    hwndParent;
	LONG    dx, dy;
	LONG    dxParent, dyParent;
	LONG    Style;

	// Get window rect
	GetWindowRect(hwnd, &rect);

	dx = rect.right - rect.left;
	dy = rect.bottom - rect.top;

	// Get parent rect
	Style = GetWindowLongPtr(hwnd, GWL_STYLE);
	if ((Style & WS_CHILD) == 0) {

		// Return the desktop windows size (size of main screen)
		dxParent = GetSystemMetrics(SM_CXSCREEN);
		dyParent = GetSystemMetrics(SM_CYSCREEN);
	}
	else {
		hwndParent = GetParent(hwnd);
		if (hwndParent == NULL) {
			hwndParent = GetDesktopWindow();
		}
		GetWindowRect(hwndParent, &rectParent);

		dxParent = rectParent.right - rectParent.left;
		dyParent = rectParent.bottom - rectParent.top;
	}

	// Centre the child in the parent
	rect.left = (dxParent - dx) / 2;
	rect.top = (dyParent - dy) / 3;

	// Move the child into position
	SetWindowPos(hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	SetForegroundWindow(hwnd);
}
