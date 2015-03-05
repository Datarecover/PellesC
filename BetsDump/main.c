#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <stdlib.h>
#include <stdio.h>

// Zlib 解压缩函数
int uncompress(char *dest, int *destLen, const char *source, int sourceLen);
int compress2(char *dest, int *destLen, const char *source, int sourceLen, int level);
#pragma comment(lib, "ZLIB_PollesC.lib")

// 盘口数据文件头部
#pragma pack(push, 1)
typedef struct BetsHeader
{
	short nVer;				// 0x0103
	char sHostName[32];		// 服务器名称
	char sHostIP[32];		// 服务器IP
	short nPort;			// 端口
	int nDate;				// 日期
	int nStock;				// 股票总数
	int nInfoItemSize;		// 索引项大小
}BetsHeader;
#pragma pack(pop)

// 数据索引结构
#pragma pack(push, 1)
typedef struct StockInfo
{
	char Market;			// 市场, SM_ShenZhen=0, SM_ShangHai=1 
	char Code[7];			// 6位字符代码,1位结束符'\0'
	char sName[10];			// 股票名称
	BYTE nCategory;			// StockCategory 类型: 0=其它代码, 1=指数, 2=股票, 3=权证
	BYTE nRatio;			// 价格放大比例
	float preClose;			// 前收盘价
	int nBets5Count;		// 盘口数据
	DWORD dwRVA;			// Bets5Data数据在索引块后的位置
}StockInfo;
#pragma pack(pop)


// 五档盘口
typedef struct Bets5
{
	DWORD nIndex;		// 数据序号, 即今天发送的盘口序号
	DWORD nTime;		// 盘口时间, ("时间: %02d:%02d:%02d\r\n", nTime/10000, (nTime/100)%100, nTime%100);
	float Now;			// 现价
	float lastPrice;	// 最后一笔成交价
	float lastVol;		// 最后一笔成交量
	float High;			// 最高价
	float Low;			// 最低价
	float BuyP[5];		// 5个叫买价
	float BuyV[5];		// 5个叫买量
	float SellP[5];		// 5个叫卖价
	float SellV[5];		// 5个叫卖量
	float Volume;		// 总成交量
	float Amount;		// 总成交额
	float Inside;		// 内盘
	float Outside;		// 外盘
}Bets5;

#include "main.h"

#define ID_STOCK	1
#define ID_BETS		2

#define ID_SHOWSTOCKS	100
#define ID_SHOWBETS		200
#define STOCKWIDTH		200

static BOOL InitInstance(HINSTANCE hInstance);
static LRESULT WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);
static void Main_OnSize(HWND hwnd, UINT uFlag, int cx, int cy);
static void Main_OnCommand(HWND, int, HWND, UINT);
static LRESULT Main_OnNotify(HWND hwnd, int idCtrl, LPNMHDR pnmh);
static int Main_OnCreate(HWND);
static void OnFileOpen(HWND);
static LRESULT WINAPI AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
static void ShowStocks(HWND hwnd);
static void ShowBets(HWND hwnd, int idx);

static HANDLE ghInstance;
static HANDLE ghFile;
static BetsHeader gBetsHeader;
static StockInfo* gpStockInfos;

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	ghInstance = hInstance;
	ghFile = INVALID_HANDLE_VALUE;
	gpStockInfos = NULL;

	if (!InitInstance(hInstance))
		return 0;

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (ghFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ghFile);
		free(gpStockInfos);
	}

	return msg.wParam;
}

