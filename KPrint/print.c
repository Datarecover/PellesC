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
	HDC hDC;				// ��ӡ���
	RECT rcPaper;			// ֽ������, ��λΪ0.01mm
	RECT rcPrint;			// ��ӡ����, ��λΪ0.01mm
	int nDateTimeHeight;	// ʱ������߶�, ��λΪ0.01mm
	int nVolHeight;			// �ɽ�������߶�, ��λΪ0.01mm
	int nTextHeight;		// ���ָ߶�,  ��λΪ0.01mm
	int nPriceWidth;		// �۸�������, ��λΪ0.01mm
	int nItemWidth;			// ÿ��K�߾���, ��λΪ0.01mm
	int nItemGap;			// ÿ��K�߼��, ��λΪ0.01mm
	PKLineDatas pKDA;		// K������
	int nPageItem;			// ÿҳ��ʾ����
	int nPageCount;			// ��ҳ��
	HPEN hBorderPen;		// �߿򻭱�
	HPEN hDotPen;			// ���߻���
	HPEN hNormalPen;		// ��ͨ����
	HBRUSH hBrushN;			// ���߻�ˢ
	HBRUSH hBrushX;			// ���߻�ˢ
};

static void PreparePrintInfo(PPrintInfo pPrintInfo, HDC hDC, PKLineDatas pKDA)
{
	// ��ӡ���
	pPrintInfo->hDC = hDC;
	// ֽ������
	pPrintInfo->rcPaper.left   = 0;
	pPrintInfo->rcPaper.top    = 0;
	pPrintInfo->rcPaper.right  = 100 * GetDeviceCaps(hDC, HORZSIZE);
	pPrintInfo->rcPaper.bottom = 100 * GetDeviceCaps(hDC, VERTSIZE);
	// ��ӡ����
	pPrintInfo->rcPrint.left   = pPrintInfo->rcPaper.left   + 1500;
	pPrintInfo->rcPrint.top    = pPrintInfo->rcPaper.top    + 1000;
	pPrintInfo->rcPrint.right  = pPrintInfo->rcPaper.right  - 1000;
	pPrintInfo->rcPrint.bottom = pPrintInfo->rcPaper.bottom - 1000;

	// ʱ������߶�
	pPrintInfo->nDateTimeHeight = 500;
	// �ɽ�������߶�
	pPrintInfo->nVolHeight = 4000;
	// ���ָ߶�, ��λΪ0.01mm
	pPrintInfo->nTextHeight = 500;
	// �۸�������
	pPrintInfo->nPriceWidth = 1400;
	// K������
	pPrintInfo->pKDA = pKDA;

	if (1)	// ��ҳ
	{
		// ÿ��K�߾���, ��λΪ0.01mm
		pPrintInfo->nItemWidth = 120;		
		// ÿ��K�߼��, ��λΪ0.01mm
		pPrintInfo->nItemGap = 60;
		// ÿҳ��ʾ����
		pPrintInfo->nPageItem = (pPrintInfo->rcPrint.right - pPrintInfo->rcPrint.left - pPrintInfo->nPriceWidth - pPrintInfo->nItemGap) / (pPrintInfo->nItemWidth + pPrintInfo->nItemGap);
		// ��ҳ��
		pPrintInfo->nPageCount = (pKDA->nSize + pPrintInfo->nPageItem - 1) / pPrintInfo->nPageItem;
	}
	else	// ���Ƶ�һҳ
	{
		// ��ҳ��
		pPrintInfo->nPageCount = 1;
		// ÿҳ��ʾ����
		pPrintInfo->nPageItem = pKDA->nSize + 1;
		// ÿҳ����ʾ���
		int cx = pPrintInfo->rcPrint.right - pPrintInfo->rcPrint.left - pPrintInfo->nPriceWidth;
		float G = ((float)cx / (float)pPrintInfo->nPageItem - 1.0f) / 3.0f;
		// ÿ��K�߾���, ��λΪ0.01mm
		pPrintInfo->nItemWidth = (int)(2.0f * G);		
		// ÿ��K�߼��, ��λΪ0.01mm
		pPrintInfo->nItemGap = pPrintInfo->nItemWidth / 2;
	}

	// ����
	pPrintInfo->hBorderPen = CreatePen(PS_SOLID, 30, RGB(0,0,0));			// �߿򻭱�
	pPrintInfo->hDotPen    = CreatePen(PS_SOLID,  0, RGB(128,128,128));		// ���߻���
	pPrintInfo->hNormalPen = CreatePen(PS_SOLID, 10, RGB(0,0,0));			// ��ͨ����
	// ��ˢ
	pPrintInfo->hBrushN = GetStockObject(WHITE_BRUSH);		// ���߻�ˢ
	pPrintInfo->hBrushX = GetStockObject(GRAY_BRUSH);		// ���߻�ˢ
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
	PPrintInfo pPrintInfo;	// ��ӡ��Ϣ
	int iCurPage;			// ��ǰҳ��
	int idxFrom;			// ��ǰҳ��ʼ���
	int nCurCount;			// ��ǰҳ��ʾ����
	float maxPrice;			// ��ǰҳ��߼۸�
	float minPrice;			// ��ǰҳ��ͼ۸�
	float maxVol;			// ��ǰҳ��߳ɽ���
	float ratePrice;		// ��ǰҳÿ1���۸�λ��Ӧ�����߼��ߴ�
	float rateVol;			// ��ǰҳÿ1���ɽ�����λ��Ӧ�����߼��ߴ�
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

// ��������, �õ�ǰ���ʻ���ֱ��
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

	// ���
	{
		SelectObject(hDC, pPrintInfo->hBorderPen);
		SelectObject(hDC, pPrintInfo->hBrushN);
		Rectangle(hDC, x1, y1, x2, y2);
	}
	// ����
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// �ֽ���
		SelectObject(hDC, pPrintInfo->hNormalPen);
		Line(hDC, x1, y1 + pPrintInfo->nDateTimeHeight, x2, y1 + pPrintInfo->nDateTimeHeight);
		// K������
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// ��ʼ���
		int lastYear = pKD->Date / 10000;
		// ��ʼ�·�
		int lastMonth = (pKD->Date %10000) / 100;

		len = wsprintf(txt, "%d��", lastYear);
		TextOut(hDC, x1 + 50, y1 + pPrintInfo->nDateTimeHeight - 100, txt, len);

		x = x1 + pPrintInfo->nItemGap + pPrintInfo->nItemWidth / 2;
		for (int i = 1; i< pageInfo.nCurCount; i++)
		{
			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// ����λ��

			int curYear = pKD->Date/10000;
			int curMonth = (pKD->Date %10000) / 100;
			if (lastYear != curYear)
				len = wsprintf(txt, "%d��", curYear);
			else if(lastMonth != curMonth)
				len = wsprintf(txt, "%d��", curMonth);

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

	// �ɽ���
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// �ֽ���
		y = y1 + pPrintInfo->nDateTimeHeight + pPrintInfo->nVolHeight;
		Line(pPrintInfo->hDC, x1, y, x2, y);
		// K������
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// ��ʼλ��
		x = x1 + pPrintInfo->nItemGap;
		int y0 = pPrintInfo->rcPrint.top + pPrintInfo->nDateTimeHeight + 50;
		for (int i = 0; i< pageInfo.nCurCount; i++)
		{
			HBRUSH hCurBrush = (pKD->Open >= pKD->Close ? pPrintInfo->hBrushX : pPrintInfo->hBrushN);
			SelectObject(hDC, hCurBrush);

			int cy = (int)(pKD->Volume * pageInfo.rateVol);
			Rectangle(hDC, x, y0, x + pPrintInfo->nItemWidth, y0 + cy);

			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// ����λ��
		}
	}

	// �۸���
	{
		SelectObject(hDC, pPrintInfo->hNormalPen);
		Line(hDC, x2 - pPrintInfo->nPriceWidth, y1, x2 - pPrintInfo->nPriceWidth, y2);
	}

	// K��
	SelectObject(hDC, pPrintInfo->hNormalPen);
	{
		// K������
		PKLineData pKD = pPrintInfo->pKDA->pData + pageInfo.idxFrom;
		// ��ʼλ��
		int y0 = pPrintInfo->rcPrint.top + pPrintInfo->nDateTimeHeight + pPrintInfo->nVolHeight + pPrintInfo->nTextHeight;	// ��׼Y
		x = x1 + pPrintInfo->nItemGap + pPrintInfo->nItemWidth / 2;	// ����λ��
		for (int i = 0; i< pageInfo.nCurCount; i++)
		{
			int yHigh = y0 + pageInfo.ratePrice * (pKD->High - pageInfo.minPrice);
			int yLow  = y0 + pageInfo.ratePrice * (pKD->Low - pageInfo.minPrice);
			int yBig, ySmall;	// Open or Close

			// ����
			if (pKD->Open < pKD->Close)
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Close - pageInfo.minPrice);
				ySmall = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
			}
			// ����
			else if (pKD->Open > pKD->Close)
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
				ySmall = y0 + pageInfo.ratePrice * (pKD->Close - pageInfo.minPrice);
			}
			// ʮ����
			else
			{
				yBig = y0 + pageInfo.ratePrice * (pKD->Open - pageInfo.minPrice);
				ySmall = yBig;
			}

			HBRUSH hCurBrush = (pKD->Open >= pKD->Close ? pPrintInfo->hBrushX : pPrintInfo->hBrushN);
			SelectObject(hDC, hCurBrush);

			// ��Ӱ��
			Line(hDC, x, yHigh, x, yBig);
			// ��Ӱ��
			Line(hDC, x, ySmall, x, yLow);
			// ʵ��
			if (ySmall == yBig)
				Line(hDC, x - pPrintInfo->nItemWidth / 2, ySmall, x + pPrintInfo->nItemWidth / 2, ySmall);
			else
				Rectangle(hDC, x - pPrintInfo->nItemWidth / 2, yBig,  x + pPrintInfo->nItemWidth / 2, ySmall);

			pKD++;
			x += pPrintInfo->nItemWidth + pPrintInfo->nItemGap;	// ����λ��
		}
	}

}

static void PrepareDC(HDC hDC)
{
	// �������½�Ϊ����ԭ��
	SetViewportOrgEx(hDC, 0, GetDeviceCaps(hDC, VERTRES), NULL);
	// ����ӳ�䵥λΪ0.01mm
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
