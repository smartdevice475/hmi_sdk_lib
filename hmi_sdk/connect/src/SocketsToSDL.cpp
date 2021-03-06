﻿#include "global_first.h"
#include <SocketsToSDL.h>

#ifdef WIN32
#ifdef WINCE
#pragma comment(lib,"ws2.lib")
#else
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"pthreadVC2.lib") 
#pragma comment(lib,"pthreadVCE2.lib") 
#pragma comment(lib,"pthreadVSE2.lib") 
#endif
#else
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

// add by fanqiang
#include "Channel.h"

SocketsToSDL::SocketsToSDL()
	:	m_Read_Sign(-1), 
		m_Write_Sign(-1),
		m_SendThread(),
        m_bTerminate(false)
{
    m_sHost = "127.0.0.1";
    m_iPort = 8087;

#ifdef WIN32
	WSADATA wsa = { 0 };
	WSAStartup(MAKEWORD(2, 2), &wsa); 
#endif
}

SocketsToSDL::~SocketsToSDL()
{
    m_bTerminate = true;
	Notify();
	pthread_join(m_SendThread, 0);

    CloseSockets();
}

void SocketsToSDL::CloseSockets()
{
    if (-1 != m_Read_Sign)
#if defined(WIN32)
        closesocket(m_Read_Sign);
#else
        close(m_Read_Sign);
#endif

    if (-1 != m_Write_Sign)
#ifdef WIN32
        closesocket(m_Write_Sign);
#else
        close(m_Write_Sign);
#endif

    m_Read_Sign = -1;
    m_Write_Sign = -1;
    m_bTerminate = true;

    int iNum = m_SocketHandles.size();
    for (int i = 0; i < iNum; ++i) {
        CSockHandle * pHandle = m_SocketHandles[i];
        pHandle->Close();
        delete pHandle;
    }
    m_SocketHandles.clear();
}

void SocketsToSDL::Notify()
{
    if (-1 == m_Write_Sign)
		return;
	
	char c = 0;
	::send(m_Write_Sign, (const char *)&c, 1, 0);
}

bool SocketsToSDL::CreateSignal()
{
    int tcp1, tcp2;
    sockaddr_in name;
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#ifdef WIN32
    int namelen = sizeof(name);
#else
    size_t namelen = sizeof(name);;
#endif

    tcp1 = tcp2 = -1;
	int tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp == -1) {
		goto clean;
	}
    if (bind(tcp, (sockaddr*)&name, namelen) == -1) {
		goto clean;
	}
    if (::listen(tcp, 5) == -1) {
		goto clean;
	}
#ifdef WIN32
    if (getsockname(tcp, (sockaddr*)&name, &namelen) == -1) {
#else
    if (getsockname(tcp, (sockaddr*)&name, (socklen_t *)&namelen) == -1) {
#endif
        goto clean;
	}
    tcp1 = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp1 == -1) {
		goto clean;
	}
    if (-1 == connect(tcp1, (sockaddr*)&name, namelen)) {
		goto clean;
	}
#ifdef WIN32
    tcp2 = accept(tcp, (sockaddr*)&name, &namelen);
#else
    tcp2 = accept(tcp, (sockaddr*)&name, (socklen_t *)&namelen);
#endif
    if (tcp2 == -1) {
		goto clean;
	}
#ifdef WIN32
    if (closesocket(tcp) == -1) {
#else
    if (close(tcp) == -1) {
#endif
        goto clean;
	}
	m_Write_Sign = tcp1;
	m_Read_Sign = tcp2;
#ifdef WIN32
    {
        u_long iMode = 1;
        ioctlsocket(m_Read_Sign, FIONBIO, (u_long FAR*) &iMode);
    }
#else
    #include<fcntl.h>
    fcntl(m_Read_Sign,F_SETFL, O_NONBLOCK);
#endif
    return true;

clean:
    if (tcp != -1) {
#ifdef WIN32
        closesocket(tcp);
#else
        close(tcp);
#endif
    }
    if (tcp2 != -1) {
#ifdef WIN32
        closesocket(tcp2);
#else
        close(tcp2);
#endif
    }
    if (tcp1 != -1) {
#ifdef WIN32
        closesocket(tcp1);
#else
        close(tcp1);
#endif
    }
	return false;
}

void* StartSocketThread(void* p) {
	SocketsToSDL * pThis = (SocketsToSDL *)p;
	pThis->RunThread();
	return 0;
}

