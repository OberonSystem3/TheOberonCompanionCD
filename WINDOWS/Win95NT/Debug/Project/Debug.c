#include <windows.h>
#include <richedit.h>
#include <stdio.h>
#include "Debug.h"

#define Title "Debug (ejz)"

#define ModLink 0
#define ModName 4
#define ModRefCnt 40
#define ModSB 44
#define ModData 104
#define ModCode 108
#define ModRefs 112

#define VarTag 1
#define VarParTag 3
#define ProcRefTag 0xF8

#define MAX_THREADS 32

static HANDLE hInst, hThread = INVALID_HANDLE_VALUE;
static HWND hwndMain, hwndMDIClient, hwndCon, hwndExc, hwndSys;
static CHAR exePath[MAX_PATH] = "", workPath[MAX_PATH] = "", iniFile[MAX_PATH];
static CHAR module[64];
static PROCESS_INFORMATION proc;
static STARTUPINFO start;
static DWORD threadId, debugAction, level /* 0: nix; 1: inner core; 2: outer core */ ;
static LPCSTR logMsg;
static LPVOID modules;
static EXCEPTION_POINTERS excPtrs;
static struct thread {HANDLE hThread; DWORD dwThreadId;} threads[MAX_THREADS];

int WINAPI WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	WNDCLASS wndclass;
	MSG msg;

	if(!hPrevInstance) {
		wndclass.style = 0;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, "DEBUG");
		wndclass.hCursor = NULL;
		wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		wndclass.lpszMenuName = "Debug_Menu";
		wndclass.lpszClassName = "Debug_Class";
     	if (!RegisterClass(&wndclass)) {
			return 1;
 		}
		wndclass.lpfnWndProc = ChildWndProc;
		wndclass.cbWndExtra = 4;
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "Debug_Child_Class";
     	if (!RegisterClass(&wndclass)) {
			return 1;
 		}
    }

	hwndMain = CreateWindow("Debug_Class", Title, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | 
		WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN | WS_OVERLAPPED,
 		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	if (hwndMain == NULL) {
		return 1;
	}

	hInst = hInstance;

 	ShowWindow(hwndMain, nCmdShow);
 	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

 	UnregisterClass("Debug_Child_Class", hInstance);
	UnregisterClass("Debug_Class", hInstance);
 	return msg.wParam;
} 

DWORD CALLBACK EditStream(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb) {
	(*pcb) = 0;
	while (cb > 0) {
		if (logMsg[0] != (CHAR)0) {
			pbBuff[0] = (BYTE)(logMsg[0]);
			pbBuff++; logMsg++;
			(*pcb)++; cb--;
		} else {
			return 0;
		}
	}
	return (DWORD)(*pcb);
}

void ClearLog(HWND hWnd)  {
	HWND hwndE;
	EDITSTREAM estream;
	CHAR msg[4];

	hwndE = (HWND)GetWindowWord(hWnd, GWW_HWNDEDIT);
	estream.pfnCallback = EditStream;
	msg[0] = (CHAR)0; logMsg = msg;
	SendMessage(hwndE, EM_STREAMIN, SF_TEXT, (LPARAM)&estream);
}

void LogMessage(HWND hWnd, LPCSTR msg) {
	HWND hwndE;
	EDITSTREAM estream;
	DWORD len;

	if (hWnd != NULL) {
		hwndE = (HWND)GetWindowWord(hWnd, GWW_HWNDEDIT);
		len = SendMessage(hwndE, EM_GETLIMITTEXT, 0, 0);
		SendMessage(hwndE, EM_SETSEL, len, len);
		estream.pfnCallback = EditStream;
		logMsg = msg;
		SendMessage(hwndE, EM_STREAMIN, SF_TEXT | SFF_SELECTION, (LPARAM)&estream);
	} else {
		MessageBox(hwndMain, msg, Title, MB_OK | MB_APPLMODAL);
	}
}

LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE: {
			HWND hwnd;

			if (LoadLibrary("RichED32.DLL") == NULL) {
				LogMessage(NULL, "Requires RichED32.DLL!\n");
			} else {
				hwnd = CreateWindow("RichEdit", NULL, WS_CHILD | WS_VISIBLE | WS_HSCROLL |
					WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
					0, 0, 0, 0, hWnd,	(HMENU)ID_EDIT,	hInst, (LPVOID)NULL);
				SetWindowWord(hWnd, GWW_HWNDEDIT, (WORD)hwnd);
			}
			break;
		}

		case WM_SIZE: {
			RECT rc;
			HWND hwnd;

			GetClientRect(hWnd, &rc);
			hwnd = (HWND)GetWindowWord(hWnd, GWW_HWNDEDIT);
			MoveWindow(hwnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
			return DefMDIChildProc(hWnd, uMsg, wParam, lParam);

			break;
		}

		case WM_CLOSE:
			return 0;
			break;

		default:
			return DefMDIChildProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDD_Ok) {
				EndDialog(hwndDlg, 0);
				return TRUE;
			}
			break;

		default: return FALSE;
	}
	return FALSE;
}

void ShowModules(void) {
	LPVOID mod;
	DWORD read, refCnt;
	CHAR modName[32];
	CHAR msg[64];

	LogMessage(hwndSys, "ShowModules\n");
	ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)modules+ModLink), &mod, 4, &read);
	while ((read == 4) && (mod != NULL)) {
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModName), modName, 32, &read);
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModRefCnt), &refCnt, 4, &read);
		wsprintf(msg, "\t%s %i\n", modName, refCnt);
		LogMessage(hwndSys, msg);
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModLink), &mod, 4, &read);
	}
	LogMessage(hwndSys, "\n");
}

void ShowThreads(void) {
	DWORD i;
	CHAR msg[64];

	LogMessage(hwndSys, "ShowThreads\n");
	i = 0;
	while (i < MAX_THREADS) {
		if (threads[i].hThread != INVALID_HANDLE_VALUE) {
			wsprintf(msg, "%lu\n", threads[i].dwThreadId);
			LogMessage(hwndSys, msg);
		}
		i++;
	}
	LogMessage(hwndSys, "\n");
}

CHAR Read(LPVOID *adr) {
	DWORD read;
	CHAR ch;

	ReadProcessMemory(proc.hProcess, (*adr), &ch, 1, &read);
	(*adr) = (LPVOID)((DWORD)(*adr)+read);
	if (read != 1) {
		return (CHAR)0;
	} else {
		return ch;
	}
}

BYTE ReadByte(LPVOID *adr) {
	DWORD read;
	BYTE b;

	ReadProcessMemory(proc.hProcess, (*adr), &b, 1, &read);
	(*adr) = (LPVOID)((DWORD)(*adr)+read);
	if (read != 1) {
		return (BYTE)0;
	} else {
		return b;
	}
}

void ReadBytes(LPVOID *adr, LPBYTE buf, DWORD len) {
	DWORD read;

	ReadProcessMemory(proc.hProcess, (*adr), buf, len, &read);
	(*adr) = (LPVOID)((DWORD)(*adr)+read);
	while (read < len) {
		buf[read] = (BYTE)0; read++;
	}
}

LONG ReadNum(LPVOID *adr) {
	LONG i, s, n, x;

	s = 0; n = 0;
	x = ReadByte(adr); 
	while (x >= 128) {
		n = n + ((x-128) << s);
		s = s+7;
		x = ReadByte(adr);
	}
	i = n + (( (x % 64) - (x /64)*64) << s);
	return i;
}

void ReadString(LPVOID *adr, LPSTR str, DWORD len) {
	DWORD i;
	CHAR x;

	i = 0;
	do {
		x = Read(adr); str[i] = x;
		i++;
	} while ((x != (CHAR)0) && (i < len));
	str[len-1] = (CHAR)0;
}

