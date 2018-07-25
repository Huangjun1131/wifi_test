#include "WifiControl.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <assert.h>
#include "queue.h"
#include "ddebug.h"

#define RSP_NONE 	0
#define RSP_OK		1
#define RSP_ERR 	2
#define RSP_SOCKET	3
#define RSP_APSTA	4
#define RSP_UART	5
#define RSP_SCAN	6

#define ALIVE_NONE	0
#define ALIVE_PASS	1
#define ALIVE_CMD	2

#define COMM_ENTER_AT	"+++"			
#define COMM_EXIT_AT	"AT#Exit\r\n"
#define COMM_SOCKET		"AT#Socket\r\n"	
#define COMM_APSTA		"AT#APSTA\r\n"
#define COMM_APID		"AT#APID\r\n"
#define COMM_APSEC		"AT#APSec\r\n"
#define COMM_STAID		"AT#STAID\r\n"
#define COMM_STASEC		"AT#STASec\r\n"
#define COMM_UART		"AT#Uart\r\n"
#define COMM_SCAN		"AT#Scan\r\n"

#define REPLY_OK		"OK"			
#define REPLY_ERR		"ERROR"			

#define BUF_MAX_SIZE	1024

typedef enum _InteractiveMode
{
	SEND_COMMAND,
	SEND_PASSTHROUGH,
}InteractiveMode;

typedef struct _WifiInfo{
	int32_t s_fd;			//串口文件描述符
	char *s_fileName;		//串口文件名
	char *s_macAddress;	//物理地址
	char *s_gateway;		//网关地址
	char *s_netmask;		//子网掩码
	uint16_t s_encryption;	//加密方式
	char *s_dnsAddress;	//dns服务器地址
	SocketType s_socketType; //协议(UDP/TCP Client Server)类型 IP地址和端口号 通道号
	
	IdAndPasswd s_idAndPasswd;	//主机模式/从机模式 下的ID和密码
#ifdef ENABLE_SCAN
	apNode *s_apNode;
#endif

	CommMode s_commMode;	//串口通讯参数

	WifiClass s_handle;		//操作句柄
	
	InteractiveMode s_interactiveMode;	//交互方式,透传或者命令模式
	unsigned char s_alive;		//指示当前的交互方式,
	unsigned char s_setmode;		
	unsigned char s_response;		//命令模式下收到的响应类型
	pthread_t s_thrd;		//线程句柄
	pthread_mutex_t s_mut;	//线程锁
	pthread_cond_t s_cond;	//线程调节变量
	QUEUE	s_queue;		//命令解析所使用的缓冲队列
	void (*ReadCallBack)(char *data, uint32_t len);	//透传数据处理的回调函数
}WifiInfo;

static int32_t SetEnterMode(WifiClass *handle, InteractiveMode mode);

uint32_t GetQueueAvail(QUEUE *pQueue)
{
	int32_t i;
	uint32_t availLen = QueueDataSize(pQueue);
	if (availLen < 2)
	{
		return 0;
	}

	for (i = 0; i < availLen - 1; i++)
	{
//		DDEBUG(P_NORMAL, "availLen is %d, pos is %d, value is %d\r\n",availLen, i, QueueByteAccess(pQueue, i));
//		if (QueueByteAccess(pQueue, i) == '\r' && QueueByteAccess(pQueue, i+1) == '\n')
		if (QueueByteAccess(pQueue, i+1) == '\n')
		
		{
			break;
		}
	}

	return (i == availLen - 1)?0:i;
}

