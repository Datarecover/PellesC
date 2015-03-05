#define WIN32_LEAN_AND_MEAN
#define WIN32_DEFAULT_LIBS
#include <windows.h>
#include <winsock2.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "tdx.h"
#include "profile.h"

// Zlib ��ѹ������
extern int compress2(BYTE *dest, int *destLen, const BYTE *source, DWORD sourceLen, int level);
extern int uncompress(BYTE *dest, int *destLen, const BYTE *source, DWORD sourceLen);
#pragma comment(lib, "ZLIB_PollesC.lib")

//-------------------------------------------------------------------------------------------------
// ���Թ���
#define BUFLEN 1024
void TRACE(const char* pFmt, ...)
{
	va_list	args;
	va_start(args, pFmt);

	char sMsg[BUFLEN];
	vsnprintf(sMsg, BUFLEN, pFmt, args);

	OutputDebugString(sMsg);

	va_end(args);
}

//-------------------------------------------------------------------------------------------------
// ͨ�������������
static TdxHost gTdxHost;

BOOL TdxHostInit(void)
{
	return IniGetHost(&gTdxHost);
}

//-------------------------------------------------------------------------------------------------
// �ͷ�K������
void ReleaseKLineDatas(PKLineDatas pKD)
{
	if (pKD != NULL)
	{
		if (pKD->pData != NULL)
			free(pKD->pData);
		free(pKD);
	}
}

//-------------------------------------------------------------------------------------------------
// ��������ͷ��
typedef struct SendHeader SendHeader, *PSendHeader;
#pragma pack(push, 1)
struct SendHeader
{
	char cmdSig;	// = 0x0C
	WORD cmdObj;	// ����Ͷ���
	WORD cmdIndex;	// ��������
	char rev;		// = ???
	WORD oSize;	// ͷ�����ָ�����С�����ָ���徭��ѹ������ָѹ����Ĵ�С��
	WORD nSize;	// ָ�����Сԭʼ��С, ���ָ���徭��ѹ������ָѹ��ǰ�Ĵ�С��
};
#pragma pack(pop)

static void SendHeader_Init(PSendHeader pHeader, WORD size, WORD obj, WORD index)
{
	pHeader->cmdSig = 0x0C;
	pHeader->cmdIndex = index;
	pHeader->cmdObj = obj;
	pHeader->rev = (char)0;
	pHeader->nSize = size;
	pHeader->oSize = size;
}

//-------------------------------------------------------------------------------------------------
// �����������ݵ�ͷ��
typedef struct RecvHeader RecvHeader, *PRecvHeader;
#pragma pack(push, 1)
struct RecvHeader 
{
	int recvSig;	// = 0x0074BCC1
	char cmdSig;	// = 0x0C
	WORD cmdObj;	// ����Ͷ���
	WORD cmdIndex;	// ��������
	char rev;		// = ???
	WORD cmdID;		// = ���ݶ�Ӧ�������
	WORD oSize;		// ͷ�����ָ�����С�����ָ���徭��ѹ������ָѹ����Ĵ�С��
	WORD nSize;		// ָ�����Сԭʼ��С, ���ָ���徭��ѹ������ָѹ��ǰ�Ĵ�С��
};
#pragma pack(pop)


//-------------------------------------------------------------------------------------------------
// ͨ��������ͬ����ѯ
typedef struct TdxQuery TdxQuery, *PTdxQuery;
struct TdxQuery
{
	SOCKET hSocket;			// ���Ӿ��
	int nRecvSize;			// �ѽ������ݳ���
	BYTE *pRecvBuf;			// �ѽ�������
};

static BOOL QueryConnect(PTdxQuery pQuery)
{
	// ���ӷ�����...
	pQuery->hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (pQuery->hSocket == INVALID_SOCKET)
		return FALSE;
		
	struct sockaddr_in srvr;
	srvr.sin_family = AF_INET;
	srvr.sin_port = htons(gTdxHost.nPort);
	srvr.sin_addr.s_addr = inet_addr(gTdxHost.sIP);
		
	if (connect(pQuery->hSocket, (LPSOCKADDR)&srvr, sizeof(srvr)) == SOCKET_ERROR)
	{
		closesocket(pQuery->hSocket);
		pQuery->hSocket = INVALID_SOCKET;
		return FALSE;
	}

	pQuery->nRecvSize = 0;
	pQuery->pRecvBuf = 0;
	return TRUE;
}