void DumpVars(LPVOID *adr, LPVOID base) {
	DWORD offs;
	LPVOID varAdr;
	CHAR name[32], msg[64];
	BYTE tag, form, b;
	CHAR si;
	short int i;
	LONG li;
	float r;
	double lr;

	tag = ReadByte(adr);
	while ((tag == VarTag) || (tag == VarParTag)) {
		form = ReadByte(adr);
		offs = ReadNum(adr); ReadString(adr, name, 32);
		wsprintf(msg, "\t%s = ", name);
		LogMessage(hwndSys, msg);
		varAdr = (LPVOID)((DWORD)base+offs);
		if (tag == VarParTag) {
			ReadBytes(&varAdr, (LPBYTE)(&li), 4);
			varAdr = (LPVOID)li;
		}
		if (form != (BYTE)0) {
			switch (form) {
				case (BYTE)1:
					b = ReadByte(&varAdr);
					wsprintf(msg, "%u", b);
					break;

				case (BYTE)2:
					b = ReadByte(&varAdr);
					if (b == 0) {
						strcpy(msg, "FALSE");
					} else {
						strcpy(msg, "TRUE");
					}
					break;

				case (BYTE)3:
					b = ReadByte(&varAdr);
					wsprintf(msg, "CHR(%u)", b);
					break;

				case (BYTE)4:
					ReadBytes(&varAdr, &si, 1);
					wsprintf(msg, "%i", si);
					break;

				case (BYTE)5:
					ReadBytes(&varAdr, (LPBYTE)(&i), 2);
					wsprintf(msg, "%i", i);
					break;

				case (BYTE)6:
					ReadBytes(&varAdr, (LPBYTE)(&li), 4);
					wsprintf(msg, "%i", li);
					break;
				
				case (BYTE)7:
					ReadBytes(&varAdr, (LPBYTE)(&r), 4);
					wsprintf(msg, "%f", r);
					break;

				case (BYTE)8:
					ReadBytes(&varAdr, (LPBYTE)(&lr), 8);
					wsprintf(msg, "%f", lr);
					break;

				case (BYTE)9:
					ReadBytes(&varAdr, (LPBYTE)(&li), 4);
					wsprintf(msg, "SET(0%lXH)", li);
					break;

				case (BYTE)13:
					ReadBytes(&varAdr, (LPBYTE)(&li), 4);
					wsprintf(msg, "PTR(0%lXH)", li);
					break;

				case (BYTE)15:
					si = ReadByte(&varAdr);
					msg[0] = '"'; i = 1;
					while ((si >= ' ') & (si <= '~')) {
						msg[i] = si; i++;
						si = ReadByte(&varAdr);
					}
					msg[i] = '"';
					msg[i+1] = (CHAR)0;
					break;

				default:
					strcpy(msg, "(unknown type)");
					break;
			}
			LogMessage(hwndSys, msg);
		} else {
			LogMessage(hwndSys, "(invalid address)");
		}
		wsprintf(msg, "\n");
		LogMessage(hwndSys, msg);
		tag = ReadByte(adr);
	}
}

void TrapMsg(DWORD excode, DWORD eax, LPSTR msg, BOOL ln) {
	CHAR form[8];

	if (ln)
		strcpy(form, "%s\n");
	else
		strcpy(form, "%s");
	switch (excode) {
		case EXCEPTION_ACCESS_VIOLATION:
			wsprintf(msg, form, "Access Violation");
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			wsprintf(msg, form, "Array Bounds Exceeded");
			break;
		case EXCEPTION_BREAKPOINT:
			switch (eax) {
				case 1:
					wsprintf(msg, form, "heap overflow");
					break;
				case 15:
					wsprintf(msg, form, "invalid case in WITH statement");
					break;
				case 16:
					wsprintf(msg, form, "invalid case in CASE statement");
					break;
				case 17:
					wsprintf(msg, form, "function procedure without RETURN");
					break;
				case 18:
					wsprintf(msg, form, "type guard check");
					break;
				case 19:
					wsprintf(msg, form, "implicit type guard check in record assignment");
					break;
				case 21:
					wsprintf(msg, form, "index out of range");
					break;
				case 22:
					wsprintf(msg, form, "dimension trap");
					break;
				case 24:
					wsprintf(msg, form, "abort from keyboard");
					break;
				default:
					if (ln)
						wsprintf(msg, "HALT / ASSERT failed %i\n", eax);
					else
						wsprintf(msg, "HALT / ASSERT failed %i", eax);
					break;
			}
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			wsprintf(msg, form, "Data Misalignment");
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			wsprintf(msg, form, "Float Denormal Operand");
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			wsprintf(msg, form, "Float Divide By Zero");
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			wsprintf(msg, form, "Float Inexact Result");
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			wsprintf(msg, form, "Float Invalid Operation");
			break;
		case EXCEPTION_FLT_OVERFLOW:
			wsprintf(msg, form, "Float Overflow");
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			wsprintf(msg, form, "Float Stack Check");
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			wsprintf(msg, form, "Float Underflow");
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			wsprintf(msg, form, "Illegal Instruction");
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			wsprintf(msg, form, "Page Error");
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			wsprintf(msg, form, "Integer Divide By Zero");
			break;
		case EXCEPTION_INT_OVERFLOW:
			wsprintf(msg, form, "Integer Overflow");
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			wsprintf(msg, form, "Invalid Disposition");
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			wsprintf(msg, form, "Noncontinuable Exception");
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			wsprintf(msg, form, "Privileged Instruction");
			break;
		case EXCEPTION_SINGLE_STEP:
			wsprintf(msg, form, "Single Step");
			break;
		case EXCEPTION_STACK_OVERFLOW:
			wsprintf(msg, form, "Stack Overflow");
			break;
		case EXCEPTION_GUARD_PAGE:
			wsprintf(msg, form, "Guard Page Violation");
			break;
		default:
			if (ln)
				wsprintf(msg, "ExceptionCode %i\n", excode);
			else
				wsprintf(msg, "ExceptionCode %i", excode);
			break;
	}
}

