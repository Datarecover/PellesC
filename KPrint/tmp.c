// 临时代码

/*

static void InitPrinters(HWND hDlg)
{
	DRIVER_INFO_1 *pLevel1 = NULL;
	DWORD dwNeeded = 0, dwReceived = 0, dwError = 0;
	int nRet = EnumPrinterDrivers(
		NULL,		// local machine 
		NULL,		// current environment 
		1,			// level 1, whatever that means
		(LPBYTE)pLevel1,
		0,
		&dwNeeded,
		&dwReceived);
	
	pLevel1 = (DRIVER_INFO_1 *)calloc(1, dwNeeded);
	nRet = EnumPrinterDrivers(
		NULL, // local machine
		NULL, // current environment
		1, // level 1, whatever that means
		(LPBYTE) pLevel1,
		dwNeeded,
		&dwNeeded,
		&dwReceived);

	HWND hChild = GetDlgItem(hDlg, IDC_PRINTER);
	for (int nCount=0; nCount<(int)dwReceived; nCount++)
		ComboBox_AddString(hChild, pLevel1[nCount].pName);
	ComboBox_SetCurSel(hChild, 0);

	free(pLevel1);
}


static INT_PTR OnTest1(HWND hDlg)
{
	HDC hDC = CreateDC("WINSPOOL", "CutePDF Writer", NULL, NULL);

	DEVMODE dm = {0};
	dm.dmSpecVersion = DM_SPECVERSION;
	dm.dmSize = sizeof(DEVMODE);
	dm.dmFields = DM_ORIENTATION;

	DOCINFO di = {0};
	di.cbSize = sizeof(DOCINFO);
	di.lpszDocName = "Test Print";

	if (StartDoc(hDC, &di) != SP_ERROR)
	{
		dm.dmOrientation = DMORIENT_PORTRAIT;
		hDC = ResetDC(hDC, &dm);
		StartPage(hDC);
		TextOut(hDC, 600, 600, "This is print portrait.", 23);
		EndPage(hDC);

		dm.dmOrientation = DMORIENT_LANDSCAPE;
		hDC = ResetDC(hDC, &dm);
		StartPage(hDC);
		TextOut(hDC, 600, 600, "This is print landscape.", 24);
		EndPage(hDC);

		EndDoc(hDC);
	}
	DeleteDC(hDC);

	return TRUE;
}

static INT_PTR OnTest2(HWND hDlg)
{
	PAGESETUPDLG ps = {0};
	ps.lStructSize = sizeof(PAGESETUPDLG);
	ps.hwndOwner = hDlg;
	ps.Flags = PSD_INHUNDREDTHSOFMILLIMETERS;
	ps.hDevMode = ghDevMode;
	ps.hDevNames = ghDevNames;

	if (!PageSetupDlg(&ps))
		return TRUE;

	ghDevMode = ps.hDevMode;
	ghDevNames = ps.hDevNames;

	DEVMODE* pDevMode = (DEVMODE*)GlobalLock(ghDevMode);

	HDC hDC = CreateDC("WINSPOOL", "CutePDF Writer", NULL, NULL);

	GlobalUnlock(ghDevMode);

	DOCINFO di = {0};
	di.cbSize = sizeof(DOCINFO);
	di.lpszDocName = "Test Print";

	if (StartDoc(hDC, &di) != SP_ERROR)
	{
		StartPage(hDC);
		TextOut(hDC, 600, 600, "This is print portrait.", 23);
		EndPage(hDC);

		EndDoc(hDC);
	}
	DeleteDC(hDC);

	return TRUE;

}

static INT_PTR OnTest(HWND hDlg)
{
	// 取特定日期区间日K线数据
	BOOL bTranslated;
	UINT iStart = GetDlgItemInt(hDlg, IDC_STARTDATE, &bTranslated, FALSE);
	UINT iEnd   = GetDlgItemInt(hDlg, IDC_ENDDATE,   &bTranslated, FALSE);

	int iSel = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_STOCK));
	PKLineDatas pKDA = GetSpanKLineData(gpStocks->pData+iSel, iStart, iEnd);
	if (pKDA != NULL)
	{
		FILE* fp = fopen("D:\\600545.txt", "w");
		fprintf(fp, "日期       开盘   最高   最低   收盘     成交量     成交额\n");
		for (int i=0; i<pKDA->nSize; i++)
		{
			PKLineData pKD = pKDA->pData + i;
			fprintf(fp, "%08d %6.2f %6.2f %6.2f %6.2f %10.0f %12.0f\n", pKD->Date, pKD->Open, pKD->Low, pKD->High, pKD->Close, pKD->Volume, pKD->Amount);
		}
		fprintf(fp, "%08d 至 %08d 共 %d 个周期!\n", iStart, iEnd, pKDA->nSize); 
		fclose(fp);
		TRACE("%08d 至 %08d 共 %d 个周期!\n", iStart, iEnd, pKDA->nSize); 
		ReleaseKLineDatas(pKDA);
	}
	return TRUE;
}

static INT_PTR GetPrintInfo(HWND hDlg)
{
	PRINTDLGEX pd = {0};
	PRINTPAGERANGE pprs[10];
	pd.lStructSize = sizeof(PRINTDLGEX);
	pd.hwndOwner = hDlg;
	pd.hDevMode = ghDevMode;
	pd.hDevNames = ghDevNames;
	pd.hDC = NULL;
	pd.Flags = PD_RETURNDC | PD_HIDEPRINTTOFILE | PD_NOPAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE | PD_COLLATE;
	pd.Flags2 = 0;
	pd.ExclusionFlags = 0;
	pd.nPageRanges = 0;
	pd.nMaxPageRanges = 10;
	pd.lpPageRanges = pprs;
	pd.nMinPage = 1;
	pd.nMaxPage = 1000;
	pd.nCopies = 1;
	pd.hInstance = 0;
	pd.lpPrintTemplateName = NULL;
	pd.lpCallback = NULL;
	pd.nPropertyPages = 0;
	pd.lphPropertyPages = NULL;
	pd.nStartPage = START_PAGE_GENERAL;
	pd.dwResultAction = 0;

	HRESULT hResult = PrintDlgEx(&pd);
	if (hResult == S_OK && pd.dwResultAction == PD_RESULT_PRINT)
	{
		DEVMODE* pDevMode = (DEVMODE*)GlobalLock(pd.hDevMode);
		if (pDevMode && (pDevMode->dmFields & DM_ORIENTATION) == DM_ORIENTATION)
			TRACE("%d : 1=DMORIENT_PORTRAIT, 2=DMORIENT_LANDSCAPE\n", pDevMode->dmOrientation);
		GlobalUnlock(pd.hDevMode);

		TRACE("开始打印...\n");
		// 以mm为单位总大小
		TRACE("HORZSIZE: %d\n", GetDeviceCaps(pd.hDC, HORZSIZE));
		TRACE("VERTSIZE: %d\n", GetDeviceCaps(pd.hDC, VERTSIZE));
		// 以像素为单位总大小
		TRACE("HORZRES: %d\n", GetDeviceCaps(pd.hDC, HORZRES));
		TRACE("VERTRES: %d\n", GetDeviceCaps(pd.hDC, VERTRES));
		// 每英寸点数
		TRACE("LOGPIXELSX: %d\n", GetDeviceCaps(pd.hDC, LOGPIXELSX));
		TRACE("LOGPIXELSY: %d\n", GetDeviceCaps(pd.hDC, LOGPIXELSY));

		// 以像素为单位总大小
		TRACE("PHYSICALWIDTH: %d\n", GetDeviceCaps(pd.hDC, PHYSICALWIDTH));
		TRACE("PHYSICALHEIGHT: %d\n", GetDeviceCaps(pd.hDC, PHYSICALHEIGHT));
		// 以像素为单位总大小
		TRACE("PHYSICALOFFSETX: %d\n", GetDeviceCaps(pd.hDC, PHYSICALOFFSETX));
		TRACE("PHYSICALOFFSETY: %d\n", GetDeviceCaps(pd.hDC, PHYSICALOFFSETY));


		DOCINFO di;
		memset(&di, 0, sizeof(DOCINFO));
		di.cbSize       = sizeof(DOCINFO);
		di.lpszDocName  = "Printing Test";
		di.lpszOutput   = (LPTSTR)NULL;
		di.lpszDatatype = (LPTSTR)NULL;
		di.fwType       = 0;
		StartDoc(pd.hDC, &di); 
		StartPage(pd.hDC); 
		MoveToEx(pd.hDC, 0, 0, NULL);
		LineTo(pd.hDC, 500, 500);
		LineTo(pd.hDC, 600, 900);
		EndPage(pd.hDC); 
		EndDoc(pd.hDC);
		// User clicked the Print button, so
		// use the DC and other information returned in the 
		// PRINTDLGEX structure to print the document

	}

	if (pd.hDC != NULL) 
	    DeleteDC(pd.hDC);

	if (ghDevMode == NULL)
		ghDevMode = pd.hDevMode;
	if (ghDevNames == NULL)
		ghDevNames = pd.hDevNames;

	return TRUE;
}




*/
