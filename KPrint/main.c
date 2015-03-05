// K �ߴ�ӡС����

//#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <process.h>

#include "main.h"
#include "tdx.h"
#include "profile.h"

void PrintKLine(HWND hDlg, HDC hDC, PKLineDatas pKDA);

static INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR OnInitDialog(HWND hDlg);
static INT_PTR OnCommand(HWND hDlg, WORD nCode, WORD nID, HWND hCtrlWnd);
static INT_PTR OnPrint(HWND hDlg);
static INT_PTR OnTest(HWND hDlg);

static void __cdecl InitStockProc(void* dummy);

static HANDLE ghInstance;
static PStocks gpStocks;
static HGLOBAL ghDevMode, ghDevNames; 

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	WSADATA WSAData;
	WSAStartup(0x0202, &WSAData);

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASSEX wcx;
	wcx.cbSize = sizeof(wcx);
	if (!GetClassInfoEx(NULL, MAKEINTRESOURCE(32770), &wcx))
		return 0;

	TdxHostInit();

	ghInstance = hInstance;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_ICO_MAIN));
	wcx.lpszClassName = "KPrintClass";
	if (!RegisterClassEx(&wcx))
		return 0;

	int nRet = DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)MainDlgProc);

	WSACleanup();

	if (gpStocks != NULL)
		FreeStocks(gpStocks);

	if (ghDevMode != NULL) 
	    GlobalFree(ghDevMode); 
	if (ghDevNames != NULL) 
	    GlobalFree(ghDevNames); 

	return nRet;
}

static INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			return OnInitDialog(hDlg);

		case WM_COMMAND:
			return OnCommand(hDlg, HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

		case WM_CLOSE:
			EndDialog(hDlg, 0);
			return TRUE;
	}

	return FALSE;
}

static INT_PTR OnInitDialog(HWND hDlg)
{
	HWND hChild;

	// ����
	hChild = GetDlgItem(hDlg, IDC_CYCLE);
	ComboBox_AddString(hChild, "����");
	//ComboBox_AddString(hChild, "����");
	ComboBox_SetCurSel(hChild, 0);

	// ��ӡ����
	hChild = GetDlgItem(hDlg, IDC_CONTEXT);
	ComboBox_AddString(hChild, "ȫ��");
	ComboBox_AddString(hChild, "ʱ������");
	//ComboBox_AddString(hChild, "���һҳ");
	ComboBox_SetCurSel(hChild, 1);

	// ��Ʊ�б�
	char str[128];
	gpStocks = IniGetAllStocks();
	if (gpStocks == NULL)
	{
		MessageBox(hDlg, "��ȡ��Ʊ�б����!", "��ʾ", MB_OK);
		EndDialog(hDlg, 0);
		return FALSE;
	}
	
	hChild = GetDlgItem(hDlg, IDC_STOCK);
	for (int i=0; i<gpStocks->nSize; i++)
	{
		PStock pStock = gpStocks->pData + i;
		wsprintf(str, "%s%s %s", (pStock->ssCode.Market == 0 ? "sz" : "sh"), pStock->ssCode.Code, pStock->sName);
		ComboBox_AddString(hChild, str);
	}
	ComboBox_SetCurSel(hChild, 0);

	wsprintf(str, "���� %d ֧��Ʊ\n", gpStocks->nSize);
	SetDlgItemText(hDlg, IDC_MSG, str);

	return TRUE;
}

static INT_PTR OnContext(HWND hDlg, WORD nCode, HWND hBox)
{
	if (nCode == CBN_SELCHANGE)
	{
		int iCur = ComboBox_GetCurSel(hBox);
		BOOL bEnable = (iCur == 1);
		EnableWindow(GetDlgItem(hDlg, IDC_STARTDATE), bEnable);
		EnableWindow(GetDlgItem(hDlg, IDC_TO), bEnable);
		EnableWindow(GetDlgItem(hDlg, IDC_ENDDATE), bEnable);
	}

	return TRUE;
}

static INT_PTR OnCommand(HWND hDlg, WORD nCode, WORD nID, HWND hCtrlWnd)
{
	switch (nID)
	{
		case IDC_CONTEXT:
			return OnContext(hDlg, nCode, hCtrlWnd);

		case IDC_PRINT:
			return OnPrint(hDlg);
	}
	return FALSE;
}

static PKLineDatas GetKLineDatas(HWND hDlg)
{
	SetDlgItemText(hDlg, IDC_MSG, "���ڶ�ȡ��ʷ����, ���Ժ�...");

	int iSel = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_STOCK));
	PStock pStock = gpStocks->pData+iSel;
	if (pStock == NULL)
		return NULL;

	UINT iStartDate = GetDlgItemInt(hDlg, IDC_STARTDATE, NULL, FALSE);
	UINT iEndDate   = GetDlgItemInt(hDlg, IDC_ENDDATE,   NULL, FALSE);
	if (iEndDate <= iStartDate || iEndDate == 0)
		return NULL;

	PKLineDatas pKDA = GetSpanKLineData(pStock, iStartDate, iEndDate);
	return pKDA;
}

static HDC GetPrinterDC(HWND hDlg)
{
	HDC hPrintDC = NULL;

	PRINTDLGEX pd = {0};
	pd.lStructSize = sizeof(PRINTDLGEX);
	pd.hwndOwner = hDlg;
	pd.hDevMode = ghDevMode;
	pd.hDevNames = ghDevNames;
	pd.Flags = PD_RETURNDC | PD_HIDEPRINTTOFILE | PD_NOPAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE | PD_COLLATE;
	pd.nCopies = 1;
	pd.nStartPage = START_PAGE_GENERAL;

	HRESULT hResult = PrintDlgEx(&pd);
	if (hResult == S_OK && pd.dwResultAction == PD_RESULT_PRINT)
		hPrintDC = pd.hDC;
	else
	{
		if (pd.hDC != NULL) 
		    DeleteDC(pd.hDC);
	}

	if (ghDevMode == NULL)
		ghDevMode = pd.hDevMode;
	if (ghDevNames == NULL)
		ghDevNames = pd.hDevNames;

	return hPrintDC;
}

static INT_PTR OnPrint(HWND hDlg)
{
	PKLineDatas pKDA = GetKLineDatas(hDlg);
	if (pKDA == NULL)
	{
		SetDlgItemText(hDlg, IDC_MSG, "��ȡ��ʷ����ʧ��!");
		MessageBox(hDlg, "��ȡ��ʷ����ʧ��!", "����", MB_OK);
		return TRUE;
	}
	TRACE("��ȡ��ʷ���� %d ��!\n", pKDA->nSize);
	PKLineData pKD = pKDA->pData;
	for (int i=0; i<pKDA->nSize; i++)
	{
		TRACE("%08d %6.2f %6.2f %6.2f %6.2f %10.0f %12.0f\n", pKD->Date, pKD->Open, pKD->Low, pKD->High, pKD->Close, pKD->Volume, pKD->Amount);
		pKD++;
	}

	HDC hDC = GetPrinterDC(hDlg);
	if (hDC != NULL)
	{
		PrintKLine(hDlg, hDC, pKDA);
	    DeleteDC(hDC);
	}

	ReleaseKLineDatas(pKDA);
	return TRUE;
}