static void ATCommandProcess(WifiInfo *devInfo, char *data, int32_t len)
{
	assert(devInfo != NULL);

	uint32_t availLen;
	char *frameBuf = (char *)malloc(BUF_MAX_SIZE);
	char *pStart;

	data[len] = '\0';
	DDEBUG(P_NORMAL, "[WIFI][%s]recv command:%s\r\n", __func__, data);
	QueueWrite(&devInfo->s_queue, (unsigned char *)data, len);

	while ((availLen = GetQueueAvail(&devInfo->s_queue)) > 0)
	{
		DDEBUG(P_NORMAL, "[WIFI][%s]queue read %d\r\n", __func__, availLen);
		QueueRead(&devInfo->s_queue, (unsigned char *)frameBuf, availLen + 2);
		frameBuf[availLen+2] = 0;
		if (strstr(frameBuf, "OK"))
		{
			DDEBUG(P_NORMAL, "[WIFI][%s]strstr get OK\r\n", __func__);
			pthread_mutex_lock(&devInfo->s_mut);
			devInfo->s_response = RSP_OK;
			pthread_cond_signal(&devInfo->s_cond);
			pthread_mutex_unlock(&devInfo->s_mut);
		}
		else if (strstr(frameBuf, "ERROR"))
		{
			DDEBUG(P_NORMAL, "[WIFI][%s]strstr get ERR\r\n", __func__);
			pthread_mutex_lock(&devInfo->s_mut);
			devInfo->s_response = RSP_ERR;
			pthread_cond_signal(&devInfo->s_cond);
			pthread_mutex_unlock(&devInfo->s_mut);
		}
		else 
		{
			if (!strncmp(frameBuf, "-C", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]we caught a -C\r\n", __func__);
				if (NULL != (pStart = strstr(frameBuf,"-C")))
				{
					devInfo->s_socketType.s_channelNum = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf,"-A")))
				{
					strncpy(devInfo->s_socketType.s_ipAddress, pStart+2, 17);
					if (NULL != (pStart = strchr(devInfo->s_socketType.s_ipAddress, ' ')))
					{
						*pStart = '\0';
					}
				}
				if (NULL != (pStart = strstr(frameBuf, "-P")))
				{
					devInfo->s_socketType.s_port = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf, "-M")))
				{	
					//'0' for Server , '1' for client
					if (*(pStart+2) == '1')
					{
						devInfo->s_socketType.s_procotolType |= 1;
					}
				}
				if (NULL != (pStart = strstr(frameBuf, "-T")))
				{
					//'0' for TCP , '1' for UDP
					if (*(pStart+2) == '1')
					{
						devInfo->s_socketType.s_procotolType |= 2;
					}
				}
				DDEBUG(P_NORMAL, "[WIFI][%s]IP address is %s\r\n", __func__, devInfo->s_socketType.s_ipAddress);
				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_SOCKET;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);
			}
			else if (!strncmp(frameBuf, "-M", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]Caught a -M\r\n", __func__);
				if (NULL != (pStart = strstr(frameBuf,"-M")))
				{
					if (*(pStart+2) == '0')
					{
						devInfo->s_idAndPasswd.s_workMode = STA_MODE;
					}
					else
					{
						devInfo->s_idAndPasswd.s_workMode = AP_MODE;
					}
				}
				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_APSTA;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);
			}
			else if (!strncmp(frameBuf, "-S", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]Caught a -S\r\n", __func__);
				if (NULL != (pStart = strstr(frameBuf,"-S")))
				{
					strcpy(devInfo->s_idAndPasswd.s_id, pStart+2);
				}
				if (NULL != (pStart = strchr(devInfo->s_idAndPasswd.s_id, '\r')))
				{
					*pStart = '\0';
				}
				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_APSTA;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);
			}
			else if (!strncmp(frameBuf, "-K", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]Caught a -K\r\n", __func__);
				if (NULL != (pStart = strstr(frameBuf,"-K")))
				{
					strcpy(devInfo->s_idAndPasswd.s_passwd, pStart+2);
				}
				if (NULL != (pStart = strchr(devInfo->s_idAndPasswd.s_passwd, '\r')))
				{
					*pStart = '\0';
				}
				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_APSTA;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);
			}
			else if (!strncmp(frameBuf, "-B", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]Caught a -B\r\n", __func__);
				if (NULL != (pStart = strstr(frameBuf, "-B")))
				{
					devInfo->s_commMode.s_commBandRate = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf, "-D")))
				{
					devInfo->s_commMode.s_commDataBits = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf, "-P")))
				{
					devInfo->s_commMode.s_commCheckBits = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf, "-S")))
				{
					devInfo->s_commMode.s_commEndBits = atoi(pStart+2);
				}

				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_UART;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);
			}
