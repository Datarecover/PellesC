#pragma once

//-------------------------------------------------------------------------------------------------
// 调试工具
void TRACE(const char* pFmt, ...);

//-------------------------------------------------------------------------------------------------
// 通达信行情服务器
typedef struct TdxHost TdxHost, *PTdxHost;
struct TdxHost
{
	char sName[32];	// 服务器名称
	char sIP[16];	// 服务器IP地址
	short nPort;	// 端口
};

// 初始化服务器
BOOL TdxHostInit(void);

//-------------------------------------------------------------------------------------------------
// 股票交易市场: 深市,沪市
enum StockMarket { SM_ShenZhen=0, SM_ShangHai=1 };
typedef enum StockMarket StockMarket;

//-------------------------------------------------------------------------------------------------
// 市场和股票代号
typedef struct StockCode StockCode, *PStockCode;
#pragma pack(push, 1)
struct StockCode
{
	char Market;	// StockMarket(byte)
	char Code[7];	// 6位字符代码,1位结束符'\0'
};
#pragma pack(pop)

//-------------------------------------------------------------------------------------------------
// 指数、股票、权证
typedef struct Stock Stock, *PStock;
#pragma pack(push, 1)
struct Stock
{
	StockCode ssCode;		// 股票代码及交易市场
	char sName[10];			// 股票名称
	short nRatio;			// 价格放大比例
	DWORD InnerCode;		// 仅在内部使用的整型代码, 查找时用到,由CvtCode7转换
};
#pragma pack(pop)

// 股票数组
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

// 从服务器或本地文件读取当前所有交易股票列表
PStocks QueryAllStockInfo(void);

//-------------------------------------------------------------------------------------------------
// K线数据
typedef struct KLineData KLineData, *PKLineData;
struct KLineData 
{
	UINT Date;
	float Open, High, Low, Close;
	float Amount, Volume;
};

// K线数组
typedef struct KLineDatas KLineDatas, *PKLineDatas;
struct KLineDatas
{
	PStock pStock;
	int nSize;
	PKLineData pData;
};

void ReleaseKLineDatas(PKLineDatas pKD);

// 取特定日期区间日K线数据
PKLineDatas GetSpanKLineData(PStock pStock, UINT nStartDate, UINT nEndDate);
