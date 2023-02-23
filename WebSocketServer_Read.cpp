#include "WebSocketServer.h"
#include <string>
#include "TestWebSocket.h"
#include <algorithm>


using namespace std::chrono;


namespace websocket
{
	int WebRecv(S_WEBSOCKET_BASE* quest,char* buf, int bufLen)
	{
		//接收数据
		memset(quest->unPackBuf, 0, sizeof(quest->unPackBuf));
		memcpy(quest->unPackBuf, buf, bufLen);
		memset(buf, 0, bufLen);
		// 1bit，1表示最后一帧
		char fin = (quest->unPackBuf[0] & 0x80) == 0x80;
		// 超过一帧暂不处理
		if (!fin || bufLen < 2 || bufLen <= 0 || (quest->unPackBuf[0] & 0xF) == 8)
		{
			return 0;
		}
		// 是否包含掩码
		char maskFlag = (quest->unPackBuf[1] & 0x80) == 0x80;
		// 不包含掩码的暂不处理
		if (!maskFlag)
		{
			return NULL;
		}
		char* payloadData = NULL;
		// 数据长度
		unsigned int payloadLen = quest->unPackBuf[1] & 0x7F;
		char masks[4] = { 0 };
		if (payloadLen == 126)
		{
			memcpy(masks, quest->unPackBuf + 4, 4);
			payloadLen = (quest->unPackBuf[2] & 0xFF) << 8 | (quest->unPackBuf[3] & 0xFF);
			payloadLen = bufLen > payloadLen ? payloadLen : bufLen;
			memset(buf, 0, payloadLen);
			memcpy(buf, quest->unPackBuf + 8, payloadLen);
		}
		else if (payloadLen == 127)
		{
			char temp[8] = { 0 };
			memcpy(masks, quest->unPackBuf + 10, 4);
			for (int i = 0; i < 8; i++)
			{
				temp[i] = quest->unPackBuf[9 - i];
			}
			unsigned long n = 0;
			memcpy(&n, temp, 8);
			payloadLen = bufLen > n ? n : bufLen;
			memset(buf, 0, payloadLen);
			memcpy(buf, quest->unPackBuf + 14, payloadLen);//toggle error(core dumped) if data is too long.
		}
		else
		{
			memcpy(masks, quest->unPackBuf + 2, 4);
			payloadLen = bufLen > payloadLen ? payloadLen : bufLen;
			memset(buf, 0, payloadLen);
			memcpy(buf, quest->unPackBuf + 6, payloadLen);
		}


		for (int i = 0; i < payloadLen; i++)
		{
			buf[i] = (char)(buf[i] ^ masks[i % 4]);
		}
		return strlen(buf);
	}
	
	


