#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "anLogs.h"

typedef struct _AVLOGS_DATA__
{
	AvLogs_Callback LogCallback;
}AVLOGS_DATA;

AVLOGS_DATA gAvLogData = {0};

int AvLogInit(AvLogs_Callback cb)
{
	memset(&gAvLogData, 0, sizeof(gAvLogData));
	
	gAvLogData.LogCallback = cb;

	return 0;
}

int AvLogTerminate()
{
	return 0;
}

int XLogWrite(int level, const char *fmt, ...)
{
	va_list argp;
	char              szTextBuf[2048];
	int length;

	if (gAvLogData.LogCallback)
	{
		if (level <= AVLOGCB_ERROR)
		{
			va_start(argp, fmt);
			length = vsnprintf(szTextBuf, sizeof(szTextBuf) - 1, fmt, argp);
			va_end(argp);

			return gAvLogData.LogCallback(level, szTextBuf, length);
		}
	}

	return 0;
}

int XLogWriteBin(unsigned char *pData, int len)
{

	if (gAvLogData.LogCallback)
	{
		return gAvLogData.LogCallback(AVLOGCB_BIN, (char *)pData, len);
	}

	return 0;
}