LPVOID FindProc(DWORD pc, DWORD *ref) {
	LPVOID mod, pos;
	DWORD read, codebase, codelen, refend, beg, offs;
	LONG ch;
	CHAR name[32];

	ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)modules+ModLink), &mod, 4, &read);
	while ((read == 4) && (mod != NULL)) {
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModCode), &codebase, 4, &read);
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)codebase+12), &codelen, 4, &read);
		codebase = codebase+16;
		if ((pc >= codebase) && (pc <= (codebase+codelen))) {
			read = 0;
		} else {
			ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModLink), &mod, 4, &read);
			if (read != 4) {
				mod = NULL;
			}
		}
	}
	if (mod != NULL) {
		pc = pc-codebase;
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModRefs), &pos, 4, &read);
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)pos+12), &codelen, 4, &read);
		pos = (LPVOID)((DWORD)pos+16);
		refend = (DWORD)pos+codelen;
		ch = ReadByte(&pos);
		beg = (DWORD)pos; (*ref) = 0;
		while (((DWORD)pos <= refend) && (ch == ProcRefTag)) {
			(*ref) = beg; beg = (DWORD)pos;
			offs = ReadNum(&pos);
			if (offs >= pc) {
				return mod;
			}
			ReadString(&pos, name, 32);
			ch = ReadByte(&pos);
			while ((ch == VarTag) || (ch == VarParTag)) {
				ch = ReadByte(&pos);
				offs = ReadNum(&pos);
				ReadString(&pos, name, 32);
				ch = ReadByte(&pos);
			}
		}
		(*ref) = beg;
	}
	return mod;
}

void ShowStack(EXCEPTION_POINTERS *exp, BOOL trap) {
	DWORD bp, sp, read, ref, n;
	LPVOID pc, mod, sb;
	CHAR msg[128], modn[32], name[32];

	pc = exp->ExceptionRecord->ExceptionAddress;
	bp = exp->ContextRecord->Ebp;
	sp = exp->ContextRecord->Esp;
	if (pc == NULL) {
		ReadProcessMemory(proc.hProcess, (LPVOID)(sp), &pc, 4, &read);
	}
	if (trap) {
		TrapMsg(exp->ExceptionRecord->ExceptionCode, exp->ContextRecord->Eax, msg, TRUE);
	} else {
		wsprintf(msg, "StackFrames\n");
	}
	LogMessage(hwndSys, msg);
// stack frames
	wsprintf(msg, "  PC = 0%lXH\n", pc);
	LogMessage(hwndSys, msg);
	mod = FindProc((DWORD)pc, &ref); n = 0;
	while ((bp > 0) && (n < 64)) {
		if (mod != NULL) {
			ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModName), modn, 32, &read);
			read = ReadNum(&((LPVOID)ref));
			ReadString(&((LPVOID)ref), name, 32);
			wsprintf(msg, "%s.%s\n", modn, name);
			LogMessage(hwndSys, msg);
			if (strcmp(name, "$$") == 0) {
				ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModSB), &sb, 4, &read);
				DumpVars(&((LPVOID)ref), sb);
			} else {
				DumpVars(&((LPVOID)ref), (LPVOID)bp);
			}
		} else {
			wsprintf(msg, "mod not found\n");
			LogMessage(hwndSys, msg);
		}
		ReadProcessMemory(proc.hProcess, (LPVOID)(bp+4), &pc, 4, &read);
		ReadProcessMemory(proc.hProcess, (LPVOID)(bp), &bp, 4, &read);
		mod = FindProc((DWORD)pc, &ref); n++;
	}
}