bool SocketsToSDL::ConnectTo(std::vector<IChannel *> Channels, INetworkStatus * pNetwork)
{
    m_pNetwork = pNetwork;
	if (!CreateSignal())
		return false;

    // add by fanqiang read sdl host address
    if (!g_StaticConfigJson.isNull()){
        Json::Value sdladdr = g_StaticConfigJson["SDLAddr"];
        if (sdladdr.isMember("host"))
            m_sHost = sdladdr["host"].asCString();
        if (sdladdr.isMember("port"))
            m_iPort = sdladdr["port"].asInt();
    }

    int iNum = Channels.size();
    for (int i = 0; i < iNum; ++i) {
        CSockHandle * pHandle = new CSockHandle(1024);
        if (pHandle->Connect(Channels[i], m_sHost, m_iPort)) {
            m_SocketHandles.push_back(pHandle);
            Channels[i]->setSocketManager(this, pHandle);
        } else {
            goto FAILED;
        }
	}

    m_bTerminate = false;
	if (0 != pthread_create(&m_SendThread, 0, &StartSocketThread, this))
		goto FAILED;

    return true;

FAILED:
    CloseSockets();
    return false;
}

bool SocketsToSDL::ConnectToVS( IChannel * ChannelVS, std::string sIP, int iPort,INetworkStatus * pNetwork)
{
    m_pNetwork = pNetwork;

    CSockHandle * pHandle = new CSockHandle(576000);
    if (pHandle->Connect(ChannelVS, sIP, iPort)) {
        m_SocketHandles.push_back(pHandle);
        ChannelVS->setSocketManager(this, pHandle);
        Notify();
        return true;
    } else {
        return false;
    }
}
void SocketsToSDL::DelConnectToVS()
{
    CSockHandle * pHandle = m_SocketHandles.at(m_SocketHandles.size() - 1);

    m_SocketHandles.pop_back();
    Notify();
#ifdef WIN32
    Sleep(1000);
#else
    usleep(1000000);
#endif
//    shutdown(pHandle->m_i_socket, SHUT_RDWR);
    pHandle->Close();
}

void SocketsToSDL::SendData(void * pHandle, void * pData, int iLength)
{
    if (m_bTerminate)
        return;

    CSockHandle * p = (CSockHandle *)pHandle;
    p->PushData(pData, iLength);
	Notify();
}

