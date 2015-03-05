#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "tdx.h"
#include "profile.h"

typedef struct PrintInfo PrintInfo, *PPrintInfo;
struct PrintInfo
{
	HDC hDC;				// 打印句柄
	RECT rcPaper;			// 纸面区域, 单位为0.01mm
	RECT rcPrint;			// 打印区域, 单位为0.01mm
	int nDateTimeHeight;	// 时间区域高度, 单位为0.01mm
	int nVolHeight;			// 成交量区域高度, 单位为0.01mm
	int nTextHeight;		// 文字高度,  单位为0.01mm
	int nPriceWidth;		// 价格区域宽度, 单位为0.01mm
	int nItemWidth;			// 每个K线净宽, 单位为0.01mm
	int nItemGap;			// 每个K线间隔, 单位为0.01mm
	PKLineDatas pKDA;		// K线数据
	int nPageItem;			// 每页显示个数
	int nPageCount;			// 总页数
	HPEN hBorderPen;		// 边框画笔
	HPEN hDotPen;			// 虚线画笔
	HPEN hNormalPen;		// 普通画笔
	HBRUSH hBrushN;			// 阳线画刷
	HBRUSH hBrushX;			// 阴线画刷
};

static void PreparePrintInfo(PPrintInfo pPrintInfo, HDC hDC, PKLineDatas pKDA)
{
	// 打印句柄
	pPrintInfo->hDC = hDC;
	// 纸面区域
	pPrintInfo->rcPaper.left   = 0;
	pPrintInfo->rcPaper.top    = 0;
	pPrintInfo->rcPaper.right  = 100 * GetDeviceCaps(hDC, HORZSIZE);
	pPrintInfo->rcPaper.bottom = 100 * GetDeviceCaps(hDC, VERTSIZE);
	// 打印区域
	pPrintInfo->rcPrint.left   = pPrintInfo->rcPaper.left   + 1500;
	pPrintInfo->rcPrint.top    = pPrintInfo->rcPaper.top    + 1000;
	pPrintInfo->rcPrint.right  = pPrintInfo->rcPaper.right  - 1000;
	pPrintInfo->rcPrint.bottom = pPrintInfo->rcPaper.bottom - 1000;

	// 时间区域高度
	pPrintInfo->nDateTimeHeight = 500;
	// 成交量区域高度
	pPrintInfo->nVolHeight = 4000;
	// 文字高度, 单位为0.01mm
	pPrintInfo->nTextHeight = 500;
	// 价格区域宽度
	pPrintInfo->nPriceWidth = 1400;
	// K线数据
	pPrintInfo->pKDA = pKDA;

	if (1)	// 分页
	{
		// 每个K线净宽, 单位为0.01mm
		pPrintInfo->nItemWidth = 120;		
		// 每个K线间隔, 单位为0.01mm
		pPrintInfo->nItemGap = 60;
		// 每页显示个数
		pPrintInfo->nPageItem = (pPrintInfo->rcPrint.right - pPrintInfo->rcPrint.left - pPrintInfo->nPriceWidth - pPrintInfo->nItemGap) / (pPrintInfo->nItemWidth + pPrintInfo->nItemGap);
		// 总页数
		pPrintInfo->nPageCount = (pKDA->nSize + pPrintInfo->nPageItem - 1) / pPrintInfo->nPageItem;
	}
	else	// 绘制到一页
	{
		// 总页数
		pPrintInfo->nPageCount = 1;
		// 每页显示个数
		pPrintInfo->nPageItem = pKDA->nSize + 1;
		// 每页净显示宽度
		int cx = pPrintInfo->rcPrint.right - pPrintInfo->rcPrint.left - pPrintInfo->nPriceWidth;
		float G = ((float)cx / (float)pPrintInfo->nPageItem - 1.0f) / 3.0f;
		// 每个K线净宽, 单位为0.01mm
		pPrintInfo->nItemWidth = (int)(2.0f * G);		
		// 每个K线间隔, 单位为0.01mm
		pPrintInfo->nItemGap = pPrintInfo->nItemWidth / 2;
	}

	// 画笔
	pPrintInfo->hBorderPen = CreatePen(PS_SOLID, 30, RGB(0,0,0));			// 边框画笔
	pPrintInfo->hDotPen    = CreatePen(PS_SOLID,  0, RGB(128,128,128));		// 虚线画笔
	pPrintInfo->hNormalPen = CreatePen(PS_SOLID, 10, RGB(0,0,0));			// 普通画笔
	// 画刷
	pPrintInfo->hBrushN = GetStockObject(WHITE_BRUSH);		// 阳线画刷
	pPrintInfo->hBrushX = GetStockObject(GRAY_BRUSH);		// 阴线画刷
}

