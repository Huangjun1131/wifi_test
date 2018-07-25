#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "WifiControl.h"
#include "ddebug.h"

void ReadCallBack(char *data, uint32_t len)
{
	data[len] = 0;
	uint32_t i;
	for (i = 0; i < len; i++)
	{
		printf("%d:%d ", i, data[i]);
	}
	printf("recv:%s\r\n", data);
}

int main(int argc, const char *argv[])
{
	char readBuf[100];
	CommMode comm = {
		115200,
		8,
		1,
		0,
	};
	WifiClass *handle = WifiInstance("/dev/ttyUSB0", &comm, ReadCallBack);
	if (handle == NULL)
	{
		return -1;
	}
	while(1)
	{
		fgets(readBuf, 100, stdin);
		if (!strncmp(readBuf, "bye", 3))
		{
			break;
		}
		if (!strncmp(readBuf, "NWK", 3))
		{
			SocketType *socket = NULL;
			handle->GetSocket(handle, &socket);
			if (socket)
			{
				printf("Main get ip:%s, port %d, pro %s, channel %d\r\n",socket->s_ipAddress, socket->s_port, 
						socket->s_procotolType==TCP_SERVER?"TCP SERVER":
						socket->s_procotolType==TCP_CLIENT?"TCP CLIENT":
						socket->s_procotolType==UDP_SERVER?"UDP_SERVER":"UDP_CLIENT"
						, socket->s_channelNum);
			}
			continue;
		}
		if (!strncmp(readBuf, "TCP", 3))
		{
			SocketType sck = {
				TCP_SERVER,
				"192.168.2.1",
				0,
				8888,
			};
			handle->SetSocket(handle, &sck);
			continue;
		}
		if (!strncmp(readBuf, "UDP", 3))
		{
			SocketType sck = {
				UDP_SERVER,
				"192.168.1.1",
				0,
				9999,
			};
			handle->SetSocket(handle, &sck);
			continue;
		}
		if (!strncmp(readBuf, "IPC", 3))
		{
			IdAndPasswd *ptr;
			handle->GetIdAndPasswd(handle, &ptr);
			printf("main workmode %s, id %s, passwd %s\r\n", ptr->s_workMode==AP_MODE?"AP MODE":"STA MODE", ptr->s_id, ptr->s_passwd);
			continue;
		}

		if (!strncmp(readBuf, "APD", 3))
		{
			IdAndPasswd idpass = {
				AP_MODE,
				"my_ap",
				"22223333"
			};	
			handle->SetIdAndPasswd(handle, &idpass);
			continue;
		}
		if (!strncmp(readBuf, "STA", 3))
		{
			IdAndPasswd idpass = {
				STA_MODE,
				"skkh",
				"1234567891"
			};	
			handle->SetIdAndPasswd(handle, &idpass);
			continue;
		}
		if (!strncmp(readBuf, "UAR", 3))
		{
			CommMode *comm = handle->GetCommMode(handle);
			if (comm)
			{
				printf("uart baud is %d, datalen is %d, check is %d, stop is %d\r\n",	\
						comm->s_commBandRate, comm->s_commDataBits,						\
						comm->s_commCheckBits, comm->s_commEndBits
						);
			}
			continue;
		}
		if (!strncmp(readBuf, "9600", 4))
		{
			CommMode com = {
				9600,
				8,
				1,
				0,
			};
			handle->SetCommMode(handle, &com);
			continue;
		}
		if (!strncmp(readBuf, "115200", 6))
		{
			CommMode com = {
				115200,
				8,
				1,
				0,
			};
			handle->SetCommMode(handle, &com);
			continue;
		}
#ifdef ENABLE_SCAN
		if (!strncmp(readBuf, "SCAN", 4))
		{
			apNode *node;
			handle->ScanAP(handle, &node);
			while (node)
			{
				printf("scan:no %d rs %d ch %d en %s ss %s\r\n", 
						node->s_no,
						node->s_rssi,
						node->s_channel,
						node->s_encryp,
						node->s_ssid
					  );
				node = node->s_next;
			}
		}
		if (!strncmp(readBuf, "CONN", 4))
		{
			char pswd[32];
			uint32_t num;
			printf("Input the num you want to connect:");
			scanf("%d", &num);
			printf("Input the password:");
			scanf("%s", pswd);
			handle->ConnectAP(handle, num, pswd);
		}
#endif
		handle->Write(handle, readBuf, strlen(readBuf));
	}
	WifiDestroy(handle);
	return 0;
}
