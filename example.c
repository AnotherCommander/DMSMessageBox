// clang -c DMSMessageBox.c -I. -o DMSMessageBox.o
// clang -o DMSMessageBoxExample.exe example.c DMSMessageBox.o -I. -ldwmapi -lcomctl32 -mwindows

#include <windows.h>
#include <stdio.h>
#include "DMSMessageBox.h"

#define TABS1	"\t"
#define TABS2	"\t"
#define TABS3	"\t"
#define TABS4	""
#define OOLITE_WINDOWS 1
#define OO_VERSION_FULL "1.92.1.1234-260407-abcdef1"
#define STRINGIFY(a) str(a)
#define str(a) #a
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int argc)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	char processname[] = "oolite";
	char s[2048];
	snprintf(s, sizeof(s), "Usage: %s [options]\n\n"
				"Options can be any of the following: \n\n"
				"--compile-sysdesc"TABS2"Compile system descriptions *\n"
				"--export-sysdesc"TABS2"Export system descriptions *\n"
#if OOLITE_WINDOWS
				"-hdr"TABS3"Start up in HDR mode\n"
#endif
				"-load [filepath]"TABS2"Load commander from [filepath]\n"TABS3 TABS4"(\"-load\" is optional)\n"
				"-message [messageString]"TABS1"Display [messageString] at startup\n"
				"-nodust     "TABS2 TABS4"Do not draw space dust\n"
				"-noshaders"TABS2 TABS4"Start up with shaders disabled\n"
				"-nosplash    "TABS2 TABS4"Force disable splash screen on startup\n"
				"-nosound     "TABS2 TABS4"Start up with sound disabled\n"
				"-novsync"TABS3"Force disable V-Sync\n"
				"--openstep"TABS2 TABS4"When compiling or exporting\n"TABS3 TABS4"system descriptions, use openstep\n"TABS3 TABS4"format *\n"
				"-showversion"TABS2 TABS4"Display version at startup screen\n"
				"-splash"TABS3 TABS4"Force splash screen on startup\n"
				"-verify-oxp [filepath]    "TABS1"Verify OXP at [filepath] *\n"
				"--xml"TABS3 TABS4"When compiling or exporting\n"TABS3 TABS4"system descriptions, use xml\n"TABS3 TABS4"format *\n"
				"\n"
				"Options marked with \"*\" are available only in Test Release configuration.\n"
				"Version "OO_VERSION_FULL"\n"
				"Debug functionality enabled (Test Release): "
#ifndef NDEBUG
					"yes\n"
#else
					"no\n"
#endif
				"Built with "
#if __clang_major__
				"Clang version " STRINGIFY(__clang_major__) "." STRINGIFY(__clang_minor__) "." STRINGIFY(__clang_patchlevel__)
#else
				"GCC version " STRINGIFY(__GNUC__) "." STRINGIFY(__GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__)
#endif	
				"\n\n", processname);
	
//#define DOMSGBOX(x1, x2, x3, x4)	do { \
	if (isDark)  r = DarkMessageBox((x1), (x2), (x3), (x4)); \
	else  r = MessageBoxW((x1), (x2), (x3), (x4)); \
} while(0)
	
	int r = DMSMessageBox(NULL,
			//"Yo",
			//"Ελληνικά - No primary display in HDR mode was detected.\n\n"
	    	//"If you continue, graphics will not be rendered as intended.\n"
	    	//"Click OK to launch anyway, or Cancel to exit.",
			//L"oolite.exe - HDR requested", MB_OK | MB_ICONWARNING);
	

            //L"Delete all files?\nThis action cannot be undone.",
            //L"You do not need to call the default window procedure; this function calls it automatically.\n\
            \n\
The SUBCLASS module defines helper functions that are used to subclass windows. The code maintains a single property on the subclassed window and dispatches various subclass callbacks to its clients as required. The client is provided reference data and a default processing API.\n\
            \n\
A subclass callback is identified by a unique pairing of a callback function pointer and an unsigned ID value. Each callback can also store a single DWORD of reference data, which is passed to the callback function when it is called to filter messages. No reference counting is performed for the callback; it may repeatedly call SetWindowSubclass to alter the value of its reference data element.",
			//"Ελληνικά - oolite.exe - HDR requested",
            //L"Warning",
	    	s,
	    	processname,
            MB_OKCANCEL | MB_ICONINFORMATION
			//MB_YESNO | MB_ICONWARNING
        );

    return r;
}
