#include "JSClient.h"
#include <WS2tcpip.h>
#include <stdio.h>

// for git test

CJSClient::CJSClient(CNotify* pNofity)
	:m_pNotify(pNofity)
{
	WSADATA wsaData;
	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);

	m_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

// 	sockaddr_in serv_addr;
// 	memset(&serv_addr, 0, sizeof(sockaddr_in));
// 	serv_addr.sin_family = AF_INET;
// 	serv_addr.sin_port = htons(1985);
// 	inet_pton(AF_INET, "127.0.0.1", (void*)&serv_addr.sin_addr);
// 	if (serv_addr.sin_addr.s_addr == INADDR_NONE)
// 	{
// 		struct addrinfo *answer, hint, *curr;
// 		memset(&hint, 0x0, sizeof(hint));
// 		hint.ai_family = AF_INET;		/* Allow IPv4 */
// 		hint.ai_flags = AI_PASSIVE;		/* For wildcard IP address */
// 		hint.ai_protocol = 0;			/* Any protocol */
// 		hint.ai_socktype = SOCK_STREAM;
// 
// 		int ret = getaddrinfo("my_computer", NULL, &hint, &answer);
// 		if (ret != 0)
// 		{
// 			
// 		}
// 
// 		for (curr = answer; curr != NULL; curr = curr->ai_next) 
// 		{
// 			memcpy(&serv_addr.sin_addr, &(((struct sockaddr_in *)(curr->ai_addr))->sin_addr), sizeof(serv_addr.sin_addr));
// 			break;
// 		}
// 	}
// 
// 	nRet = ::bind(m_fd, (sockaddr*)&serv_addr, sizeof(serv_addr));

	m_buffer.clear();

	m_bRunning = true;
}

CJSClient::~CJSClient()
{
	m_bRunning = false;

	closesocket(m_fd);

	WSACleanup();
}

int CJSClient::Connect(const char* ip, int port)
{
	if (ip == NULL || port <= 0)
	{
		return -1;
	}

	sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, (PVOID)&serv_addr.sin_addr);
	int ret = connect(m_fd, (sockaddr*)&serv_addr, sizeof(serv_addr));
	if (ret < 0)
	{
		printf("Connect Server[%s]--Port[%d] fail, error[%d]", ip, port, WSAGetLastError());

		return ret;
	}

	u_long nonblock = 1;
	ret = ioctlsocket(m_fd, FIONBIO, &nonblock);

	DWORD thread_id;

	CreateThread(NULL, 0, StartRoutine, this, 0, &thread_id);

	return ret;
}

DWORD WINAPI CJSClient::StartRoutine(LPVOID arg)
{
	CJSClient* pClient = (CJSClient*)arg;

	pClient->BeginDetect(pClient);

	return 0;
}

void CJSClient::BeginDetect(CJSClientListener* pListener)
{
	if (NULL == pListener)
	{
		return ;
	}

	fd_set read_set, write_set, excep_set;
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100 * 1000;	// 10 millisecond

	while (m_bRunning)
	{
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_ZERO(&excep_set);
		FD_SET(m_fd, &read_set);
		FD_SET(m_fd, &write_set);
		FD_SET(m_fd, &excep_set);

		int nfds = select(0, &read_set, &write_set, &excep_set, &timeout);

		if (nfds == SOCKET_ERROR)
		{
			printf("select failed, error code: %d\n", GetLastError());
			Sleep(100);
			continue;			// select again
		}

		if (nfds == 0)
		{
			Sleep(100);
			continue;
		}

		for (u_int i = 0; i < read_set.fd_count; i++)
		{
			pListener->OnRead(read_set.fd_array[i]);	
		}

		for (u_int i = 0; i < write_set.fd_count; i++)
		{
			pListener->OnWrite(write_set.fd_array[i]);	
		}

		for (u_int i = 0; i < excep_set.fd_count; i++)
		{
			pListener->OnExcept(excep_set.fd_array[i]);	
		}

		Sleep(100);
	}

	return ;
}

int CJSClient::OnRead(SOCKET fd)
{
	u_long avail = 0;
	if ( (ioctlsocket(m_fd, FIONREAD, &avail) == SOCKET_ERROR) || (avail == 0) )
	{
		m_bRunning = false;

		closesocket(m_fd);

		return 0;
	}

	char recvbuf[MAX_SEND_RCV_LEN] = {0};

	m_buffer.clear();

	for (;;)
	{
		int nRecvLen = recv(fd, recvbuf, sizeof(recvbuf), 0);
		if (nRecvLen <= 0)
		{
			break;
		}

		for (int i = 0; i < nRecvLen; ++i)
		{
			m_buffer.push_back(recvbuf[i]);
		}

		memset(recvbuf, 0x0, sizeof(recvbuf));
	}
	
	if (!m_buffer.empty())
	{
		onData();
	}

	return 0;
}

void CJSClient::onData()
{
	m_pNotify->onData((void*)&m_buffer[0], m_buffer.size());

	return ;
}

int CJSClient::Send(const char* buf, int len)
{
	if (NULL == buf || len <= 0)
	{
		return -1;
	}

	int offset = 0;
	int remain = len;

	while (remain > 0)
	{
		int send_size = remain;
		if (send_size > MAX_SEND_RCV_LEN)
			send_size = MAX_SEND_RCV_LEN;

		int nSend = send(m_fd, (char*)buf+offset, send_size, 0);
		if (nSend <= 0)
		{
			break;
		}

		offset += nSend;
		remain -= nSend;
	}

	return 0;
}

int CJSClient::OnWrite(SOCKET fd)
{

	return 0;
}

int CJSClient::OnExcept(SOCKET fd)
{
	m_bRunning = false;

	closesocket(m_fd);

	return 0;
}