/*
* Oberon for Windows Boot File Loader (MH Feb 1993 / 1.2.94 / 6.1.95)
* cleanup & loading from resource ejz / 12.5.96 / 22.5.97 / 23.10.97
*/

/* IMPORT */
#include <Windows.h>
#include <stdio.h>
#include <math.h>

/* CONST */
#define DefaultHeapSize  0x200000
#define DefaultBootFile  "Oberon.Hex"
#define DefaultModule    "Oberon"
#define DefaultProc      "Loop"
#define DefaultInifile   "Oberon.Ini"
#define InifileLen       128

/*TYPE */
typedef long int LONGINT;
typedef signed char SHORTINT;
typedef unsigned char OCHAR;
typedef void (*Proc)(void);

/* VAR */
OCHAR   mod[64] = DefaultModule;
OCHAR   cmd[64] = DefaultProc;
OCHAR   bootname[64] = DefaultBootFile;
OCHAR   Inifile[InifileLen];
LONGINT heapAdr, bootBlockAdr;
LPVOID  memoryAdr;
HANDLE  hInstance, hPrevInstance;

/* -------------------- dynamic binding -------------------------------*/

/* Kernel.GetAdr */
void __stdcall GetAdr (LONGINT *adr, OCHAR *ident, LONGINT handle)
{
	if (handle != 0) {
		*adr = (LONGINT)GetProcAddress((HMODULE)handle, ident);
	}
	else {
		if (strcmp("heapAdr", ident) == 0) *adr = (LONGINT)heapAdr;
		else if (strcmp("heapSize", ident) == 0) *adr = (LONGINT)DefaultHeapSize;
		else if (strcmp("modPtr", ident) == 0) *adr = (LONGINT)mod;
		else if (strcmp("cmdPtr", ident) == 0) *adr = (LONGINT)cmd;
		else if (strcmp("Inifile", ident) == 0) *adr = (LONGINT)Inifile;
		else if (strcmp("hInstance", ident) == 0) *adr = (LONGINT)hInstance;
		else if (strcmp("hPrevInstance", ident) == 0) *adr = (LONGINT)hPrevInstance;

		else if (strcmp("sqrt", ident) == 0) *adr = (LONGINT)sqrt;
		else if (strcmp("exp", ident) == 0) *adr = (LONGINT)exp;
		else if (strcmp("ln", ident) == 0) *adr = (LONGINT)log;
		else if (strcmp("sin", ident) == 0) *adr = (LONGINT)sin;
		else if (strcmp("cos", ident) == 0) *adr = (LONGINT)cos;
		else if (strcmp("arctan", ident) == 0) *adr = (LONGINT)atan;

		else if (strcmp("LoadLibrary", ident) == 0) *adr = (LONGINT)LoadLibraryA;
		else if (strcmp("FreeLibrary", ident) == 0) *adr = (LONGINT)FreeLibrary;
		else {
			*adr = 0;
		};
	};
}


/*-------------------------- boot image loading ------------------*/

void ReadLong (LONGINT *x)
{
	*x = *((long*)bootBlockAdr);
	bootBlockAdr += 4;
}

void ReadNum (LONGINT *x)
{
	LONGINT n, shift;
	OCHAR ch;

    shift = 0; n = 0; 
	ch = *((unsigned char*)bootBlockAdr++);
	while (ch >= 128) {
        n += (ch & 0x7F) << shift;
        shift += 7;
		ch = *((unsigned char*)bootBlockAdr++);
    }
    *x = n + (((ch & 0x3f) - ((ch >> 6) << 6)) << shift);
}

void Relocate (LONGINT heapAdr, LONGINT shift)
{
	LONGINT len, adr;

	ReadNum(&len);
	while (len != 0) {
		ReadNum(&adr);
		adr += heapAdr;
		*((LONGINT *)adr) += shift;
		len--;
	}
}

void MakeBootFileName (char *name, int namelen)
{
	int i, j;

	GetModuleFileName(NULL, name, namelen);
	i = 0; j = 0;
	while (name[i] != '\0') {
		if (name[i] == '\\') j = i+1;
		i++;
	}
	i = 0;
	if (j > 0) {
		while (name[j] = bootname[i]) { i++; j++; }
	}
}

void ShowMsg (char* title, char* msg) {
	MessageBox(HWND_DESKTOP, msg, title, MB_ICONSTOP | MB_OK | MB_SETFOREGROUND);
}