#ifdef ENABLE_SCAN
			else if (!strncmp(frameBuf, "-I", 2))
			{
				DDEBUG(P_NORMAL, "[WIFI][%s]Caught a -I\r\n", __func__);
				apNode *node = (apNode *)malloc(sizeof(apNode));
				memset(node, 0, sizeof(apNode));
				apNode *pNode;
				apNode *pNext;
				apNode **ppNode;
				if (NULL != (pStart = strstr(frameBuf, "-I")))
				{
					node->s_no = atoi(pStart+2);
					//如果收到的是第一个,而且之前链表中有数据,先把之前的数据全部干掉.
					if (node->s_no == 1 && devInfo->s_apNode != NULL)
					{
						pNode = devInfo->s_apNode;
						devInfo->s_apNode = NULL;
						DDEBUG(P_NORMAL,"[WIFI][%s] clean these data\r\n", __func__);
						while(pNode != NULL)
						{
							pNext = pNode->s_next;
							free(pNode);
							pNode = pNext;
						}
					}
				}
				if (NULL != (pStart = strstr(frameBuf,"-R")))
				{
					node->s_rssi = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf,"-C")))
				{
					node->s_channel = atoi(pStart+2);
				}
				if (NULL != (pStart = strstr(frameBuf,"-T")))
				{
					strncpy(node->s_encryp, pStart+2, 15);
					if (NULL != (pStart = strchr(node->s_encryp, '\t')))
					{
						*pStart = '\0';
					}
				}
				if (NULL != (pStart = strstr(frameBuf,"-S")))
				{
					strncpy(node->s_ssid, pStart+2, 31);
					if (NULL != (pStart = strchr(node->s_ssid, '\r')))
					{
						*pStart = '\0';
					}
				}
				DDEBUG(P_NORMAL, "[WIFI][%s]scan:no %d rs %d ch %d ss %s en %s\r\n", 
						__func__,
						node->s_no,
						node->s_rssi,
						node->s_channel,
						node->s_ssid,
						node->s_encryp
						);
				pNode = devInfo->s_apNode;
				if (pNode == NULL)
				{
					devInfo->s_apNode = node;
				}
				else 
				{
					while(pNode->s_next != NULL)
					{
						pNode = pNode->s_next;
					}
					ppNode = &(pNode->s_next);
					*ppNode = node;
				}
				pthread_mutex_lock(&devInfo->s_mut);
				devInfo->s_response = RSP_SCAN;
				pthread_cond_signal(&devInfo->s_cond);
				pthread_mutex_unlock(&devInfo->s_mut);

			}
#endif
		}
	}
	free(frameBuf);
}


int32_t WriteWaitResponse(WifiInfo *devInfo, char *data, uint32_t len)
{
	struct timeval now;
	struct timespec timeout;
	int retcode;
	int ret;
	/* 1.获取线程互斥锁;
	 * 2.写入AT指令;
	 * 3.等待被唤醒,或者超时;
	 * 4.如果超时,则返回-1,回复OK,则返回0,回复ERR,返回1,其他返回2
	 * 5.释放线程锁	
	 * */
	DDEBUG(P_NORMAL, "[WIFI][%s]we write a command for response:%s\r\n",__func__, data);
	pthread_mutex_lock(&devInfo->s_mut);
	write(devInfo->s_fd, data, len);
	gettimeofday(&now, NULL);
	timeout.tv_sec = now.tv_sec + 2;
	timeout.tv_nsec = now.tv_usec * 1000;
	retcode = 0;
	devInfo->s_response = RSP_NONE;
	while (!devInfo->s_response && retcode != ETIMEDOUT) {
		retcode = pthread_cond_timedwait(&devInfo->s_cond, &devInfo->s_mut, &timeout);
	}
	if (retcode == ETIMEDOUT) 
	{
		DDEBUG(P_WARNING, "[WIFI][%s] Did not get a response\r\n",__func__);
		devInfo->s_alive = ALIVE_NONE;
		ret = -1;
		/* timeout occurred */
	} 
	else 
	{
		DDEBUG(P_WARNING, "[WIFI][%s] We get a response at %d .\r\n", __func__, devInfo->s_response);
		if (devInfo->s_response == RSP_OK)
		{
			ret = 0;
		
		}
		else if (devInfo->s_response == RSP_ERR)
		{
			ret = 1;
		}
		else
		{
			ret = 2;
		}
		/* operate on x and y */
	}
	pthread_mutex_unlock(&devInfo->s_mut);
	DDEBUG(P_NORMAL, "[WIFI][%s]return %d\r\n", __func__, ret);

	return ret;
}
static int32_t OpenDev(WifiInfo *devInfo, char *fileName)
{
	int32_t fd;
	struct termios oldstdio;
	int32_t baud;

	switch (devInfo->s_commMode.s_commBandRate)
	{
	case 115200:
		baud = B115200;
		break;
	case 9600:
		baud = B9600;
		break;
	default:
		baud = B115200;
		break;
	}
	fd = open(fileName, O_RDWR | O_NOCTTY);
	if (-1 == fd)
	{
		DDEBUG(P_WARNING, "[WIFI][%s]cannot open %s\r\n", __func__, fileName);
		return -1;
	}
	tcgetattr(fd, &oldstdio);
	cfsetispeed(&oldstdio, baud);
	tcsetattr(fd, TCSANOW, &oldstdio);
	tcflush(fd, TCIFLUSH);

	DDEBUG(P_NORMAL, "[WIFI][%s]%s open with BAUD %d\r\n",__func__, fileName, B115200);
	return fd;
}