void SocketsToSDL::RunThread()
{
#ifdef WIN32
    FD_SET fdRead;
    int fd_max = -1;
#else
    fd_set fdRead;
    fd_set fdWrite;
    int fd_max = m_Read_Sign;
#endif
    while (!m_bTerminate) {
		FD_ZERO(&fdRead);
#ifndef WIN32
		FD_ZERO(&fdWrite);
#endif

		int iNum = m_SocketHandles.size();
        for (int i = 0; i < iNum; ++i) {
            CSockHandle * pHandle = m_SocketHandles[i];
            int socket = pHandle->GetSocketID();
			FD_SET(socket, &fdRead);
#ifndef WIN32
			FD_SET(socket, &fdWrite);
#endif
            fd_max = fd_max > socket? fd_max : socket;
		}
		FD_SET(m_Read_Sign, &fdRead);

#ifdef WIN32
        if (select(0, &fdRead, NULL, NULL, NULL) == SOCKET_ERROR) {
#else
        if (select(fd_max+1, &fdRead, &fdWrite, NULL, NULL) == SOCKET_ERROR) {
#endif
            goto SOCKET_WRONG;
		}

        bool bSend = FD_ISSET(m_Read_Sign, &fdRead);
        if (bSend) {
			unsigned char buffer[8];
			int bytes_read = 0;
			do{
				bytes_read = recv(m_Read_Sign, (char *)buffer, sizeof(buffer), 0);
			} while (bytes_read > 0);
            iNum = m_SocketHandles.size();
            for (int i = 0; i < iNum; ++i) {
                if (i + 1 > m_SocketHandles.size()) {
                    break;
                }
                CSockHandle * pHandle = m_SocketHandles[i];
                if (!pHandle->SendData())
                    goto SOCKET_WRONG;
            }
		}


        iNum = m_SocketHandles.size();
        for (int i = 0; i < iNum; ++i) {
            if (i + 1 > m_SocketHandles.size()) {
                break;
            }
            CSockHandle * pHandle = m_SocketHandles[i];
            if (FD_ISSET(pHandle->GetSocketID(), &fdRead)) {
                if (!pHandle->RecvData()) {
                    goto SOCKET_WRONG;
                }
            }
		}
	}
    return;

SOCKET_WRONG:
    m_bTerminate = true;
    CloseSockets();

    if (m_pNetwork)
        m_pNetwork->onNetworkBroken();
    return;
}


CSockHandle::CSockHandle(int bufSize)
    : m_SendMutex()
{
    m_i_recBufSize = bufSize;
    m_recBuf = (unsigned char *)malloc(bufSize);

    pthread_mutex_init(&m_SendMutex, 0);

}

CSockHandle::~CSockHandle()
{
    free(m_recBuf);
    pthread_mutex_destroy(&m_SendMutex);
}

bool CSockHandle::Connect(IChannel *newChannel, std::string sIP, int iPort)
{
    sockaddr_in toLocal;
    memset(&toLocal, 0, sizeof(toLocal));
    toLocal.sin_family = AF_INET;
    toLocal.sin_addr.s_addr = inet_addr(sIP.c_str());
    toLocal.sin_port = htons(iPort);

#ifdef WIN32
    int namelen = sizeof(toLocal);
#else
    size_t namelen = sizeof(toLocal);
#endif

    this->pDataReceiver = newChannel;
    this->m_i_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKET_ERROR == this->m_i_socket) {
        LOGE("SOCKET INVALID");
    }
//    LOGE("this->m_i_socket = %d", this->m_i_socket);
    try{
        if (SOCKET_ERROR == ::connect(this->m_i_socket, (const sockaddr *)&toLocal, namelen))
            goto FAILED;
#ifdef WIN32
        {
            u_long iMode = 1;
            if (SOCKET_ERROR == ioctlsocket(this->m_i_socket, FIONBIO, (u_long FAR*) &iMode))
                goto FAILED;
        }
#else
        fcntl(this->m_i_socket, F_SETFL, O_NONBLOCK);
#endif
    }catch (...) {
        goto FAILED;
    }
    return true;

FAILED:
#if defined(WIN32)
    closesocket(this->m_i_socket);
#else
    close(this->m_i_socket);
#endif
    return false;
}

void CSockHandle::PushData(void * pData, int iLength)
{
    SEND_DATA data;
    data.iLength = iLength;

    void * pBuffer = ::malloc(iLength);
    memcpy(pBuffer, pData, iLength);
    data.pData = pBuffer;

    pthread_mutex_lock(&m_SendMutex);
    this->m_SendData.push(data);
    pthread_mutex_unlock(&m_SendMutex);
}

bool CSockHandle::SendData()
{
    bool bRet = true;
    pthread_mutex_lock(&m_SendMutex);
    while (!this->m_SendData.empty()) {
        SEND_DATA data = this->m_SendData.front();
        try{
            int total = 0;
            do{
                int iSent = ::send(this->m_i_socket, (const char *)data.pData + total, data.iLength - total, 0);
                if (SOCKET_ERROR == iSent) {
                    bRet = false;
                    break;
                }
                total += iSent;
            } while (total < data.iLength);
            if (total < data.iLength)
                break;
            this->m_SendData.pop();
            ::free(data.pData);
        }catch (...) {
            bRet = false;
            break;
        }
    }
    pthread_mutex_unlock(&m_SendMutex);
    return bRet;
}

bool CSockHandle::RecvData()
{
    int bytes_read = 0;

    bool bRet = false;
    do{
        try{
            bytes_read = recv(this->m_i_socket, (char *)m_recBuf, m_i_recBufSize, 0);
//            LOGE("pHandle->socket = %d,   bytes_read = %d", this->socket, bytes_read);
        }catch (...) {
            return false;
        }
        if (bytes_read > 0)
            this->pDataReceiver->onReceiveData(m_recBuf, bytes_read);
        else if (SOCKET_ERROR == bytes_read)
           break;
        bRet = true;
    } while (bytes_read > 0);

    return bRet;
}

void CSockHandle::Close()
{
#ifdef WIN32
    closesocket(this->m_i_socket);
#else
    close(this->m_i_socket);
#endif
    while (!this->m_SendData.empty()) {
        SEND_DATA data = this->m_SendData.front();
        this->m_SendData.pop();
        ::free(data.pData);
    }
}

int CSockHandle::GetSocketID()
{
    return m_i_socket;
}
