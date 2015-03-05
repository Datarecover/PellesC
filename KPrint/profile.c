#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include "tdx.h"
#include "profile.h"

//-------------------------------------------------------------------------------------------------
// 局部变量
static BOOL bNotInitPath = TRUE;
static char sIniFile[256] = {0};

static void InitProfile(void)
{
	if (bNotInitPath)
	{
		GetModuleFileName(GetModuleHandle(0), sIniFile, 256);
		strcpy(strrchr(sIniFile, '.'), ".ini");
		bNotInitPath = FALSE;
	}
}

// 从INI读取服务器信息
BOOL IniGetHost(PTdxHost pHost)
{
	InitProfile();

	char buf[128];
	if (GetPrivateProfileString("Host", "HostIP", "202.102.249.111", buf, 127, sIniFile) == 0)
		return FALSE;
	strncpy(pHost->sIP, buf, 16);

	if (GetPrivateProfileString("Host", "HostName", "中原证券三", buf, 127, sIniFile) == 0)
		return FALSE;
	strncpy(pHost->sName, buf, 32);
	
	pHost->nPort = (short)GetPrivateProfileInt("Host", "HostPort", 7709, sIniFile);

	return TRUE;
}

// 写入股票列表
void IniWriteAllStocks(PStocks pStocks)
{
	InitProfile();

	char buf[128], ItemName[32];
	wsprintf(buf, "%d", pStocks->nSize);
	WritePrivateProfileString("Stock", "Count", buf, sIniFile);

	for (int i=0; i<pStocks->nSize; i++)
	{
		PStock pStock = pStocks->pData + i;
		wsprintf(buf, "%d %s%s %s", pStock->nRatio, (pStock->ssCode.Market == SM_ShenZhen ? "sz" : "sh"), pStock->ssCode.Code, pStock->sName);
		wsprintf(ItemName, "Item%04d", i);
		WritePrivateProfileString("Stock", ItemName, buf, sIniFile);
	}
}

static int CvtInnerCode(PStockCode pStockCode)
{
	register char *p = (char *)pStockCode;
	register int nCode = p[0];
	for (int i=1; i<7; i++)
		nCode = (nCode << 4) | (p[i] - '0');
	return nCode;
}

// 读入股票列表
PStocks IniGetAllStocks(void)
{
	InitProfile();

	char buf[128], ItemName[32];

	int nCount = (int)GetPrivateProfileInt("Stock", "Count", 0, sIniFile);
	if (nCount == 0)
		return NULL;

	PStocks pStocks = (PStocks)malloc(sizeof(Stocks));
	pStocks->nSize = nCount;
	pStocks->pData = (PStock)malloc(nCount * sizeof(Stock));

	for (int i=0; i<nCount; i++)
	{
		wsprintf(ItemName, "Item%04d", i);
		if (GetPrivateProfileString("Stock", ItemName, "", buf, 127, sIniFile) == 0)
		{
			free(pStocks->pData);
			free(pStocks);
			return NULL;
		}

		PStock pStock = pStocks->pData + i;
		pStock->ssCode.Market = (buf[3] == 'z' ? 0 : 1);
		strncpy(pStock->ssCode.Code, buf + 4, 6);
		strncpy(pStock->sName, buf + 11, 31);
		pStock->nRatio = (short)atoi(buf);
		pStock->InnerCode = (DWORD)CvtInnerCode(&pStock->ssCode);
	}

	return pStocks;
}


/*

static void LoadStockFromIni(StockArray* pArray, int *pIndex, Market market, const char* sSection, BOOL bStock)
{
	char sKey[32], buf[128], name1[12], name2[12];
	int nCount, nCode1, nCode2;
	float val;

	nCount = GetPrivateProfileInt(sSection, "Count", 0, iniFile);
	if (nCount == 0)
		return;

	for (int i = 0; i < nCount; i++)
	{
		sprintf(sKey, "Item%d", i + 1);
		
		if (GetPrivateProfileString(sSection, sKey, "", buf, 128, iniFile) == 0)
			continue;

		if (bStock)
		{
			sscanf(buf, "%06d %s %f %06d %s", &nCode1, name1, &val, &nCode2, name2);

			Stock *pStock = pArray->stocks + (*pIndex);
			Stock_Init(pStock, market, nCode1, name1);
			pStock->VolAll = val;
		}
		else
		{
			sscanf(buf, "%06d %s", &nCode1, name1);

			Stock *pStock = pArray->stocks + (*pIndex);
			Stock_Init(pStock, market, nCode1, name1);
		}

		*pIndex = *pIndex + 1;
	}
}

// 从 .ini 文件生成需要关注的股票列表
BOOL GetAllStockFromIniFile(StockArray* pArray, int nMaxCount)
{
	if(iniFile[0] == '\0')
		InitProfile();

	pArray->nSize = nMaxCount;
	pArray->stocks = (Stock *)Malloc(nMaxCount * sizeof(Stock));

	int iIndex = 0;
	LoadStockFromIni(pArray, &iIndex, ShangHai, "沪市指数", FALSE);
	LoadStockFromIni(pArray, &iIndex, ShenZhen, "深市指数", FALSE);
	LoadStockFromIni(pArray, &iIndex, ShenZhen, "深市权证", TRUE);
	LoadStockFromIni(pArray, &iIndex, ShangHai, "沪市权证", TRUE);

	pArray->nSize = iIndex;
	pArray->stocks = (Stock *)Realloc(pArray->stocks, iIndex * sizeof(Stock));

	return (iIndex > 0);
}

// 从INI读取服务器信息
int LoadHostFromIniFile(const char* sSection, HWND hComboBox)
{
	if(iniFile[0] == '\0')
		InitProfile();

	int nCount;
	nCount = GetPrivateProfileInt(sSection, "Count", 0, iniFile);
	if (nCount == 0)
		return 0;

	int nDefault = GetPrivateProfileInt(sSection, "Default", 0, iniFile);

	char buf[128], sKey[8];
	for (int i = 0; i < nCount; i++)
	{
		sprintf(sKey, "Item%d", i + 1);
		
		if (GetPrivateProfileString(sSection, sKey, "", buf, 128, iniFile) == 0)
			continue;

		int iItem = SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)buf);
		SendMessage(hComboBox, CB_SETITEMDATA, iItem, (LPARAM)(i+1));
		if(i+1 == nDefault)
			SendMessage(hComboBox, CB_SETCURSEL, iItem, 0);
	}

	return nDefault;
}

// 从 .ini 文件读取交易系统窗口标题
int GetTdxXiadanTitle(char* buf, int nMaxSize)
{
	if(iniFile[0] == '\0')
		InitProfile();

	return GetPrivateProfileString("通达信下单", "TdxXiadanTitle", "", buf, nMaxSize, iniFile);
}
*/