static int32_t CloseDev(int32_t fd)
{
	if (fd < 0)
	{
		DDEBUG(P_NORMAL, "[WIFI][%s]dev is not opened\r\n",__func__);
		return -1;
	}
	close(fd);
	return 0;
}
/***********************************************
 * 用户输入通过串口透传方式发送出去,
 * 如果是以AT# 开头则认为是AT指令,为其加上\r\n,
 * 然后通过AT指令方式下发
 ***********************************************/
static int32_t WriteDev(WifiClass *handle, char *data, uint32_t len)
{
	assert(handle != NULL);

	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	if (devInfo->s_fd < 0)
	{
		DDEBUG(P_WARNING, "[WIFI][%s]file not open, cannot write\r\n", __func__);
		return -1;
	}
	if (!strncmp(data, "AT#", 3))
	{
		DDEBUG(P_WARNING, "[WIFI][%s]It is a command, add end\r\n",__func__);
		strcpy(data+len, "\r\n");
		len += 2;
		SetEnterMode(handle, SEND_COMMAND);
		write(devInfo->s_fd, data, len);
		sleep(1);
		SetEnterMode(handle, SEND_PASSTHROUGH);
	}
	return write(devInfo->s_fd, data, len);
}

/**************************************************
 * 接收到串口数据,并进行处理
 * 如果当前设置为透传模式,直接调用用户的回调函数,
 * 如果是AT命令模式,则调用自己的数据解析函数
 **************************************************/
void *RecvDataProcess(void *arg)
{
	WifiInfo *devInfo = arg;
	int32_t epfd, nfds;
	int32_t t_i = 0;
	struct epoll_event ev = {0}, events[2] = {{0},{0}};
#define READ_BUFFER_SIZE 1024
	char *readBuffer = (char *)malloc(READ_BUFFER_SIZE);
	uint32_t readLen;

	epfd = epoll_create(10);
	ev.data.fd = devInfo->s_fd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, devInfo->s_fd, &ev);

	while (devInfo->s_alive)
	{
		nfds = epoll_wait(epfd, events, 20, 500);
		if (nfds > 0 && events[t_i].data.fd == devInfo->s_fd)
		{
			readLen = read(devInfo->s_fd, readBuffer, READ_BUFFER_SIZE);
			if (devInfo->s_alive == ALIVE_PASS)
			{
				DDEBUG(P_NORMAL, "[WIFI][%s] recv %d at pass\r\n", __func__, readLen);
				if (devInfo->ReadCallBack)
				{
					devInfo->ReadCallBack(readBuffer, readLen);
				}
			}
			else
			{
				DDEBUG(P_NORMAL, "[WIFI][%s] recv %d at cmd\r\n", __func__, readLen);
				ATCommandProcess(devInfo, readBuffer, readLen);
			}
		}
		else if (nfds == 0)
		{
			//timeout
		}
		else
		{
			//error occur
		}
	}
	free(readBuffer);
	DDEBUG(P_NORMAL, "[WIFI][%s]data process is over\r\n",__func__);
	return NULL;
}