static void ReleasePrintInfo(PPrintInfo pPrintInfo)
{
	DeleteObject(pPrintInfo->hBorderPen);
	DeleteObject(pPrintInfo->hDotPen);
	DeleteObject(pPrintInfo->hNormalPen);
	DeleteObject(pPrintInfo->hBrushN);
	DeleteObject(pPrintInfo->hBrushX);
}

typedef struct PageInfo PageInfo, *PPageInfo;
struct PageInfo
{
	PPrintInfo pPrintInfo;	// 打印信息
	int iCurPage;			// 当前页号
	int idxFrom;			// 当前页开始序号
	int nCurCount;			// 当前页显示个数
	float maxPrice;			// 当前页最高价格
	float minPrice;			// 当前页最低价格
	float maxVol;			// 当前页最高成交量
	float ratePrice;		// 当前页每1个价格单位对应纵向逻辑尺寸
	float rateVol;			// 当前页每1个成交量单位对应纵向逻辑尺寸
};

static void PreparePageInfo(PPageInfo pPageInfo, int iPage)
{
	PPrintInfo pPrintInfo = pPageInfo->pPrintInfo;

	pPageInfo->iCurPage = iPage;
	pPageInfo->idxFrom = iPage * pPrintInfo->nPageItem;
	if (pPageInfo->idxFrom + pPrintInfo->nPageItem < pPrintInfo->pKDA->nSize)
		pPageInfo->nCurCount = pPrintInfo->nPageItem;
	else
		pPageInfo->nCurCount = pPrintInfo->pKDA->nSize - pPageInfo->idxFrom;

	PKLineData pKD = pPrintInfo->pKDA->pData + pPageInfo->idxFrom;
	pPageInfo->maxPrice = pKD->High;
	pPageInfo->minPrice = pKD->Low;
	pPageInfo->maxVol   = pKD->Volume;
	for (int i = 1; i< pPageInfo->nCurCount; i++)
	{
		pKD++;
		if (pPageInfo->maxPrice < pKD->High)
			pPageInfo->maxPrice = pKD->High;
		if (pPageInfo->minPrice > pKD->Low)
			pPageInfo->minPrice = pKD->Low;
		if (pPageInfo->maxVol < pKD->Volume)
			pPageInfo->maxVol = pKD->Volume;
	}

	int cy;
	cy  = pPrintInfo->nVolHeight;
	cy -= pPrintInfo->nTextHeight;
	pPageInfo->rateVol = cy / pPageInfo->maxVol;

	cy  = pPrintInfo->rcPrint.bottom - pPrintInfo->rcPrint.top;
	cy -= pPrintInfo->nDateTimeHeight;
	cy -= pPrintInfo->nVolHeight;
	cy -= pPrintInfo->nTextHeight * 2;
	pPageInfo->ratePrice = cy / (pPageInfo->maxPrice - pPageInfo->minPrice);
}

// 辅助函数, 用当前画笔绘制直线
static inline void Line(HDC hDC, int x1, int y1, int x2, int y2)
{
	MoveToEx(hDC, x1, y1, NULL);
	LineTo(hDC, x2, y2);
}

