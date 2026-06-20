# DMSMessageBox
A Dark Mode - Sensitive implementation of MessageBox for Windows

This is a Windows API in C which I made because I couldn't be
bothered waiting for Microsoft to fix its OS's dark mode anymore.
It can be used instead of the standard MessageBox API function. 
It's not fully implemented yet but does the job for what I would
need right now.

Supports MB_OK, MB_YESNO and MB_OKCANCEL message boxes
with either no or with inormation, warning, error
and questions icons. DMSMessageBox honors the currently
selected mode (light or dark) and can be updated in
real time when such mode changes. It is also DPI-aware.

The API contains both Unicode and ANSI variants of the
implementation which can be called either selectively
with the W or A suffix respectively or automatically by
simply calling DMSMessageBox with the typical parameters
used for the normal MessageBox Win32 API function.

Originally created for Oolite (https://oolite.space) but never
made it there. This is how it would look with Oolite's --help
message box in dark mode. You can see that DMSMessageBox actually
honors it.
<img width="609" height="747" alt="image" src="https://github.com/user-attachments/assets/6b24a44b-a93c-431b-a1aa-c63cb49b3305" />


## Usage
Build DMSMessageBox with  
`clang -c DMSMessageBox.c -I. -o DMSMessageBox.o`

Then you can simply link it with your projects (see example.c):  
`clang -o DMSMessageBoxExample.exe example.c DMSMessageBox.o -I. -ldwmapi -lcomctl32 -mwindows`
