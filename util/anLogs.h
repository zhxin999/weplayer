#ifndef __AN_LOGS_H__
#define __AN_LOGS_H__

enum
{
	AVLOGCB_RAW = 0,
	AVLOGCB_DEBUG,
	AVLOGCB_WARN,
	AVLOGCB_ERROR,
	AVLOGCB_BIN,
	AVLOGCB_TYPE_MAX
};

typedef int(*AvLogs_Callback)(int Type, const char* data, int dataLen);

#define MessageError(format,...)     XLogWrite(AVLOGCB_ERROR, format, ##__VA_ARGS__)
#define MessageOutput(format,...)    XLogWrite(AVLOGCB_DEBUG, format, ##__VA_ARGS__)  
#define MessageWarn(format,...)    XLogWrite(AVLOGCB_WARN, format, ##__VA_ARGS__)  
#define MessageRaw(format,...)    XLogWrite(AVLOGCB_RAW, format, ##__VA_ARGS__)  

#define MessageBin(format, data, len)   XLogWriteBin(data, len)


int AvLogInit(AvLogs_Callback cb);
int AvLogTerminate();
int XLogWrite(int level, const char *fmt, ...);
int XLogWriteBin(unsigned char *pData, int len);

#endif