static void QueryClose(PTdxQuery pQuery)
{
	if (pQuery->hSocket != INVALID_SOCKET)
		closesocket(pQuery->hSocket);
	pQuery->hSocket = INVALID_SOCKET;
	free(pQuery->pRecvBuf);
}

static int QueryCmd(PTdxQuery pQuery, void* pCmdData, int nLen)
{
	assert(pQuery->hSocket != INVALID_SOCKET);
	assert(pCmdData != NULL);
	assert(nLen > 0);

	// ��������
	int nTotalCount = send(pQuery->hSocket, (const char *)pCmdData, nLen, 0);
	if (nTotalCount == SOCKET_ERROR)
		return -1;

	// �����������ݵ�ͷ��
	RecvHeader recvHeader;
	nTotalCount = recv(pQuery->hSocket, (char *)&recvHeader, sizeof(recvHeader), 0);
	if (nTotalCount == SOCKET_ERROR || recvHeader.recvSig != 0x0074CBB1)
		return -1;

	// ������������
	nTotalCount = 0;
	pQuery->nRecvSize = recvHeader.oSize;
	pQuery->pRecvBuf = (BYTE *)realloc(pQuery->pRecvBuf, recvHeader.oSize);
	while (nTotalCount < recvHeader.oSize)
	{
		int curRecvCount = recv(pQuery->hSocket, (char *)(pQuery->pRecvBuf + nTotalCount), recvHeader.oSize - nTotalCount, 0);
		if (curRecvCount == 0 || curRecvCount == SOCKET_ERROR)
			return -1;
		nTotalCount += curRecvCount;
	}

	// ��Ҫʱ��ѹ������
	if ((recvHeader.cmdSig & 0x10) == 0x10)
	{
		int newSize = recvHeader.nSize + 128;
		BYTE *pDestBuf = (BYTE *)malloc(newSize);
		int ret = uncompress(pDestBuf, &newSize, pQuery->pRecvBuf, recvHeader.oSize);
		assert(ret == 0);
		if (ret == 0 && newSize == recvHeader.nSize)
		{
			pQuery->nRecvSize = newSize;
			free(pQuery->pRecvBuf);
			pQuery->pRecvBuf = pDestBuf;
		}
		else
		{
			free(pDestBuf);
			pQuery->nRecvSize = 0;
			return -1;
		}
	}

	return pQuery->nRecvSize;
}

//-------------------------------------------------------------------------------------------------
// 044E ȡ�г����׹�Ʊ����
typedef struct Cmd_044E Cmd_044E, *PCmd_044E;
#pragma pack(push, 1)
struct Cmd_044E
{
	SendHeader header;
	WORD cmd;
	short market;
	UINT date;
};
#pragma pack(pop)
#define Cmd_044E_SIZE (sizeof(Cmd_044E))

static void Cmd_044E_Init(PCmd_044E pCmd, StockMarket aMarket, UINT aDate, WORD cmdObj, WORD cmdIndex)
{
	pCmd->cmd = 0x044E;
	pCmd->market = (short)aMarket;
	pCmd->date = aDate;
	SendHeader_Init(&pCmd->header, 0x08, cmdObj, cmdIndex);
}

//-------------------------------------------------------------------------------------------------
// 0450 ȡ��ǰ���׹�Ʊ���ƴ����
typedef struct Cmd_0450 Cmd_0450, *PCmd_0450;
#pragma pack(push, 1)
struct Cmd_0450
{
	SendHeader header;
	WORD cmd;
	short market;
	WORD istart;
};
#pragma pack(pop)
#define Cmd_0450_SIZE (sizeof(Cmd_0450))

static void Cmd_0450_Init(PCmd_0450 pCmd, StockMarket aMarket, WORD iStart)
{
	pCmd->cmd = 0x0450;
	pCmd->market = (short)aMarket;
	pCmd->istart = iStart;
	SendHeader_Init(&pCmd->header, 0x06, 0, 0);
}

