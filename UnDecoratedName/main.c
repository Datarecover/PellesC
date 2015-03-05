#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <DbgHelp.h>
#include <tchar.h>
#include "main.h"

static void RemoveSpace(char* pSrc)
{
	char* pDst = pSrc;
	while(*pSrc != 0)
	{
		if (*pSrc != ' ' && *pSrc != '\t')
			*pDst++ = *pSrc;
		pSrc++;
	}
	*pDst = 0;
}

void AppendLog(HWND hDlg, const char* pMsg)
{
	HWND hEdit = GetDlgItem(hDlg, IDC_HST);
	int nLen = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);
	SendMessage(hEdit, EM_SETSEL, nLen, nLen);
	SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)pMsg);
}

static void UnDecorateName(HWND hDlg)
{
	char sDecoratedName[256];
	GetDlgItemText(hDlg, IDC_NAME, sDecoratedName, 256);
	AppendLog(hDlg, sDecoratedName);
	AppendLog(hDlg, "\r\n");
	RemoveSpace(sDecoratedName);

	char sUndecoratedName[256];
	if (UnDecorateSymbolName(sDecoratedName, sUndecoratedName, 256, 0))
	{
		SetDlgItemText(hDlg, IDC_UNAME, sUndecoratedName);
		AppendLog(hDlg, sUndecoratedName);
	}
	else
	{
		SetDlgItemText(hDlg, IDC_UNAME, "ÎÞ·¨½âÎö!");
	}
	AppendLog(hDlg, "\r\n\r\n");

}

static INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND && GET_WM_COMMAND_ID(wParam, lParam) == IDOK)
	{
		UnDecorateName(hwndDlg);
		return TRUE;
	}
	else if (uMsg == WM_CLOSE)
	{
		EndDialog(hwndDlg, 0);
		return TRUE;
	}

    return FALSE;
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEX wcx;
    wcx.cbSize = sizeof(wcx);
    if (!GetClassInfoEx(NULL, MAKEINTRESOURCE(32770), &wcx))
        return 0;

    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_ICO_MAIN));
    wcx.lpszClassName = _T("UnDecoraClass");
    if (!RegisterClassEx(&wcx))
        return 0;

    return DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)MainDlgProc);
}