void ShowState(void) {
	LPVOID mod, refs, base;
	DWORD read;
	CHAR modName[32] = "";
	CHAR msg[64];
	LONG i, x;
	CHAR ch;

	wsprintf(msg, "Module State %s\n", module);
	LogMessage(hwndSys, msg);
	ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)modules+ModLink), &mod, 4, &read);
	while ((read == 4) && (mod != NULL)) {
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModName), modName, 32, &read);
		if (strcmp(modName, module) != 0) {
			ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModLink), &mod, 4, &read);
		}
	}
	if (strcmp(modName, module) == 0) {
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModRefs), &refs, 4, &read);
		refs = (LPVOID)((DWORD)refs+16);
		while (read > 0) {
			x = ReadByte(&refs);
			if (x == ProcRefTag) {
				i = ReadNum(&refs);
				x = ReadByte(&refs); ch = Read(&refs);
				if (ch == '$') {
					x = ReadByte(&refs);
					break;
				}
			}
		}
		ReadProcessMemory(proc.hProcess, (LPVOID)((DWORD)mod+ModSB), &base, 4, &read);
		DumpVars(&refs, base);
	} else {
		wsprintf(msg, "\tNot Loaded\n");
		LogMessage(hwndSys, msg);
	}
	LogMessage(hwndSys, "\n");
}

DWORD WINAPI ThreadProc(LPVOID arg) {
	HANDLE hProcess = NULL, hExcThread;
	DEBUG_EVENT event;
	DWORD processId, read, len, i, val;
	LPCSTR debStr;
	CHAR msg[1024];
	BOOL ignore, fetchAdr;

	if (CreateProcess(exePath, NULL, NULL, NULL, FALSE, DEBUG_PROCESS, NULL, workPath,
		&start, &proc)) {
		WritePrivateProfileString("Debug", "Exe", exePath, iniFile);
		EnableMenuItem(GetMenu(hwndMain), IDM_D_Stop, MF_BYCOMMAND);
		EnableMenuItem(GetMenu(hwndMain), IDM_A_THREADS, MF_BYCOMMAND);
		LogMessage(hwndSys, "Debugger started\n");
		modules = NULL; ignore = TRUE; fetchAdr = FALSE; level = 0;
		i = 0;
		while (i < MAX_THREADS) {
			threads[i].hThread = INVALID_HANDLE_VALUE;
			threads[i].dwThreadId = 0;
			i++;
		}
		while (WaitForDebugEvent(&event, INFINITE)) {
			debugAction = DBG_CONTINUE;
			switch (event.dwDebugEventCode) {
				case EXCEPTION_DEBUG_EVENT:
					if (!ignore) {
						i = 0;
						while ( (i < MAX_THREADS) && (threads[i].dwThreadId != event.dwThreadId)) {
							i++;
						}
						if (i < MAX_THREADS) {
							hExcThread = threads[i].hThread;
							excPtrs.ExceptionRecord = &(event.u.Exception.ExceptionRecord);
							excPtrs.ContextRecord->ContextFlags = CONTEXT_FULL;
							GetThreadContext(hExcThread, excPtrs.ContextRecord);
							EnableMenuItem(GetMenu(hwndMain), IDM_A_CONTINUE, MF_BYCOMMAND);
							EnableMenuItem(GetMenu(hwndMain), IDM_A_EXCEPTION, MF_BYCOMMAND);
							EnableMenuItem(GetMenu(hwndMain), IDM_A_TRAP, MF_BYCOMMAND);
							TrapMsg(excPtrs.ExceptionRecord->ExceptionCode, excPtrs.ContextRecord->Eax, msg, FALSE);
							LogMessage(hwndExc, msg);
							SuspendThread(hThread);
							if (debugAction == DBG_CONTINUE) {
// if not breakpoint -> step to next instruction
								LogMessage(hwndExc, " Continue\n");
							} else {
								LogMessage(hwndExc, " Exception\n");
							}
						} else {
							debugAction = DBG_EXCEPTION_NOT_HANDLED;
							LogMessage(hwndExc, " unknown thread\n");
						}
					} else {
						ignore = FALSE;
					}
					break;

				case CREATE_THREAD_DEBUG_EVENT:
					wsprintf(msg, "CreateThread %i\n", event.dwThreadId);
					LogMessage(hwndExc, msg);
					i = 0;
					while ((threads[i].dwThreadId != event.dwThreadId) && (threads[i].hThread != INVALID_HANDLE_VALUE)) {
						i++;
					}
					threads[i].hThread = event.u.CreateThread.hThread;
					threads[i].dwThreadId = event.dwThreadId;
					break;

				case EXIT_THREAD_DEBUG_EVENT:
					wsprintf(msg, "ExitThread %i\n", event.dwThreadId);
					LogMessage(hwndExc, msg);
					i = 0;
					while (threads[i].dwThreadId != event.dwThreadId) {
						i++;
					}
					threads[i].hThread = INVALID_HANDLE_VALUE;
					threads[i].dwThreadId = 0;
					break;

				case CREATE_PROCESS_DEBUG_EVENT:
					if (hProcess == NULL) {
						hProcess = event.u.CreateProcessInfo.hProcess;
						processId = event.dwProcessId;
						wsprintf(msg, "CreateProcess (Thread %i)\n", event.dwThreadId);
						LogMessage(hwndExc, msg);
						i = 0;
						while ((threads[i].dwThreadId != event.dwThreadId) && (threads[i].hThread != INVALID_HANDLE_VALUE)) {
							i++;
						}
						threads[i].hThread = event.u.CreateProcessInfo.hThread;
						threads[i].dwThreadId = event.dwThreadId;
					} else {

					}
					break;

				case EXIT_PROCESS_DEBUG_EVENT:
					if (event.dwProcessId == processId) {
						wsprintf(msg, "ExitProcess (ret %i)\n", event.u.ExitProcess.dwExitCode);
						LogMessage(hwndExc, msg);
						PostMessage(hwndMain, WM_STOP, 0, 0);
						ExitThread(0);
					} else {

					}
					break;

				case OUTPUT_DEBUG_STRING_EVENT:
					debStr = event.u.DebugString.lpDebugStringData;
					len = event.u.DebugString.nDebugStringLength; read = len;
					while ((len > 0) && (read > 0)) {
						if (len > 1023 ) {
							ReadProcessMemory(proc.hProcess, debStr, msg, 1023, &read);
						} else {
							ReadProcessMemory(proc.hProcess, debStr, msg, len, &read);
							if (level < 2) {
								if (fetchAdr) {
									modules == NULL; i = 0;
									while ((msg[i] > ' ') && (i < read)) {
										if ((msg[i] >= '0') && (msg[i] <= '9')) {
											val = (DWORD)(msg[i])-(DWORD)'0';
										} else {
											val = 10+(DWORD)(msg[i])-(DWORD)'A';
										}
										modules = (LPVOID)(16*(DWORD)modules+val);
										i++;
									}
									level++;
									if (level == 1) {
										LogMessage(hwndSys, "Inner Core\n");
									} else {
										LogMessage(hwndSys, "Outer Core\n");
									}
									EnableMenuItem(GetMenu(hwndMain), IDM_A_MODULES, MF_BYCOMMAND);
									EnableMenuItem(GetMenu(hwndMain), IDM_A_STATE, MF_BYCOMMAND);
									EnableMenuItem(GetMenu(hwndMain), IDM_A_STACK, MF_BYCOMMAND);
									fetchAdr = FALSE;
								} else if (strncmp(msg, "Kernel.modules", 14) == 0) {
									fetchAdr = TRUE;
								}
							}
						}
						debStr = debStr+read; len = len-read;
						msg[read] = (CHAR)0;
						LogMessage(hwndCon, msg);
					}
					break;

				default:

					break;
			}
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, debugAction);
		}
	} else {
		LogMessage(hwndExc, "CreateProcess failed!\n");
	}
	PostMessage(hwndMain, WM_STOP, 0, 0);
	ExitThread(0);
	return 0;
}