static BOOL InitInstance(HINSTANCE hInstance)
{
	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
	InitCommonControlsEx(&icc);

	WNDCLASS wc;
	wc.lpszClassName = "BetsDumpClass";
	wc.lpfnWndProc = MainWndProc;
	wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	wc.hInstance = ghInstance;
	wc.hIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(IDR_ICO_MAIN));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MNU_MAIN);
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	ATOM hAtom = RegisterClass(&wc);
	if (hAtom == 0)
		return FALSE;

	// Create Main Window
	HWND hMainWnd = CreateWindow((LPCTSTR)hAtom, "BetsDump Program", WS_OVERLAPPEDWINDOW,
		400, 400, 500, 300, NULL, NULL, hInstance, NULL);
	if (!hMainWnd)
		return FALSE;

	ShowWindow(hMainWnd, SW_SHOW);
	UpdateWindow(hMainWnd);

	return TRUE;
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CREATE:
			return Main_OnCreate(hwnd);

		case WM_SIZE:
			Main_OnSize(hwnd, (UINT) (wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;

		case WM_NOTIFY: 
			return Main_OnNotify(hwnd, (int) wParam, (LPNMHDR) lParam);

		case WM_COMMAND:
			Main_OnCommand(hwnd, (int)(LOWORD(wParam)), (HWND) (lParam), (UINT)HIWORD(wParam));
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static int Main_OnCreate(HWND hMainWnd)
{
	// Create Stock List Window
	HWND hStockWnd = CreateWindowEx(WS_EX_CLIENTEDGE | LVS_EX_FULLROWSELECT, WC_LISTVIEW, NULL,
		WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		0, 0, 0, 0, hMainWnd, (HMENU)ID_STOCK, ghInstance, NULL);
	if (!hStockWnd)
		return -1;
	
	ListView_SetExtendedListViewStyle(hStockWnd, LVS_EX_FULLROWSELECT);

	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
	int iCol = 0;

	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 64;
	lvc.pszText = (LPTSTR)"代码";
	SendMessage(hStockWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 64;
	lvc.pszText = (LPTSTR)"名称";
	SendMessage(hStockWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.fmt = LVCFMT_RIGHT;
	lvc.cx = 52;
	lvc.pszText = (LPTSTR)"盘口数";
	SendMessage(hStockWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	// Create Bets List Window
	HWND hBetsWnd = CreateWindowEx(WS_EX_CLIENTEDGE | LVS_EX_FULLROWSELECT, WC_LISTVIEW, NULL,
		WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		0, 0, 0, 0, hMainWnd, (HMENU)ID_BETS, ghInstance, NULL);
	if (!hBetsWnd)
		return -1;

	ListView_SetExtendedListViewStyle(hBetsWnd, LVS_EX_FULLROWSELECT);
	
	iCol = 0;

	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 64;
	lvc.pszText = (LPTSTR)"时间";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.fmt = LVCFMT_RIGHT;

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"序号";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"现价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"成交价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"成交量";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"最高价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"最低价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"分时现价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"分时均价";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"总成交量";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"总成交额";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"内盘";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"外盘";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyP[4]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyP[3]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyP[2]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyP[1]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyP[0]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyV[4]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyV[3]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyV[2]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyV[1]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"BuyV[0]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellP[0]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellP[1]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellP[2]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellP[3]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellP[4]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellV[0]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellV[1]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellV[2]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellV[3]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	lvc.cx = 48;
	lvc.pszText = (LPTSTR)"SellV[4]";
	SendMessage(hBetsWnd, LVM_INSERTCOLUMN, iCol++, (LPARAM)&lvc);

	return 0;
}

static void Main_OnSize(HWND hwnd, UINT uFlag, int cx, int cy)
{
	if (uFlag == SIZE_MINIMIZED)
		return;

	HWND hStockList = GetDlgItem(hwnd, ID_STOCK);
	MoveWindow(hStockList, 0, 0, STOCKWIDTH, cy, TRUE);

	HWND hBetsList = GetDlgItem(hwnd, ID_BETS);
	MoveWindow(hBetsList, STOCKWIDTH+4, 0, cx - STOCKWIDTH - 4, cy, TRUE);
}

static LRESULT Main_OnNotify(HWND hwnd, int idCtrl, LPNMHDR pnmh)
{
	if (idCtrl != ID_STOCK)
		return DefWindowProc(hwnd, WM_NOTIFY, (WPARAM)idCtrl, (LPARAM)pnmh);

	switch (pnmh->code)
	{
		case LVN_ITEMCHANGED:
			{
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW)pnmh;
/*
				char txt[256];
				sprintf(txt, "LVN_ITEMCHANGED iItem:%d, iSubItem:%d, uNewState:%08X, uOldState:%08X, uChanged:%08X, lParam:%08X\n", 
					pnmv->iItem, pnmv->iSubItem, pnmv->uNewState, pnmv->uOldState, pnmv->uChanged, pnmv->lParam);
				OutputDebugString(txt);
*/
				if (pnmv->uNewState == (LVIS_FOCUSED|LVIS_SELECTED))
					ShowBets(hwnd, (int)(pnmv->lParam));
			}
			break;
	}
	return 0;

/*
	const char *pMsg = NULL;
	switch (pnmh->code)
	{
		case NM_OUTOFMEMORY               : pMsg = "NM_OUTOFMEMORY         \n"; break;
		case NM_CLICK                     : pMsg = "NM_CLICK               \n"; break;
		case NM_DBLCLK                    : pMsg = "NM_DBLCLK              \n"; break;
		case NM_RETURN                    : pMsg = "NM_RETURN              \n"; break;
		case NM_RCLICK                    : pMsg = "NM_RCLICK              \n"; break;
		case NM_RDBLCLK                   : pMsg = "NM_RDBLCLK             \n"; break;
		case NM_SETFOCUS                  : pMsg = "NM_SETFOCUS            \n"; break;
		case NM_KILLFOCUS                 : pMsg = "NM_KILLFOCUS           \n"; break;
		case NM_CUSTOMDRAW                : pMsg = "NM_CUSTOMDRAW          \n"; break;
		case NM_HOVER                     : pMsg = "NM_HOVER               \n"; break;
		case NM_NCHITTEST                 : pMsg = "NM_NCHITTEST           \n"; break;
		case NM_KEYDOWN                   : pMsg = "NM_KEYDOWN             \n"; break;
		case NM_RELEASEDCAPTURE           : pMsg = "NM_RELEASEDCAPTURE     \n"; break;
		case NM_SETCURSOR                 : pMsg = "NM_SETCURSOR           \n"; break;
		case NM_CHAR                      : pMsg = "NM_CHAR                \n"; break;
		case NM_TOOLTIPSCREATED           : pMsg = "NM_TOOLTIPSCREATED     \n"; break;
		case NM_LDOWN                     : pMsg = "NM_LDOWN               \n"; break;
		case NM_RDOWN                     : pMsg = "NM_RDOWN               \n"; break;
		case NM_THEMECHANGED              : pMsg = "NM_THEMECHANGED        \n"; break;
		case NM_FONTCHANGED               : pMsg = "NM_FONTCHANGED         \n"; break;
		//case NM_CUSTOMTEXT                : pMsg = "NM_CUSTOMTEXT          \n"; break;
		case NM_TVSTATEIMAGECHANGING      : pMsg = "NM_TVSTATEIMAGECHANGING\n"; break;
		case LVN_ITEMCHANGING             : pMsg = "LVN_ITEMCHANGING       \n"; break; 
		case LVN_ITEMCHANGED              : pMsg = "LVN_ITEMCHANGED        \n"; break; 
		case LVN_INSERTITEM               : pMsg = "LVN_INSERTITEM         \n"; break; 
		case LVN_DELETEITEM               : pMsg = "LVN_DELETEITEM         \n"; break; 
		case LVN_DELETEALLITEMS           : pMsg = "LVN_DELETEALLITEMS     \n"; break; 
		case LVN_BEGINLABELEDITA          : pMsg = "LVN_BEGINLABELEDITA    \n"; break; 
		case LVN_BEGINLABELEDITW          : pMsg = "LVN_BEGINLABELEDITW    \n"; break; 
		case LVN_ENDLABELEDITA            : pMsg = "LVN_ENDLABELEDITA      \n"; break; 
		case LVN_ENDLABELEDITW            : pMsg = "LVN_ENDLABELEDITW      \n"; break; 
		case LVN_COLUMNCLICK              : pMsg = "LVN_COLUMNCLICK        \n"; break; 
		case LVN_BEGINDRAG                : pMsg = "LVN_BEGINDRAG          \n"; break; 
		case LVN_BEGINRDRAG               : pMsg ="LVN_BEGINRDRAG          \n"; break;
		case LVN_ODCACHEHINT              : pMsg ="LVN_ODCACHEHINT         \n"; break;
		case LVN_ODFINDITEMA              : pMsg ="LVN_ODFINDITEMA         \n"; break;
		case LVN_ODFINDITEMW              : pMsg ="LVN_ODFINDITEMW         \n"; break;
		case LVN_ITEMACTIVATE             : pMsg ="LVN_ITEMACTIVATE        \n"; break;
		case LVN_ODSTATECHANGED           : pMsg ="LVN_ODSTATECHANGED      \n"; break;
		//case LVN_HOTTRACK                 : pMsg ="LVN_HOTTRACK            \n"; break;
		case LVN_GETDISPINFOA             : pMsg ="LVN_GETDISPINFOA        \n"; break;
		case LVN_GETDISPINFOW             : pMsg ="LVN_GETDISPINFOW        \n"; break;
		case LVN_SETDISPINFOA             : pMsg ="LVN_SETDISPINFOA        \n"; break;
		case LVN_SETDISPINFOW             : pMsg ="LVN_SETDISPINFOW        \n"; break;
		case LVN_KEYDOWN                  : pMsg ="LVN_KEYDOWN             \n"; break;
		case LVN_MARQUEEBEGIN             : pMsg ="LVN_MARQUEEBEGIN        \n"; break;
		case LVN_GETINFOTIPA              : pMsg ="LVN_GETINFOTIPA         \n"; break;
		case LVN_GETINFOTIPW              : pMsg ="LVN_GETINFOTIPW         \n"; break;
		case LVN_INCREMENTALSEARCHA       : pMsg ="LVN_INCREMENTALSEARCHA  \n"; break;
		case LVN_INCREMENTALSEARCHW       : pMsg ="LVN_INCREMENTALSEARCHW  \n"; break;
		case LVN_BEGINSCROLL              : pMsg ="LVN_BEGINSCROLL         \n"; break;
		case LVN_ENDSCROLL                : pMsg ="LVN_ENDSCROLL           \n"; break;
		case LVN_COLUMNDROPDOWN           : pMsg ="LVN_COLUMNDROPDOWN      \n"; break;
		case LVN_COLUMNOVERFLOWCLICK      : pMsg ="LVN_COLUMNOVERFLOWCLICK \n"; break;
		case LVN_LINKCLICK                : pMsg ="LVN_LINKCLICK           \n"; break;
		case LVN_GETEMPTYMARKUP           : pMsg ="LVN_GETEMPTYMARKUP      \n"; break;
		default:
		{
			char txt[128];
			sprintf(txt, "%08X\n", pnmh->code);
			OutputDebugString(txt);
		}
		break;
	}

	if (pMsg) OutputDebugString(pMsg);
*/
}

static void Main_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
		case IDM_ABOUT:
			DialogBox(ghInstance, MAKEINTRESOURCE(DLG_ABOUT), hwnd, (DLGPROC)AboutDlgProc);
			break;

		case IDM_FILE_OPEN:
			OnFileOpen(hwnd);
			break;

		case ID_SHOWSTOCKS:
			ShowStocks(hwnd);
			break;

	}
}

static LRESULT CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			/*
			 * Nothing special to initialize.
			 */
			return TRUE;

		case WM_COMMAND:
			switch (wParam)
			{
				case IDOK:
				case IDCANCEL:
					/*
					 * OK or Cancel was clicked, close the dialog.
					 */
					EndDialog(hDlg, TRUE);
					return TRUE;
			}
			break;
	}

	return FALSE;
}

