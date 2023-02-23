#pragma once

#define DEBUG_HTTP 0

#ifdef ____WIN32_
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using Socket = SOCKET;
#else
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h> 
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
using Socket = int;
#endif

#include <thread>
#include <mutex>
#include <list>
#include <map>
#include <vector>
#include <condition_variable>
#include <base.h>
#include "sha1/sha1.h"
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#define LOG_MSG printf 

#define MAX_EXE_LEN          200
#define MAX_KEEP_ALIVE       3005
#define MAX_THREAD_COUNT     8
#define MAX_PACKAGE_LENGTH   1024

#define MAX_POST_LENGTH      1024*32

#define MAX_BUF              1024*256
#define MAX_ONES_BUF         1024*20

#define LOG_MSG printf 
#ifndef S_ISREG
#define S_ISREG(m) (((m)&S_IFREG) == S_IFREG)
#endif // S_ISREG

namespace websocket
{
	extern std::string deleteString(std::string s, char c);//删除空字符串

	enum E_RECV_STATE
	{
		ER_FREE = 0x00,
		ER_HEAD = 0x01,
		ER_OVER = 0x02,
		ER_ERROR = 0x03,
	};
	enum E_SEND_STATE
	{
		ES_FREE = 0x00,
		ES_SENDING = 0x01,
		ES_OVER = 0x02
	};
	enum E_CONNECT_STATE
	{
		EC_FREE = 0x00,
		EC_CONNECT = 0x01,
		EC_WEBSOCKET = 0x02
	};
	enum E_LOADPUT_STATE
	{
		EL_FREE = 0x00,
		EL_PUT = 0x01,
		EL_LOAD= 0x02,
	};
#pragma pack(push,packing)
#pragma pack(1)
	struct S_TEST_1000
	{
		int  cmd;
		int  level;
		char name[20];
	};

	//请求和响应数据结构体
	struct S_WEBSOCKET_BASE
	{
		int   state;
		int   connectstate;
		char  buf[MAX_BUF];
		char  tempBuf[MAX_ONES_BUF];
		char  unPackBuf[MAX_ONES_BUF];
		std::string  temp_str;
		int   pos_head;
		int   pos_tail;
		int   threadid;
		std::string   method;//方法
		std::string   url;//统一资源定位符
		std::string   version;//版本号
		std::unordered_map<std::string, std::string> head;//头
		std::string Connection;    //设置连接
		std::string  Upgrade;//设置链接模式
		std::string  Origin;//来源网站
		std::string  Version;//版本
		std::string  Key;//客户端发来的key
		std::string  AcceptStr;//服务器返回的key
		//响应数据
		int   status;
		std::string   describe;//描述

		inline void Reset()
		{
			memset(buf, 0, MAX_BUF);
			memset(tempBuf, 0, MAX_ONES_BUF);
			memset(unPackBuf, 0, MAX_ONES_BUF);
			state = 0;
			status = -1;
			pos_head = 0;
			pos_tail = 0;
			method = "";
			url = "";
			version = "";
			Connection = "";
			head.clear();
			threadid = 0;
			connectstate = 0;
			Upgrade ="";//设置链接模式
			Origin="";//来源网站
			Version="";//版本
			Key="";//客户端发来的key
			AcceptStr="";//服务器返回的key

		}
		inline void Init()
		{
			Reset();
		}

		//设置请求行
		inline void SetRequestLine(std::vector<std::string>& line)
		{
			if (line.size() != 3) return;
			method = line[0];
			url = line[1];
			version = line[2];
		}
		//设置头
		inline void SetHeader(std::string key, std::string value)
		{
			auto it = head.find(key);
			if (it != head.end())
			{
				head.erase(it);
			}
			head.insert(std::make_pair(key, value));
		}
		inline std::string GetHeader(std::string key)
		{
			auto it = head.find(key);
			if (it != head.end())
			{
				return it->second;
			}
			return "";
		}

	
		inline void SetConnection(std::string value)
		{
			auto s = deleteString(value, ' ');
			Connection = s;
		}
		inline void SetUpgrade(std::string value)
		{
			auto s = deleteString(value, ' ');
			Upgrade = s;
		}
		inline void SetOrigin(std::string value)
		{
			auto s = deleteString(value, ' ');
			Origin = s;
		}
		inline void SetWebsocketVersion(std::string value)
		{
			auto s = deleteString(value, ' ');
			Version = s;
		}
		//握手解析生成accept
		inline void SetWebsocketKey(std::string value)
		{
			auto s = deleteString(value, ' ');
			std::string server_key =s;
			server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			MY_SHA1 sha;
			unsigned int message_digest[5];

			sha << server_key.c_str();
			sha.Result(message_digest);

			for (int i = 0; i < 5; ++i)
				message_digest[i] = htonl(message_digest[i]);
			AcceptStr = myEngine::base64_encode(reinterpret_cast<const unsigned char*>(message_digest), 20);
		
		}
		//***********************************************************
        //设置响应行
		inline void SetResponseLine(int stat, std::string s)
		{
			status = stat;
			version = "HTTP/1.1";
			describe = s;
		}
	};

#pragma pack(pop, packing)

	extern void log_UpdateConnect(int a, int b);
	extern std::vector<std::string> split(std::string str, std::string pattern, bool isadd = false);
	extern std::vector<std::string> split2(std::string str, std::string pattern);
	extern bool is_file(const std::string& path);
	extern void read_file(const std::string& path, std::string& out);
	extern bool read_Quest(const std::string& path, std::string& out);
	extern int writeFile(std::string filename, char* c, int len);
	extern void initPath();
	extern bool setNonblockingSocket(Socket socketfd);
	extern int select_isread(Socket socketfd, int timeout_s, int timeout_u);
	extern char* Utf8ToUnicode(const char* szU8);
	extern void UnicodeToUtf8(const wchar_t* unicode, char* src);
}