BOOL CALLBACK StateDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDD_Ok) {
				GetDlgItemText(hwndDlg, IDD_State, module, 64);
				EndDialog(hwndDlg, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDD_Cancel) {
				strcpy(module, "");
				EndDialog(hwndDlg, 0);
				return TRUE;
			}
			break;

		default:
			return FALSE;
	}
	return FALSE;
}

BOOL CALLBACK StartDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	INT i, j;

	switch (uMsg) {
		case WM_INITDIALOG:
			SetDlgItemText(hwndDlg, IDD_ExePath, exePath);
			return TRUE;
			break;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDD_Ok) {
				GetDlgItemText(hwndDlg, IDD_ExePath, exePath, MAX_PATH);
				strcpy(workPath, exePath);
				i = 0; j = -1;
				while (workPath[i] != (CHAR)0) {
					if (workPath[i] == '\\') {
						j = i;
					}
					i++;
				}
				workPath[j+1] = (CHAR)0;
				SendMessage(hwndMain, WM_START, 0, 0);
				EndDialog(hwndDlg, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDD_Cancel) {
				EnableMenuItem(GetMenu(hwndMain), IDM_D_Start, MF_BYCOMMAND);
				EndDialog(hwndDlg, 0);
				return TRUE;
			}
			break;

		default:
			return FALSE;
	}
	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) {
		case WM_START:
			ClearLog(hwndCon); ClearLog(hwndExc); ClearLog(hwndSys);
			EnableMenuItem(GetMenu(hWnd), IDM_D_Start, MF_BYCOMMAND | MF_GRAYED);
			hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &threadId);
			break;

    	case WM_STOP:
			EnableMenuItem(GetMenu(hwndMain), IDM_A_MODULES, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(GetMenu(hwndMain), IDM_A_STATE, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(GetMenu(hwndMain), IDM_A_STACK, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(GetMenu(hWnd), IDM_D_Stop, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(GetMenu(hWnd), IDM_A_THREADS, MF_BYCOMMAND | MF_GRAYED);
			if (proc.hThread != INVALID_HANDLE_VALUE) {
				CloseHandle(proc.hThread);
				proc.hThread = INVALID_HANDLE_VALUE;
			}
			if (proc.hProcess != INVALID_HANDLE_VALUE) {
				TerminateProcess(proc.hProcess, 0);
				CloseHandle(proc.hProcess);
				proc.hProcess = INVALID_HANDLE_VALUE;
			}
			TerminateThread(hThread, 0);
			CloseHandle(hThread);
			LogMessage(hwndSys, "Debugger stopped\n");
			hThread = INVALID_HANDLE_VALUE;
			EnableMenuItem(GetMenu(hwndMain), IDM_D_Start, MF_BYCOMMAND);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDM_D_Start:
					EnableMenuItem(GetMenu(hWnd), IDM_D_Start, MF_BYCOMMAND | MF_GRAYED);
					DialogBox(hInst, "Debug_Start_Dialog", hWnd, StartDlgProc);
					break;
				case IDM_D_Stop:		
					SendMessage(hWnd, WM_STOP, 0, 0);
					break;
				case IDM_D_Exit:
					SendMessage(hWnd, WM_CLOSE, 0, 0);
					break;

				case IDM_W_ONTOP:
					if (CheckMenuItem(GetMenu(hWnd), IDM_W_ONTOP, MF_BYCOMMAND | MF_CHECKED) == MF_CHECKED) {
						CheckMenuItem(GetMenu(hWnd), IDM_W_ONTOP, MF_BYCOMMAND | MF_UNCHECKED);
						SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						WritePrivateProfileString("Debug", "OnTop", "Off", iniFile);
					} else {
						SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						WritePrivateProfileString("Debug", "OnTop", "On", iniFile);
					}
					break;

				case IDM_W_TILE:
					SendMessage(hwndMDIClient, WM_MDITILE, 0, 0);
					break;

				case IDM_W_CASCADE:
					SendMessage(hwndMDIClient, WM_MDICASCADE, 0, 0);
					break;

				case IDM_W_ICONS:
					SendMessage(hwndMDIClient, WM_MDIICONARRANGE, 0, 0);
					break;

				case IDM_W_CALL:
					ClearLog(hwndCon);
					ClearLog(hwndExc);
					ClearLog(hwndSys);
					break;

				case IDM_W_CCONSOLE:
					ClearLog(hwndCon);
					break;

				case IDM_W_CEXCEPTION:
					ClearLog(hwndExc);
					break;

				case IDM_W_CSYSTEM:
					ClearLog(hwndSys);
					break;

				case IDM_H_About:
					DialogBox(hInst, "Debug_About_Dialog", hWnd, AboutDlgProc);
					break;

				case IDM_A_EXCEPTION:
					debugAction = DBG_EXCEPTION_NOT_HANDLED;
				case IDM_A_CONTINUE:
					EnableMenuItem(GetMenu(hWnd), IDM_A_CONTINUE, MF_BYCOMMAND | MF_GRAYED);
					EnableMenuItem(GetMenu(hWnd), IDM_A_EXCEPTION, MF_BYCOMMAND | MF_GRAYED);
					EnableMenuItem(GetMenu(hWnd), IDM_A_TRAP, MF_BYCOMMAND | MF_GRAYED);
					ResumeThread(hThread);
					break;

				case IDM_A_TRAP:
					ShowStack(&excPtrs, TRUE);
					break;

				case IDM_A_MODULES:
					ShowModules();
					break;

				case IDM_A_THREADS:
					ShowThreads();
					break;

				case IDM_A_STATE:
					DialogBox(hInst, "Debug_State_Dialog", hWnd, StateDlgProc);
					if (strcmp(module, "") != 0) {
						ShowState();
					}
					break;

				case IDM_A_STACK: {
						DWORD dw, i;
						CHAR msg[128];

						DialogBox(hInst, "Debug_Stack_Dialog", hWnd, StateDlgProc);
						if (strcmp(module, "") != 0) {
							sscanf(module, "%lu", &dw);
							i = 0;
							while ((i < MAX_THREADS) && (threads[i].dwThreadId != dw)) {
								i++;
							}
							if (i < MAX_THREADS) {
								excPtrs.ExceptionRecord = (PEXCEPTION_RECORD)malloc(sizeof(EXCEPTION_RECORD));
								excPtrs.ContextRecord->ContextFlags = CONTEXT_FULL;
								GetThreadContext(threads[i].hThread, excPtrs.ContextRecord);
								excPtrs.ExceptionRecord->ExceptionCode = 0;
								excPtrs.ExceptionRecord->ExceptionAddress = (LPVOID)(excPtrs.ContextRecord->Eip);
								ShowStack(&excPtrs, FALSE);
								free(excPtrs.ExceptionRecord);
							} else {
								wsprintf(msg, "ShowStack %i\n", dw);
								LogMessage(hwndSys, msg);
								wsprintf(msg, "\tThread not found\n\n", dw);
								LogMessage(hwndSys, msg);
							}
						}
						break;
					}

				default:
					return DefFrameProc(hWnd, hwndMDIClient, uMsg, wParam, lParam);
					break;
			}
			break;

		case WM_CREATE: {
				CLIENTCREATESTRUCT ccs;
				INT i, j;

				proc.hThread = INVALID_HANDLE_VALUE;
				proc.hProcess = INVALID_HANDLE_VALUE;
				ccs.hWindowMenu = GetSubMenu(GetMenu(hWnd), IDM_W_POS);
				ccs.idFirstChild = IDM_WIN_CHILD;
				hwndMDIClient = CreateWindow("MDIClient", NULL, WS_CHILD | WS_CLIPCHILDREN |
					WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, hWnd, (HMENU)ID_EDIT, hInst, (LPVOID)&ccs);
				ShowWindow(hwndMDIClient, SW_SHOW);
				hwndCon = CreateMDIWindow("Debug_Child_class", "Console", MDIS_ALLCHILDSTYLES |
					WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwndMDIClient,
					hInst, (LPARAM)NULL);
				ShowWindow(hwndCon, SW_SHOW);
				hwndExc = CreateMDIWindow("Debug_Child_class", "Exception", MDIS_ALLCHILDSTYLES |
					WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwndMDIClient,
					hInst, (LPARAM)NULL);
				ShowWindow(hwndExc, SW_SHOW);
				hwndSys = CreateMDIWindow("Debug_Child_class", "System", MDIS_ALLCHILDSTYLES |
					WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwndMDIClient,
					hInst, (LPARAM)NULL);
				ShowWindow(hwndSys, SW_SHOW);
				PostMessage(hwndMDIClient, WM_MDITILE, 0, 0);

				GetModuleFileName(NULL, iniFile, MAX_PATH);
				i = 0; j = 0;
				while (iniFile[i] != (CHAR)0) {
					if (iniFile[i] == '.') {
						j = i;
					}
					i++;
				}
				iniFile[j+1] = 'I'; iniFile[j+2] = 'n';
				iniFile[j+3] = 'i'; iniFile[j+4] = (CHAR)0;
				strcpy(exePath, ""); strcpy(workPath, "");
				GetPrivateProfileString("Debug", "OnTop", workPath, exePath, MAX_PATH, iniFile);
				if (strcmp(exePath, "On") == 0) {
					CheckMenuItem(GetMenu(hWnd), IDM_W_ONTOP, MF_BYCOMMAND | MF_CHECKED);
					SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
				strcpy(exePath, ""); strcpy(workPath, "");				
				GetPrivateProfileString("Debug", "Exe", workPath, exePath, MAX_PATH, iniFile);
				excPtrs.ContextRecord = (PCONTEXT)malloc(sizeof(CONTEXT));
				break;
			}

    	case WM_CLOSE:
			SendMessage(hWnd, WM_STOP, 0, 0);
			DestroyWindow(hWnd);
			break;
		
		case WM_DESTROY:
			free(excPtrs.ContextRecord);
			PostQuitMessage(0);
        	break;

		default:
			return DefFrameProc(hWnd, hwndMDIClient, uMsg, wParam, lParam);
	}
	return 0;
}