// ���˹�Ʊ
static BOOL FilterStock(StockMarket market, char cCode[6])
{
	int nCode = 0;
	for (int i=0; i<6; i++)
		nCode = nCode * 10 + (cCode[i] - '0');

	// ����Ӧ�� 000000 - 999999 ��
	assert(nCode >= 0 && nCode <= 999999);

	if(market == SM_ShangHai)
	{
		// 000xxx ����ָ��, 999999=��֤��ָ, 999998=A��ָ��, 999997=B��ָ��
		if (nCode < 1000 || nCode >=999997)
			return TRUE;

		// 60xxxx ���й�Ʊ
		else if (nCode >= 600000 && nCode <= 609999)
			return TRUE;

		// 580xxx ����Ȩ֤
		else if (nCode >= 580000 && nCode <= 580999)
			return TRUE;
	}

	else if(market == SM_ShenZhen)
	{
		// 399xxx ����ָ��
		if (nCode > 399000 && nCode <= 399999)
			return TRUE;

		// 00xxxx ����A��Ʊ
		else if (nCode <= 9999)
			return TRUE;

		// 03xxxx ����Ȩ֤
		else if (nCode >= 30000 && nCode <= 39999)
			return TRUE;

		// 30xxxx ���д�ҵ��
		else if (nCode >= 300001 && nCode <= 309999)
			return TRUE;
	}

	return FALSE;
}

static int CvtInnerCode(PStockCode pStockCode)
{
	register char *p = (char *)pStockCode;
	register int nCode = p[0];
	for (int i=1; i<7; i++)
		nCode = (nCode << 4) | (p[i] - '0');
	return nCode;
}

// ת������ӵ���Ʊ�����б����� 0450
static int CvtToStocks_0450(StockMarket aMarket, PStocks pStocks, void *pRecvBuf, int nRecvSize)
{
	char* p = (char*)pRecvBuf;

	// ���ζ�ȡ����
	short nRead = *(short*)p; p += 2;

	// ������ nRead ��
	pStocks->pData = (PStock)realloc(pStocks->pData, sizeof(Stock) * (pStocks->nSize + nRead));

	for(short i=0; i<nRead; i++)
	{
		if (FilterStock(aMarket, p))
		{
			PStock pStock = pStocks->pData + pStocks->nSize;
			memset(pStock, 0, sizeof(Stock));

			pStock->ssCode.Market = (BYTE)aMarket;
			memcpy(pStock->ssCode.Code, p, 6);
			pStock->ssCode.Code[6] = 0;
			memcpy(pStock->sName, p+8, 8);
			pStock->sName[8] = '\0';

			pStock->InnerCode = CvtInnerCode(&pStock->ssCode);
			pStock->nRatio = *(BYTE *)(p + 20);
			//pStock->preClose = *(float *)(p + 21);

			//pStocks->pData[pStocks->nSize] = pStock;
			pStocks->nSize += 1;
		}
		p += 29;	// ÿ����Ʊ29�ֽ�
	}

	return nRead;
}

int __cdecl cmpStock(const void *elem1, const void *elem2)
{
	PStock pStock1 = (PStock)elem1;
	PStock pStock2 = (PStock)elem2;
	return pStock1->InnerCode - pStock2->InnerCode;
}

// ��ǰ���׹�Ʊ�б�, bExtra ��ʾ�Ƿ��ȡ�������ݺ͹ɱ���Ǩ����
static int GetAllStockInfo(PTdxQuery pQuery, PStocks pStocks)
{
	Cmd_044E Cmd044E;
	Cmd_0450 Cmd0450;

	int nRead;	// �Ѷ�ȡ�Ĺ�Ʊ����,��һ��������ӵ�����ĸ���

	// ȡ���й�Ʊ
	Cmd_044E_Init(&Cmd044E, SM_ShangHai, 0, 0, 0);
	if(QueryCmd(pQuery, (void *)&Cmd044E, Cmd_044E_SIZE) < 0)
		return -1;

	int nStockSH = *(short*)(pQuery->pRecvBuf);	// ���й�Ʊ��

	nRead = 0;
	while(nRead < nStockSH)
	{
		Cmd_0450_Init(&Cmd0450, SM_ShangHai, (WORD)nRead);
		if(QueryCmd(pQuery, &Cmd0450, Cmd_0450_SIZE) < 0)
			return -1;
		nRead += CvtToStocks_0450(SM_ShangHai, pStocks, pQuery->pRecvBuf, pQuery->nRecvSize);
	}

	// ȡ���й�Ʊ
	Cmd_044E_Init(&Cmd044E, SM_ShenZhen, 0, 0, 0);
	if(QueryCmd(pQuery, (void *)&Cmd044E, Cmd_044E_SIZE) < 0)
		return -1;

	int nStockSZ = *(short*)(pQuery->pRecvBuf);	// ���й�Ʊ��

	nRead = 0;
	while(nRead < nStockSZ)
	{
		Cmd_0450_Init(&Cmd0450, SM_ShenZhen, (WORD)nRead);
		if(QueryCmd(pQuery, &Cmd0450, Cmd_0450_SIZE) < 0)
			return -1;

		nRead += CvtToStocks_0450(SM_ShenZhen, pStocks, pQuery->pRecvBuf, pQuery->nRecvSize);
	}

	// ��������
	pStocks->pData = (PStock)realloc(pStocks->pData, sizeof(Stock) * pStocks->nSize);
	qsort(pStocks->pData, pStocks->nSize, sizeof(Stock), cmpStock); 

	return pStocks->nSize;
}