static void PrintPage(PPrintInfo pPrintInfo, int iPage)
{
	if (iPage >= pPrintInfo->nPageCount)
		return;

	PageInfo pageInfo = {0};
	pageInfo.pPrintInfo = pPrintInfo;
	PreparePageInfo(&pageInfo, iPage);

	int x, y, x1, y1, x2, y2, len;
	char txt[128];
	HDC hDC = pPrintInfo->hDC;
	x1 = pPrintInfo->rcPrint.left;
	y1 = pPrintInfo->rcPrint.top;
	x2 = pPrintInfo->rcPrint.right;
	y2 = pPrintInfo->rcPrint.bottom;

	// 外框
	{
		SelectObject(hDC, pPrintInfo->hBorderPen);
		SelectObject(hDC, pPrintInfo->hBrushN);
		Rectangle(hDC, x1, y1, x2, y2);
	}
	// 日期
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// 分界线
		SelectObject(hDC, pPrintInfo->hNormalPen);
		Line(hDC, x1, y1 + pPrintInfo->nDateTimeHeight, x2, y1 + pPrintInfo->nDateTimeHeight);
		// K线数组
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// 开始年份
		int lastYear = pKD->Date / 10000;
		// 开始月份
		int lastMonth = (pKD->Date %10000) / 100;

		len = wsprintf(txt, "%d年", lastYear);
		TextOut(hDC, x1 + 50, y1 + pPrintInfo->nDateTimeHeight - 100, txt, len);

		x = x1 + pPrintInfo->nItemGap + pPrintInfo->nItemWidth / 2;
		for (int i = 1; i< pageInfo.nCurCount; i++)
		{
			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// 中心位置

			int curYear = pKD->Date/10000;
			int curMonth = (pKD->Date %10000) / 100;
			if (lastYear != curYear)
				len = wsprintf(txt, "%d年", curYear);
			else if(lastMonth != curMonth)
				len = wsprintf(txt, "%d月", curMonth);

			if (lastYear != curYear || lastMonth != curMonth)
			{
				lastYear = curYear;
				lastMonth = curMonth;

				//SelectObject(hDC, pPrintInfo->hDotPen);
				//Line(hDC, x, y1 + pPrintInfo->nDateTimeHeight, x, y2 - pPrintInfo->nTextHeight);
				Line(hDC, x, y1, x, y1 + pPrintInfo->nDateTimeHeight);

				TextOut(hDC, x + 50, y1 + pPrintInfo->nDateTimeHeight - 100, txt, len);
			}
		}
	}

	// 成交量
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// 分界线
		y = y1 + pPrintInfo->nDateTimeHeight + pPrintInfo->nVolHeight;
		Line(pPrintInfo->hDC, x1, y, x2, y);
		// K线数组
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// 开始位置
		x = x1 + pPrintInfo->nItemGap;
		int y0 = pPrintInfo->rcPrint.top + pPrintInfo->nDateTimeHeight + 50;
		for (int i = 0; i< pageInfo.nCurCount; i++)
		{
			HBRUSH hCurBrush = (pKD->Open >= pKD->Close ? pPrintInfo->hBrushX : pPrintInfo->hBrushN);
			SelectObject(hDC, hCurBrush);

			int cy = (int)(pKD->Volume * pageInfo.rateVol);
			Rectangle(hDC, x, y0, x + pPrintInfo->nItemWidth, y0 + cy);

			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// 中心位置
		}
	}

	// 价格线
	{
		SelectObject(hDC, pPrintInfo->hNormalPen);
		Line(hDC, x2 - pPrintInfo->nPriceWidth, y1, x2 - pPrintInfo->nPriceWidth, y2);
	}

	// K线
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// K线数组
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// 开始位置
		int y0 = pPrintInfo->rcPrint.top + pPrintInfo->nDateTimeHeight + pPrintInfo->nVolHeight + pPrintInfo->nTextHeight;	// 基准Y
		x = x1 + pPrintInfo->nItemGap + pPrintInfo->nItemWidth / 2;	// 中心位置
		for (int i = 0; i< pageInfo.nCurCount; i++)
		{
			int yHigh = y0 + pageInfo.ratePrice * (pKD->High - pageInfo.minPrice);
			int yLow  = y0 + pageInfo.ratePrice * (pKD->Low - pageInfo.minPrice);
			int yBig, ySmall;	// Open or Close

			// 阳线
			if (pKD->Open < pKD->Close)
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Close - pageInfo.minPrice);
				ySmall = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
			}
			// 阴线
			else if (pKD->Open > pKD->Close)
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
				ySmall = y0 + pageInfo.ratePrice * (pKD->Close - pageInfo.minPrice);
			}
			// 十字线
			else
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
				ySmall = yBig;
			}

			HBRUSH hCurBrush = (pKD->Open >= pKD->Close ? pPrintInfo->hBrushX : pPrintInfo->hBrushN);
			SelectObject(hDC, hCurBrush);

			// 上影线
			Line(hDC, x, yHigh, x, yBig);
			// 下影线
			Line(hDC, x, ySmall, x, yLow);
			// 实体
			if (ySmall == yBig)
				Line(hDC, x - pPrintInfo->nItemWidth / 2, ySmall, x + pPrintInfo->nItemWidth / 2, ySmall);
			else
				Rectangle(hDC, x - pPrintInfo->nItemWidth / 2, yBig,  x + pPrintInfo->nItemWidth / 2, ySmall);

			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// 中心位置
		}
	}

}

static void PrepareDC(HDC hDC)
{
	// 设置左下角为坐标原点
	SetViewportOrgEx(hDC, 0, GetDeviceCaps(hDC, VERTRES), NULL);
	// 设置映射单位为0.01mm
	SetMapMode(hDC, MM_HIMETRIC);
}

void PrintKLine(HWND hDlg, HDC hDC, PKLineDatas pKDA)
{

	DOCINFO di = {0};
	di.cbSize       = sizeof(DOCINFO);
	di.lpszDocName  = pKDA->pStock->sName;
	di.lpszOutput   = (LPTSTR)NULL;
	di.lpszDatatype = (LPTSTR)NULL;
	di.fwType       = 0;
	StartDoc(hDC, &di); 

	PrintInfo printInfo = {0};
	PreparePrintInfo(&printInfo, hDC, pKDA);

	PrepareDC(hDC);
	for (int iPage = 0; iPage < printInfo.nPageCount; iPage++)
	{
		StartPage(hDC); 
		PrintPage(&printInfo, iPage);
		EndPage(hDC); 
	}

	ReleasePrintInfo(&printInfo);
	EndDoc(hDC);
}
