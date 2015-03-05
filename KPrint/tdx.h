#pragma once

//-------------------------------------------------------------------------------------------------
// ���Թ���
void TRACE(const char* pFmt, ...);

//-------------------------------------------------------------------------------------------------
// ͨ�������������
typedef struct TdxHost TdxHost, *PTdxHost;
struct TdxHost
{
	char sName[32];	// ����������
	char sIP[16];	// ������IP��ַ
	short nPort;	// �˿�
};

// ��ʼ��������
BOOL TdxHostInit(void);

//-------------------------------------------------------------------------------------------------
// ��Ʊ�����г�: ����,����
enum StockMarket { SM_ShenZhen=0, SM_ShangHai=1 };
typedef enum StockMarket StockMarket;

//-------------------------------------------------------------------------------------------------
// �г��͹�Ʊ����
typedef struct StockCode StockCode, *PStockCode;
#pragma pack(push, 1)
struct StockCode
{
	char Market;	// StockMarket(byte)
	char Code[7];	// 6λ�ַ�����,1λ������'\0'
};
#pragma pack(pop)

//-------------------------------------------------------------------------------------------------
// ָ������Ʊ��Ȩ֤
typedef struct Stock Stock, *PStock;
#pragma pack(push, 1)
struct Stock
{
	StockCode ssCode;		// ��Ʊ���뼰�����г�
	char sName[10];			// ��Ʊ����
	short nRatio;			// �۸�Ŵ����
	DWORD InnerCode;		// �����ڲ�ʹ�õ����ʹ���, ����ʱ�õ�,��CvtCode7ת��
};
#pragma pack(pop)

// ��Ʊ����
typedef struct Stocks Stocks, *PStocks;
struct Stocks
{
	int nSize;
	PStock pData;
};

inline void FreeStocks(PStocks pStocks)
{
	if (pStocks->pData != NULL)
		free(pStocks->pData);
	pStocks->nSize = 0;
	pStocks->pData = 0;
}

// �ӷ������򱾵��ļ���ȡ��ǰ���н��׹�Ʊ�б�
PStocks QueryAllStockInfo(void);

//-------------------------------------------------------------------------------------------------
// K������
typedef struct KLineData KLineData, *PKLineData;
struct KLineData 
{
	UINT Date;
	float Open, High, Low, Close;
	float Amount, Volume;
};

// K������
typedef struct KLineDatas KLineDatas, *PKLineDatas;
struct KLineDatas
{
	PStock pStock;
	int nSize;
	PKLineData pData;
};

void ReleaseKLineDatas(PKLineDatas pKD);

// ȡ�ض�����������K������
PKLineDatas GetSpanKLineData(PStock pStock, UINT nStartDate, UINT nEndDate);