static void OnFileOpen(HWND hMainWnd)
{
	TCHAR fileName[256];
	fileName[0] = 0;

	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hMainWnd;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = sizeof(fileName);
	ofn.lpstrFilter = "盘口数据文件(*.bt5)\0*.bt5\0所有文件(*.*)\0*.*\0";
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (!GetOpenFileName(&ofn))
		return;

	HANDLE hFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	if (ghFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ghFile);
		free(gpStockInfos);
		gpStockInfos = NULL;
	}

	ghFile = hFile;

	PostMessage(hMainWnd, WM_COMMAND, ID_SHOWSTOCKS, (LPARAM)hMainWnd);
}

static void ShowStocks(HWND hwnd)
{
	HWND hStockList = GetDlgItem(hwnd, ID_STOCK);
	SendMessage(hStockList, LVM_DELETEALLITEMS, 0, 0);

	HWND hBetsList = GetDlgItem(hwnd, ID_BETS);
	SendMessage(hBetsList, LVM_DELETEALLITEMS, 0, 0);

	DWORD nBytes;

	SetFilePointer(ghFile, 0, NULL, FILE_BEGIN);

	// 读入文件头部
	ReadFile(ghFile, &gBetsHeader, sizeof(BetsHeader), &nBytes, NULL);
	if (gBetsHeader.nVer != 0x0103 || gBetsHeader.nInfoItemSize != sizeof(StockInfo))
	{
		CloseHandle(ghFile);
		ghFile = INVALID_HANDLE_VALUE;
		MessageBox(hwnd, "盘口数据版本有误!", "错误", MB_OK);
		return;
	}

	HCURSOR hPrev = SetCursor(LoadCursor(NULL, IDC_WAIT));
	SendMessage(hStockList, WM_SETREDRAW, FALSE, 0);

	gpStockInfos = (StockInfo*)malloc(gBetsHeader.nStock * sizeof(StockInfo));
	ReadFile(ghFile, gpStockInfos, gBetsHeader.nStock * sizeof(StockInfo), &nBytes, NULL);

	StockInfo* pStockInfo = gpStockInfos;

	char txt[256];
	LVITEM lvi;
	lvi.mask = LVIF_PARAM|LVIF_TEXT;
	lvi.pszText = txt;

	for (int i=0; i<gBetsHeader.nStock; i++)
	{
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.lParam = (LPARAM)i;
		sprintf(txt, "%s%s", (pStockInfo->Market == 0 ? "sz" : "sh"), pStockInfo->Code);
		SendMessage(hStockList, LVM_INSERTITEM, 0, (LPARAM)&lvi);

		strncpy(txt, pStockInfo->sName, 10);
		txt[8] = 0;
		lvi.iSubItem = 1;
		SendMessage(hStockList, LVM_SETITEMTEXT, i, (LPARAM)&lvi);

		sprintf(txt, "%d", pStockInfo->nBets5Count);
		lvi.iSubItem = 2;
		SendMessage(hStockList, LVM_SETITEMTEXT, i, (LPARAM)&lvi);

		pStockInfo++;
	}

	SendMessage(hStockList, WM_SETREDRAW, TRUE, 0);

	SetCursor(hPrev);
}