static int32_t SetEnterMode(WifiClass *handle, InteractiveMode mode)
{
	assert(handle != NULL);

	uint32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	DDEBUG(P_WARNING, "[WIFI][%s][%d]Enter mode %s\r\n", __func__, __LINE__, mode==SEND_COMMAND?"At command":"Passthrought");
	devInfo->s_alive = ALIVE_CMD;
	if (mode == SEND_COMMAND)
	{
		ret = WriteWaitResponse(devInfo, COMM_ENTER_AT, strlen(COMM_ENTER_AT));
	}
	else
	{
		ret = WriteWaitResponse(devInfo, COMM_EXIT_AT, strlen(COMM_EXIT_AT));
	}
	if (ret >=0)
	{
		devInfo->s_alive = (mode == SEND_COMMAND)?ALIVE_CMD:ALIVE_PASS;
	}
	return ret;
}

static int32_t GetSocketDev(struct _WifiClass *handle, SocketType **socket)
{
	assert(handle != NULL);

	/* 1.检查当前的IP是否填写,如果填写了直接返回,
	 * 2.如果没有填写,则发送AT指令得到IP,并返回
	 * */
	uint32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	DDEBUG(P_NORMAL, "[WIFI][%s]called %d\r\n", __func__, __LINE__);
//	if (!devInfo->s_socketType.s_ipAddress)
//	{
		/* 1.进入AT指令模式,
		 * 2.发送AT#Socket
		 * 3.等待解析正确
		 * 4.进入透传模式
		 * 5.返回IP
		 * */
		SetEnterMode(handle, SEND_COMMAND);
		pthread_mutex_lock(&devInfo->s_mut);
		devInfo->s_response = RSP_NONE;
		pthread_mutex_unlock(&devInfo->s_mut);
		ret = WriteWaitResponse(devInfo, COMM_SOCKET, strlen(COMM_SOCKET));
		if (ret < 0 || devInfo->s_response != RSP_SOCKET)
		{
			SetEnterMode(handle, SEND_PASSTHROUGH);
			socket = NULL;
			return -1;
		}
		SetEnterMode(handle, SEND_PASSTHROUGH);
//	}
	
	*socket = &devInfo->s_socketType;
	return 0;
}
int32_t SetSocketDev(struct _WifiClass *handle, SocketType *socket)
{
	assert(handle != NULL);

	int32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	char *cmdBuffer = (char *)malloc(128);
	DDEBUG(P_NORMAL, "[WIFI][%s] called\r\n", __func__);
	if (socket == NULL)
	{
		return -1;
	}
	if (devInfo->s_socketType.s_ipAddress == NULL)
	{
		
	}
	strncpy(devInfo->s_socketType.s_ipAddress, socket->s_ipAddress, 16);
	devInfo->s_socketType.s_procotolType = socket->s_procotolType;
	devInfo->s_socketType.s_channelNum = socket->s_channelNum;
	devInfo->s_socketType.s_port = socket->s_port;
	
	sprintf(cmdBuffer, "AT#Socket -c%d -t%d -m%d -a%s -p%d\r\n",
			devInfo->s_socketType.s_channelNum,
			(devInfo->s_socketType.s_procotolType & 0x02) >>1,
			devInfo->s_socketType.s_procotolType & 0x01,
			devInfo->s_socketType.s_ipAddress,
			devInfo->s_socketType.s_port
			);
	DDEBUG(P_NORMAL, "[WIFI][%s] going to send cmd %s\r\n", __func__, cmdBuffer);
	/* 1.进入AT指令模式,
	 * 2.发送AT#Socket
	 * 3.等待解析正确
	 * 4.进入透传模式
	 * 5.返回IP
	 * */
	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);
	ret = WriteWaitResponse(devInfo, cmdBuffer, strlen(cmdBuffer));
	if (ret < 0 || devInfo->s_response != RSP_OK)
	{
		ret = -1;
	}
	SetEnterMode(handle, SEND_PASSTHROUGH);
	free(cmdBuffer);
	return ret;
}

