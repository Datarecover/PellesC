#pragma once

// 从INI读取服务器信息
BOOL IniGetHost(PTdxHost pHost);

// 写入股票列表
void IniWriteAllStocks(PStocks pStocks);
// 读入股票列表
PStocks IniGetAllStocks(void);