static void ShowBets(HWND hwnd, int idx)
{
	HWND hBetsList = GetDlgItem(hwnd, ID_BETS);
	SendMessage(hBetsList, LVM_DELETEALLITEMS, 0, 0);

	if (ghFile == INVALID_HANDLE_VALUE)
		return;

	StockInfo* pStockInfo = gpStockInfos + idx;
	char ssCode[8];
	int nBetsCount;
	DWORD nBytes;

	SetFilePointer(ghFile, sizeof(BetsHeader) + gBetsHeader.nStock * sizeof(StockInfo) + pStockInfo->dwRVA, NULL, FILE_BEGIN);
	ReadFile(ghFile, ssCode, 8, &nBytes, NULL);
	ReadFile(ghFile, &nBetsCount, sizeof(int), &nBytes, NULL);

	Bets5 bets5;

	int iCol = 0;
	int iRow = 0;
	int nLastIndex = 9999;
	char txt[256];
	LVITEM lvi;
	lvi.mask = LVIF_TEXT;
	lvi.pszText = txt;

	HCURSOR hPrev = SetCursor(LoadCursor(NULL, IDC_WAIT));
	SendMessage(hBetsList, WM_SETREDRAW, FALSE, 0);

	for(int i=0; i<pStockInfo->nBets5Count; i++)
	{
		ReadFile(ghFile, &bets5, sizeof(Bets5), &nBytes, NULL);

		int nInsertCnt = bets5.nIndex - nLastIndex - 1;
		if (nInsertCnt > 0)
		{
			for (int k=0; k<nInsertCnt; k++)
			{
				lvi.iItem = iRow;
				lvi.iSubItem = 0;
				sprintf(txt, "--:--:--");
				SendMessage(hBetsList, LVM_INSERTITEM, 0, (LPARAM)&lvi);

				lvi.iSubItem = 1;
				sprintf(txt, "%d", nLastIndex + k + 1);
				SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);
				iRow++;
			}
		}

		iCol = 0;

		// 时间
		lvi.iItem = iRow;
		lvi.iSubItem = iCol++;
		sprintf(txt, "%02d:%02d:%02d", bets5.nTime/10000, (bets5.nTime/100)%100, bets5.nTime%100);
		SendMessage(hBetsList, LVM_INSERTITEM, 0, (LPARAM)&lvi);

		// 序号
		lvi.iSubItem = iCol++;
		if (nLastIndex != bets5.nIndex)
		{
			nLastIndex = bets5.nIndex;
			sprintf(txt, "%d", bets5.nIndex);
			SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);
		}

		// 现价
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.Now);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 成交价
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.lastPrice);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 成交量
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.lastVol);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 最高价
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.High);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 最低价
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.Low);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 分时现价
		lvi.iSubItem = iCol++;
		//sprintf(txt, "%.3f", bets5.mnPrice);
		//SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 分时均价
		lvi.iSubItem = iCol++;
		//sprintf(txt, "%.3f", bets5.avPrice);
		//SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 总成交量
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.Volume);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 总成交额
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.Amount);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 内盘
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.Inside);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// 外盘
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.Outside);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyP[4]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.BuyP[4]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyP[3]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.BuyP[3]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyP[2]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.BuyP[2]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyP[1]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.BuyP[1]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyP[0]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.BuyP[0]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyV[4]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.BuyV[4]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyV[3]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.BuyV[3]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyV[2]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.BuyV[2]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyV[1]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.BuyV[1]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// BuyV[0]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.BuyV[0]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellP[0]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.SellP[0]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellP[1]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.SellP[1]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellP[2]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.SellP[2]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellP[3]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.SellP[3]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellP[4]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.3f", bets5.SellP[4]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellV[0]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.SellV[0]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellV[1]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.SellV[1]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellV[2]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.SellV[2]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellV[3]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.SellV[3]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		// SellV[4]
		lvi.iSubItem = iCol++;
		sprintf(txt, "%.0f", bets5.SellV[4]);
		SendMessage(hBetsList, LVM_SETITEMTEXT, iRow, (LPARAM)&lvi);

		iRow++;
	}

	// 调整列宽
	HWND hHeaderCtrl = (HWND)SendMessage(hBetsList, LVM_GETHEADER, 0, 0);
	int nCol = (int)SendMessage(hHeaderCtrl, HDM_GETITEMCOUNT, 0, 0);
	
	for(int iCol=0; iCol<nCol; iCol++)
	{
		int nColWidth, nHdrWidth;
		SendMessage(hBetsList, LVM_SETCOLUMNWIDTH, iCol, LVSCW_AUTOSIZE);
		nColWidth = SendMessage(hBetsList, LVM_GETCOLUMNWIDTH, iCol, 0);

		SendMessage(hBetsList, LVM_SETCOLUMNWIDTH, iCol, LVSCW_AUTOSIZE_USEHEADER);
		nHdrWidth = SendMessage(hBetsList, LVM_GETCOLUMNWIDTH, iCol, 0);

		SendMessage(hBetsList, LVM_SETCOLUMNWIDTH, iCol, max(nColWidth, nHdrWidth));
	}

	SendMessage(hBetsList, WM_SETREDRAW, TRUE, 0);
	SetCursor(hPrev);
}