	//接收数据 粘包处理
	int WebSocketServer::recvSocket(Socket socketfd, S_WEBSOCKET_BASE* quest)
	{
		//memset(quest->tempBuf, 0, MAX_ONES_BUF);
		memset(quest->tempBuf, 0, MAX_ONES_BUF);
		int recvBytes = recv(socketfd, quest->tempBuf, MAX_ONES_BUF, 0);
		if(quest->connectstate >=EC_WEBSOCKET)WebRecv(quest, quest->tempBuf, MAX_ONES_BUF);
		//std::string s = myEngine::base64_decode(quest->temp_str);
		if (recvBytes > 0)
		{
			if(quest->connectstate)
			if (quest->pos_head == quest->pos_tail)
			{
				quest->pos_tail = 0;
				quest->pos_head = 0;
			}
			if (quest->pos_tail + recvBytes >= MAX_BUF) return -1;
			memcpy(&quest->buf[quest->pos_tail], quest->tempBuf, recvBytes);
			quest->pos_tail += recvBytes;
			return 0;
		}
#ifdef ____WIN32_
		if (recvBytes < 0)
		{
			int err = WSAGetLastError();
			if (err == WSAEINTR) return 0;
			else if (err == WSAEWOULDBLOCK) return 0;
			else return -1;
		}
		else if (recvBytes == 0)
		{
			return -2;
		}
#else
		if (recvBytes < 0)  //出错
		{
			if (errno == EINTR) return 0; //被信号中断
			else if (errno == EAGAIN)return 0;//没有数据 请稍后再试
			else return -1;
		}
		else if (recvBytes == 0)
		{
			return -2;
		}
#endif
	}
	int WebSocketServer::analyData(Socket socketfd, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse)
	{
		if (quest->state >= ER_OVER) return 0;

		if (quest->state == ER_HEAD)
		{
			if (quest->method !="GET" )
			{
				quest->state == ER_ERROR;
				//返回错误
				reponse->SetResponseLine(1002, "Failed");
				this->writeData(quest, reponse, "err", 3);
				return -1;
			}
			//读取消息体
			readBody(socketfd, quest, reponse);
			return 0;
		}
		if (quest->state != ER_FREE) return 0;

		//1、没有解析过数据
		int length = quest->pos_tail - quest->pos_head;

		quest->temp_str.clear();
		quest->temp_str.assign(&quest->buf[quest->pos_head], length);
		std::string s = myEngine::base64_decode(quest->temp_str.c_str());
		//2、解析请求行 请求头 请求空行
		int pos = quest->temp_str.find("\r\n\r\n");
		if (pos < 0)
		{

			if (quest->method != "PUT")
			if (length >= MAX_PACKAGE_LENGTH)
			{
				quest->state == ER_ERROR;

				//返回错误
				reponse->SetResponseLine(401, "Failed");
				this->writeData(quest, reponse, "err", 3);
				return -1;
			}

			return 0;
		}
		length = pos + 4;
		quest->temp_str.clear();
		quest->temp_str.assign(&quest->buf[quest->pos_head], length);
		std::vector<std::string> arr = split(quest->temp_str, "\r\n", false);

		//1、请求行数据
		std::vector<std::string> line = split(arr[0], " ", true);
		quest->SetRequestLine(line);
		//2、解析头
		for (int i = 1; i < arr.size() - 1; i++)
		{
			std::vector<std::string>  head = split2(arr[i], ":");
 			if (head.size() == 2)
			{
				//转为小写
				std::string key = "";
				std::transform(head[0].begin(), head[0].end(), std::back_inserter(key), ::tolower);
		
				quest->SetHeader(key, head[1]);
				if (strcmp(key.c_str(), "connection") == 0)
				{
					quest->SetConnection(head[1]);
				}
				if (strcmp(key.c_str(), "upgrade") == 0)
				{
					quest->SetUpgrade(head[1]);
				}
				if (strcmp(key.c_str(), "origin") == 0)
				{
					quest->SetOrigin(head[1]);
				}
				if (strcmp(key.c_str(), "sec-websocket-version") == 0)
				{
					quest->SetWebsocketVersion(head[1]);
				}
				if (strcmp(key.c_str(), "sec-websocket-key") == 0)
				{
					quest->SetWebsocketKey(head[1]);
				}
			}
		}
		//3、设置偏移位置
		quest->pos_head += (pos + 4);

		//输出**************************************************************************************
#ifdef DEBUG_HTTP
		char ftime[30];
	

		LOG_MSG("Quest [%s]==========================%d-%d\n", ftime,(int)socketfd, quest->threadid);
		LOG_MSG("%s %s %s\n", quest->method.c_str(), quest->url.c_str(), quest->version.c_str());

		auto it = quest->head.begin();
		while (it != quest->head.end())
		{
			auto a = it->first;
			auto b = it->second;
			LOG_MSG("%s:%s\n", a.c_str(), b.c_str());
			++it;
		}
		LOG_MSG("\r\n");
		LOG_MSG("%s-%s\n",quest->Connection.c_str(), quest->Key.c_str());
#endif // DEBUG
		//输出**************************************************************************************
		//读取消息体
		readBody(socketfd, quest, reponse);
		return 0;
	}

	//处理消息体
	int WebSocketServer::readBody(Socket socketfd, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse)
	{

		if (strcmp(quest->Upgrade.c_str(), "websocket") == 0 && quest->connectstate < EC_WEBSOCKET)
		{
			webSocket::onWebSocketHandShake(this, quest, reponse);
			return 0;
		}
		else {
			webSocket::onCommand(this, quest, reponse);
 			return 0;
		}
	}
}