void Boot (void)
{
	LONGINT adr, len, shift, d, i, fileHeapAdr, fileHeapSize;
	LONGINT dlsymAdr, bootBlock = (LONGINT)NULL;
	Proc    body;
	char    fullBootFileName[128];
	HANDLE  bootFile = NULL;
	HRSRC   res = NULL;
	HGLOBAL hres = NULL;

	bootBlockAdr = (LONGINT)NULL;
	MakeBootFileName(fullBootFileName, sizeof(fullBootFileName));
	bootFile = CreateFile(fullBootFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (bootFile != INVALID_HANDLE_VALUE) {
		len = GetFileSize(bootFile, NULL);
		bootBlockAdr = (LONGINT)VirtualAlloc(NULL, len, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		ReadFile(bootFile, (LPVOID)bootBlockAdr, len, &len, NULL);
		bootBlock = bootBlockAdr;
		CloseHandle(bootFile);
	} else {
		bootFile = GetModuleHandle(NULL);
		res = FindResource(bootFile, "Oberon", "BootFile");
		if (res != NULL) {
			hres = LoadResource(bootFile, res);
			if (hres != NULL) {
				bootBlockAdr = (LONGINT)LockResource(hres);
			}
		}
		if (bootBlockAdr == (LONGINT)NULL) {
			ShowMsg(fullBootFileName, "Bootfile not found!");
			exit(1);
		}
	}
	ReadLong(&fileHeapAdr); ReadLong(&fileHeapSize);
	memoryAdr = VirtualAlloc(NULL, DefaultHeapSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (memoryAdr == NULL) {
		ShowMsg("Oberon Error", "Not enough memory available to allocate\nOberon heap");
		exit(1);
	}
	heapAdr = (LONGINT)memoryAdr;
	heapAdr += (-heapAdr) & 0x1F;
	d = heapAdr; i = fileHeapSize + 32;
	while (i > 0) {
		*((LONGINT *) d) = 0;
		i -= 4; d += 4;
	}

	shift = heapAdr - fileHeapAdr;
	ReadLong(&adr); ReadLong(&len);

    while (len != 0) {
		adr += shift;
		memcpy((LPVOID)adr, (LPVOID)bootBlockAdr, len);
		bootBlockAdr += len;
        ReadLong(&adr); ReadLong(&len);
    }

	body = (Proc)(adr + shift);
	Relocate(heapAdr, shift);
	ReadLong(&dlsymAdr);
	*((LONGINT *)(heapAdr + dlsymAdr)) = (LONGINT)GetAdr;
	if (bootBlock != (LONGINT)NULL) {
		VirtualFree((LPVOID)bootBlock, 0, MEM_RELEASE);
	}

	__try {
		(*body)();
	} __except (TRUE, EXCEPTION_EXECUTE_HANDLER) {
		ShowMsg("Oberon Error", "Unhandled exception in Oberon");
		exit(1);
	}

	ShowMsg("Oberon Error", "Unhandled exception in Oberon");
	exit(1);
}

/*--------------------- command line interface -----------------------*/

void PrintHelpAndExit(void)
{
	ShowMsg("Oberon Command Line Syntax", "Usage:\n\nOberon [-x module command] [-b bootfile] [-i inifile]");
	exit(1);
}

char * getToken (char *str, char *token)
{
	while ((*str > '\0') && (*str <= ' ')) str++;
	while (*str > ' ') { *token = *str; str++; token++; }
	*token = '\0';
	return(str);
}

int APIENTRY WinMain (HINSTANCE inst, HINSTANCE prevInst, LPSTR cmdLine, int cmdShow)
{
	BOOL  cmdLineErr;
	char  token[128], InifileTemp[128], *cmdPtr;
	LPTSTR dummy;

	hInstance = inst; hPrevInstance = prevInst;
	strcpy(InifileTemp, DefaultInifile);

	cmdPtr = cmdLine; cmdLineErr = FALSE;
	while (*cmdPtr != '\0') {
		cmdPtr = getToken(cmdPtr, token);
		if ((token[0] == '-') || (token[0] == '/')) {
			switch (token[1]) {
			case 'x': case 'X':
				cmdPtr = getToken(cmdPtr, mod);
				cmdPtr = getToken(cmdPtr, cmd);
				break;
			case 'b': case 'B':
				cmdPtr = getToken(cmdPtr, bootname);
				break;
			case 'i': case 'I':
				cmdPtr = getToken(cmdPtr, InifileTemp);
				break;
			default:
				PrintHelpAndExit();
			}
		}
		else if (token[0] == '\0') break;
		else {
			cmdLineErr = TRUE;
		}
		
	}

	if (cmdLineErr) PrintHelpAndExit();

	if (!SearchPath(NULL, InifileTemp, ".INI", InifileLen, Inifile, &dummy)) {
		ShowMsg(InifileTemp, "Ini file not found!");
		return(1);
	}

	Boot();

	/* no return to this point*/
	return(1);
}

