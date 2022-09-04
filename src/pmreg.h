/* * * * * * * *\
	PMREG.H -
		Written by Microsoft, 1992
	DESCRIPTION -
		This file contains the registry keys for Program Manager.
		NO DEPENDENCIES!
\* * * * * * * */

// Definitions
#define MAXKEYLEN      100

// Keys
extern WCHAR           szAnsiProgramGroups[MAXKEYLEN + 1];
extern HKEY            hkeyProgramManager;
extern HKEY            hkeyPMSettings;
extern HKEY            hkeyPMRestrict;
extern HKEY            hkeyPMGroups;
extern HKEY            hkeyPMCommonGroups;
extern HKEY            hkeyPMAnsiGroups;
extern HKEY            hkeyAnsiProgramGroups;
extern HKEY            hkeyProgramGroups;
extern HKEY            hkeyCommonGroups;
