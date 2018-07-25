#ifndef __WIFICONTROL_H__
#define __WIFICONTROL_H__

#define ENABLE_SCAN

//typedef   signed		char int8_t;
typedef   signed short	int int16_t;
typedef   signed		int int32_t;

//typedef unsigned		char uint8_t;
typedef unsigned short	int uint16_t;
typedef unsigned		int uint32_t;

typedef enum _ProtocolType
{
	TCP_SERVER = 0,
	TCP_CLIENT = 1,
	UDP_SERVER = 2,
	UDP_CLIENT = 3,
}ProtocolType;

typedef enum _WorkMode
{
	AP_MODE,
	STA_MODE,
}WorkMode;

typedef struct _IdAndPasswd
{
	WorkMode s_workMode;
	char *s_id;
	char *s_passwd;
}IdAndPasswd;

typedef struct _SocketType
{
	ProtocolType s_procotolType;	//协议类型,UDP/TCP Client/Server
	char *s_ipAddress;	//IP地址
	uint16_t s_channelNum;	//通道号
	uint16_t s_port;		//端口
	
}SocketType;

typedef struct _CommMode
{
	uint32_t s_commBandRate;
	unsigned char s_commDataBits;
	unsigned char s_commEndBits;
	unsigned char s_commCheckBits;
}CommMode;

#ifdef ENABLE_SCAN
typedef struct _apNode
{
	unsigned char s_no;
	char s_rssi;
	unsigned char s_channel;
	char s_encryp[16];
	char s_ssid[32];
	struct _apNode *s_next;
}apNode;
#endif

typedef struct _WifiClass
{
	int32_t (*Write)(struct _WifiClass *handle, char *data, uint32_t len);
	
	int32_t (*GetSocket)(struct _WifiClass *handle, SocketType **socket);
	int32_t (*SetSocket)(struct _WifiClass *handle, SocketType *socket);
	
	int32_t (*GetIdAndPasswd)(struct _WifiClass *handle, IdAndPasswd **idAndPasswd_ptr);
	int32_t (*SetIdAndPasswd)(struct _WifiClass *handle, IdAndPasswd *idAndPasswd_ptr);
	
	CommMode* (*GetCommMode)(struct _WifiClass *handle);
	int32_t (*SetCommMode)(struct _WifiClass *handle, CommMode *mode_ptr);
#ifdef ENABLE_SCAN
	int32_t (*ScanAP)(struct _WifiClass *handle, apNode **node);
	int32_t (*ConnectAP)(struct _WifiClass *handle, unsigned char num, char *ssid);
#endif
}WifiClass;

WifiClass *WifiInstance(char *fileName, CommMode *comm_ptr, void (*ReadCallBack)(char *data, uint32_t len));

void WifiDestroy(struct _WifiClass *handle);

#endif