int32_t GetIdAndPasswdDev(struct _WifiClass *handle, IdAndPasswd **idAndPasswd_ptr)
{
	assert(handle != NULL);

	uint32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	
	/* 1.进入AT指令模式
	 * 2.发送AT获取当前是AP或者STA
	 * 3.根据第二步的结果进行查询ID和PASSWD
	 * 4.进入透传模式
	 * 5.返回
	 * */

	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);

	ret = WriteWaitResponse(devInfo, COMM_APSTA, strlen(COMM_APSTA));
	if (ret < 0 || devInfo->s_response != RSP_APSTA)
	{
		goto ERR;
	}
	if (devInfo->s_idAndPasswd.s_workMode == AP_MODE)
	{
		ret = WriteWaitResponse(devInfo, COMM_APID, strlen(COMM_APID));
		ret = WriteWaitResponse(devInfo, COMM_APSEC, strlen(COMM_APSEC));
	}
	else
	{
		ret = WriteWaitResponse(devInfo, COMM_STAID, strlen(COMM_STAID));
		ret = WriteWaitResponse(devInfo, COMM_STASEC, strlen(COMM_STASEC));
	}
	*idAndPasswd_ptr = &devInfo->s_idAndPasswd;
	SetEnterMode(handle, SEND_PASSTHROUGH);
	return 0;
ERR:
	SetEnterMode(handle, SEND_PASSTHROUGH);
	*idAndPasswd_ptr = NULL;
	return -1;
}
int32_t SetIdAndPasswdDev(struct _WifiClass *handle, IdAndPasswd *idAndPasswd_ptr)
{
	assert(handle != NULL);

	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	char *cmdBuffer = (char *)malloc(64);
	int32_t ret;

	devInfo->s_idAndPasswd.s_workMode = idAndPasswd_ptr->s_workMode;
	strcpy(devInfo->s_idAndPasswd.s_passwd, idAndPasswd_ptr->s_passwd);
	strcpy(devInfo->s_idAndPasswd.s_id, idAndPasswd_ptr->s_id);
	/* 1.进入AT命令模式
	 * 2.设置对应的ID和PASSWD
	 * 3.进入透传模式
	 * */
	SetEnterMode(handle, SEND_COMMAND);
	if (devInfo->s_idAndPasswd.s_workMode == AP_MODE)
	{
		sprintf(cmdBuffer, "AT#APID -s%s\r\n", devInfo->s_idAndPasswd.s_id);
	}
	else
	{
		sprintf(cmdBuffer, "AT#STAID -s%s\r\n", devInfo->s_idAndPasswd.s_id);
	}
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);
	ret = WriteWaitResponse(devInfo, cmdBuffer, strlen(cmdBuffer));
	if (ret < 0 || devInfo->s_response != RSP_OK)
	{
		goto ERR_SETID1;
	}
	if (devInfo->s_idAndPasswd.s_workMode == AP_MODE)
	{
		sprintf(cmdBuffer, "AT#APSec -k%s\r\n", devInfo->s_idAndPasswd.s_passwd);
	}
	else
	{
		sprintf(cmdBuffer, "AT#STASec -k%s\r\n", devInfo->s_idAndPasswd.s_passwd);
	}
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);
	ret = WriteWaitResponse(devInfo, cmdBuffer, strlen(cmdBuffer));
	if (ret < 0 || devInfo->s_response != RSP_OK)
	{
		goto ERR_SETID2;
	}
	free(cmdBuffer);
	SetEnterMode(handle, SEND_PASSTHROUGH);
	return 0;
ERR_SETID2:
	free(cmdBuffer);
ERR_SETID1:
	SetEnterMode(handle, SEND_PASSTHROUGH);
	return -1;
}

