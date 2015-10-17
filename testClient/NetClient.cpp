// NetClient.cpp : Defines the entry point for the console application.
//

#include "JSClient.h"

class CMyNotify:public CNotify
{
public:
	CMyNotify() {};
	~CMyNotify() {};

public:
	 virtual int onData(void* data, int len);
};

int CMyNotify::onData(void* data, int len)
{
	char* pString = (char*)data;

	printf("receive data : [%s]", pString);

	return 0;
}

int main()
{
 	CMyNotify* pNofity = new CMyNotify;
 
 	CJSClient cjsClient(pNofity);

 	int nRet = cjsClient.Connect("127.0.0.1", 1987);

	cjsClient.Send("hellohahah", strlen("hellohahah")+1);

	while (true)
	{
		Sleep(100);
	}
	
	return 0;
}