// �ӷ�������ȡ��ǰ���н��׹�Ʊ�б�
PStocks QueryAllStockInfo(void)
{
	TdxQuery query;

	// ���ӷ�����...
	if (!QueryConnect(&query))
		return NULL;

	PStocks pStocks = (PStocks)malloc(sizeof(Stocks));
	pStocks->nSize = 0;
	pStocks->pData = 0;
	GetAllStockInfo(&query, pStocks);

	// �ر�����
	QueryClose(&query);

	return pStocks;
}

//-------------------------------------------------------------------------------------------------
// ȡ�ض���������K������
typedef struct Cmd_0FC8 Cmd_0FC8, *PCmd_0FC8;
#pragma pack(push, 1)
struct Cmd_0FC8
{
	SendHeader header;
	WORD cmd;
	short market;	// StockMarket(word)
	char code[6];	// 6λ�ַ�����,�޽�����'\0'
	UINT nStartDate, nEndDate;
	short cycle;		// ����ʱ�� 1������=7, 5������=0, 15������=1, 30������=2, 60������=3, ����=4, ����=5
};
#pragma pack(pop)

static void Cmd_0FC8_Init(PCmd_0FC8 pCmd, PStock pStock, UINT iStartDate, UINT iEndDate)
{
	pCmd->cmd = 0x0FC8;
	pCmd->market = (short)pStock->ssCode.Market;
	strcpy(pCmd->code, pStock->ssCode.Code);
	pCmd->nStartDate = iStartDate;
	pCmd->nEndDate = iEndDate;
	pCmd->cycle = 4;
	SendHeader_Init(&pCmd->header, 20, 0, 0);
};
#define Cmd_0FC8_SIZE (sizeof(Cmd_0FC8))

// ת��ΪK������ 0FC8
PKLineDatas CvtToKLineData_0FC8(PStock pStock, void *pRecvBuf, int nRecvSize)
{
	if(nRecvSize == 0)
		return NULL;

	PKLineDatas pKDA = (PKLineDatas)malloc(sizeof(KLineDatas));
	pKDA->pStock = pStock;

	// ������ȡ������
	BYTE *p = (BYTE *)pRecvBuf;
	short klc = *(short *)p; p += 2;
	assert(klc == 4);

	int nSize = *(int *)p; p += 4;
	pKDA->nSize = nSize / 32;	// ÿ������32�ֽ�
	pKDA->pData = (PKLineData)malloc(sizeof(KLineData) * pKDA->nSize);

	int tmp;
	float rate = (pStock->nRatio == 2 ? 100.0f : 1000.0f);

	for (int i=0; i<pKDA->nSize; i++)
	{
		PKLineData pKD = pKDA->pData + i;

		pKD->Date = *(UINT *)p; p += 4;
		tmp = *(int *)p; p += 4;
		pKD->Open = tmp / rate;
		tmp = *(int *)p; p += 4;
		pKD->High = tmp / rate;
		tmp = *(int *)p; p += 4;
		pKD->Low = tmp / rate;
		tmp = *(int *)p; p += 4;
		pKD->Close = tmp / rate;
		pKD->Amount = *(float *)p; p += 4;
		tmp = *(int *)p; p += 4;
		pKD->Volume = (float)tmp;
		tmp = *(int *)p; p += 4;
	}

	return pKDA;
}

// ȡ�ض�����������K������
PKLineDatas GetSpanKLineData(PStock pStock, UINT nStartDate, UINT nEndDate)
{
	// ��ʼ������
	Cmd_0FC8 Cmd;
	Cmd_0FC8_Init(&Cmd, pStock, nStartDate, nEndDate);

	TdxQuery query;

	// ���ӷ�����...
	if (!QueryConnect(&query))
		return NULL;

	// ��������
	if (QueryCmd(&query, (void*)&Cmd, Cmd_0FC8_SIZE) < 0)
	{
		// �ر�����
		QueryClose(&query);
		return NULL;
	}

	// ת������
	PKLineDatas pKDA = CvtToKLineData_0FC8(pStock, query.pRecvBuf, query.nRecvSize);

	// �ر�����
	QueryClose(&query);

	return pKDA;
}