CommMode *GetCommModeDev(struct _WifiClass *handle)
{
	assert(handle != NULL);

	int32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	DDEBUG(P_NORMAL, "[WIFI][%s] get in %d\r\n", __func__, __LINE__);
	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);

	ret = WriteWaitResponse(devInfo, COMM_UART, strlen(COMM_UART));
	if (ret < 0 || devInfo->s_response != RSP_UART)
	{
		SetEnterMode(handle, SEND_PASSTHROUGH);
		return NULL;
	}
	SetEnterMode(handle, SEND_PASSTHROUGH);
	return &devInfo->s_commMode;
}
int32_t SetCommModeDev(struct _WifiClass *handle, CommMode *mode_ptr)
{
	assert(handle != NULL);

	struct termios oldstdio;
	int32_t baud;
	int32_t ret;
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	char *cmdBuffer = (char *)malloc(64);

	devInfo->s_commMode.s_commBandRate = mode_ptr->s_commBandRate;
	devInfo->s_commMode.s_commDataBits = mode_ptr->s_commDataBits;
	devInfo->s_commMode.s_commCheckBits = mode_ptr->s_commCheckBits;
	devInfo->s_commMode.s_commEndBits = mode_ptr->s_commEndBits;

	/* 1.进入AT命令模式;
	 * 2.改变模块的波特率;
	 * 4.改变串口通信的波特率;
	 * 4.进入透传模式
	 * */

	DDEBUG(P_NORMAL, "[WIFI][%s] get in %d\r\n", __func__, __LINE__);
	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);
	sprintf(cmdBuffer, "AT#Uart -b%d -d%d -p%d -s%d\r\n",
			devInfo->s_commMode.s_commBandRate,	
			devInfo->s_commMode.s_commDataBits,
			devInfo->s_commMode.s_commCheckBits,
			devInfo->s_commMode.s_commEndBits);
	ret = WriteWaitResponse(devInfo, cmdBuffer, strlen(cmdBuffer));
	if (ret < 0 || devInfo->s_response == RSP_ERR)
	{
		free(cmdBuffer);
		return -1;
	}

	switch (devInfo->s_commMode.s_commBandRate)
	{
	case 115200:
		baud = B115200;
		break;
	case 9600:
		baud = B9600;
		break;
	default:
		baud = B115200;
		break;
	}

	
	tcgetattr(devInfo->s_fd, &oldstdio);
	cfsetispeed(&oldstdio, baud);
	tcsetattr(devInfo->s_fd, TCSANOW, &oldstdio);
	tcflush(devInfo->s_fd, TCIFLUSH);

	return 0;
}
#ifdef ENABLE_SCAN
int32_t ScanAPDev(struct _WifiClass *handle, apNode **node)
{
	assert(handle != NULL);

	int32_t ret;
	WifiInfo  *devInfo = container_of(handle, WifiInfo, s_handle);

	/* 1.进入AT指令模式;
	 * 2.发送Scan指令;
	 * 3.等待回应;
	 * 4.延时5s;
	 * 5.进入透传模式.
	 * */
	DDEBUG(P_NORMAL, "[WIFI][%s] get in %d\r\n", __func__, __LINE__);
	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);

	ret = WriteWaitResponse(devInfo, COMM_SCAN, strlen(COMM_SCAN));
	if (ret < 0 || devInfo->s_response != RSP_SCAN)
	{
		SetEnterMode(handle, SEND_PASSTHROUGH);
		*node = NULL;
		return -1;
	}
	DDEBUG(P_NORMAL, "[WIFI][%s] scan recv something, wait to compelete\r\n", __func__);
	sleep(8);
	SetEnterMode(handle, SEND_PASSTHROUGH);
	*node = devInfo->s_apNode;
	return 0;
}
int32_t ConnectAPDev(struct _WifiClass *handle, unsigned char num, char *ssid)
{
	assert(handle != NULL);

	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	int32_t ret;
	char *cmdBuffer = (char *)malloc(64);
	/* 1.进入AT命令模式;
	 * 2.设置连接;
	 * 3.进入透传模式;
	 * */
	DDEBUG(P_NORMAL, "[WIFI][%s] get in %d\r\n", __func__, __LINE__);
	SetEnterMode(handle, SEND_COMMAND);
	pthread_mutex_lock(&devInfo->s_mut);
	devInfo->s_response = RSP_NONE;
	pthread_mutex_unlock(&devInfo->s_mut);
	sprintf(cmdBuffer, "AT#Connect -I%d -K%s\r\n", num, ssid);
	ret = WriteWaitResponse(devInfo, cmdBuffer, strlen(cmdBuffer));
	if (ret < 0 || devInfo->s_response == RSP_ERR)
	{
		free(cmdBuffer);
		return -1;
	}
	free(cmdBuffer);
	SetEnterMode(handle, SEND_PASSTHROUGH);
	return 0;
}
#endif
WifiClass *WifiInstance(char *fileName, CommMode *comm_ptr, void (*ReadCallBack)(char *data, uint32_t len))
{
	void *ret;
	WifiInfo *devInfo = (WifiInfo *)malloc(sizeof(WifiInfo));
	memset(devInfo, 0, sizeof(WifiInfo));

	if (comm_ptr != NULL)
	{
		memcpy(&devInfo->s_commMode, comm_ptr, sizeof(CommMode));
	}
	else
	{
		devInfo->s_commMode.s_commBandRate = 115200;
		devInfo->s_commMode.s_commDataBits = 8;
		devInfo->s_commMode.s_commEndBits = 1;
		devInfo->s_commMode.s_commCheckBits = 0;
	}
	devInfo->ReadCallBack = ReadCallBack;

	devInfo->s_fd = OpenDev(devInfo, fileName);
	if (devInfo->s_fd < 0)
	{
		goto ERR_HANDLE;
	}
	
	devInfo->s_socketType.s_ipAddress = (char *)malloc(18);
	memset(devInfo->s_socketType.s_ipAddress, 0, 18);
	devInfo->s_idAndPasswd.s_id = (char *)malloc(32);
	devInfo->s_idAndPasswd.s_passwd = (char *)malloc(32);

	devInfo->s_handle.Write = WriteDev;
	devInfo->s_handle.GetSocket = GetSocketDev;
	devInfo->s_handle.SetSocket = SetSocketDev;
	devInfo->s_handle.GetIdAndPasswd = GetIdAndPasswdDev;
	devInfo->s_handle.SetIdAndPasswd = SetIdAndPasswdDev;
	devInfo->s_handle.GetCommMode = GetCommModeDev;
	devInfo->s_handle.SetCommMode = SetCommModeDev;
#ifdef ENABLE_SCAN
	devInfo->s_handle.ScanAP = ScanAPDev;
	devInfo->s_handle.ConnectAP = ConnectAPDev;
#endif
	devInfo->s_alive = ALIVE_CMD;

	unsigned char *queueBuffer = (unsigned char *)malloc(BUF_MAX_SIZE);

	QueueInit(&devInfo->s_queue, queueBuffer, BUF_MAX_SIZE);

	pthread_cond_init(&devInfo->s_cond, NULL);
	pthread_mutex_init(&devInfo->s_mut, NULL);
	pthread_create(&devInfo->s_thrd, NULL, RecvDataProcess, devInfo);

	usleep(100000);

	if (SetEnterMode(&devInfo->s_handle, SEND_COMMAND) < 0)
	{
		goto ERR_HANDLE2;
	}
	SetEnterMode(&devInfo->s_handle, SEND_PASSTHROUGH);
	return &devInfo->s_handle;
ERR_HANDLE2:
	DDEBUG(P_WARNING, "ERR_HANDLE2\r\n");
	devInfo->s_alive = ALIVE_NONE;
	free(queueBuffer);
	free(devInfo->s_socketType.s_ipAddress);
	pthread_join(devInfo->s_thrd, &ret);
	CloseDev(devInfo->s_fd);
ERR_HANDLE:
	DDEBUG(P_WARNING, "ERR_HANDLE\r\n");
	free(devInfo);
	return NULL;
}

void WifiDestroy(struct _WifiClass *handle)
{
	WifiInfo *devInfo = container_of(handle, WifiInfo, s_handle);
	void *ret;
	apNode *pNode,*pNext;	
	
	devInfo->s_alive = ALIVE_NONE;
	free(devInfo->s_queue.pStart);
	free(devInfo->s_socketType.s_ipAddress);
	pNode = devInfo->s_apNode;
	devInfo->s_apNode = NULL;
	DDEBUG(P_NORMAL,"[WIFI][%s] clean these data\r\n", __func__);
	while(pNode != NULL)
	{
		pNext = pNode->s_next;
		free(pNode);
		pNode = pNext;
	}
	pthread_join(devInfo->s_thrd, &ret);
	CloseDev(devInfo->s_fd);
	free(devInfo);
}
