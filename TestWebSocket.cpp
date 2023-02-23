#include "TestWebSocket.h"
#include "json.hpp"
using Json = nlohmann::json;

enum WS_FrameType
{
	WS_EMPTY_FRAME = 0xF0,
	WS_ERROR_FRAME = 0xF1,
	WS_TEXT_FRAME = 0x01,
	WS_BINARY_FRAME = 0x02,
	WS_PING_FRAME = 0x09,
	WS_PONG_FRAME = 0x0A,
	WS_OPENING_FRAME = 0xF3,
	WS_CLOSING_FRAME = 0x08
};
int WebSend(std::string inMessage, std::string& outFrame, WS_FrameType frametypes)
{
	int ret = WS_EMPTY_FRAME;
	const uint32_t messageLength = inMessage.size();
	if (messageLength > 32767)
	{
		// 暂不支持这么长的数据
		LOG_MSG("暂不支持这么长的数据");
		return WS_ERROR_FRAME;
	}

	uint8_t payloadFieldExtraBytes = (messageLength <= 0x7d) ? 0 : 2;
	// header: 2字节, mask位设置为0(不加密), 则后面的masking key无须填写, 省略4字节  
	uint8_t frameHeaderSize = 2 + payloadFieldExtraBytes;
	uint8_t* frameHeader = new uint8_t[frameHeaderSize];
	memset(frameHeader, 0, frameHeaderSize);
	// fin位为1, 扩展位为0, 操作位为frameType  
	frameHeader[0] = static_cast<uint8_t>(0x80 | frametypes);

	// 填充数据长度  
	if (messageLength <= 0x7d)
	{
		frameHeader[1] = static_cast<uint8_t>(messageLength);
	}
	else
	{
		frameHeader[1] = 0x7e;
		uint16_t len = htons(messageLength);
		memcpy(&frameHeader[2], &len, payloadFieldExtraBytes);
	}

	// 填充数据  
	uint32_t frameSize = frameHeaderSize + messageLength;
	char* frame = new char[frameSize + 1];
	memcpy(frame, frameHeader, frameHeaderSize);
	memcpy(frame + frameHeaderSize, inMessage.c_str(), messageLength);
	frame[frameSize] = '\0';
	outFrame = frame;

	delete[] frame;
	delete[] frameHeader;
	return ret;
}

void websocket_login(WebSocketServer* server, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse, Json& js, int cmd)
{
	std::string err = std::to_string(cmd);
	std::string context;
	int errcode=WebSend(err, context, WS_TEXT_FRAME);
	if (errcode < 0)
	{
		quest->state = ER_ERROR;
		return;
	};
	server->writeData(quest, reponse, context.c_str(), context.length());
	quest->temp_str.clear();
}

void webSocket::onCommand(WebSocketServer* server, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse)
{
		quest->state = ER_OVER;
		quest->temp_str.clear();
		quest->temp_str = quest->tempBuf;
		//不允许抛出错误？
		Json js = Json::parse(quest->temp_str,nullptr,false,false);
		if (js.is_object() == false) 
		{
			reponse->SetResponseLine(1003, "Failed");
        	server->writeData(quest, reponse, "not object", 10);
			return;
		}
		auto js_cmd = js["cmd"];
		if (js_cmd.is_number_integer() == false) 
		{
			reponse->SetResponseLine(1003, "Failed");
			server->writeData(quest, reponse, "not int", 7);
			return;
		}		
		int cmd = js_cmd;
		switch (cmd)
		{
		case 10://登录
			websocket_login(server, quest, reponse, js, cmd);
			break;
		}
}

void webSocket::onWebSocketHandShake(WebSocketServer* server, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse)
{
	{
		if (strcmp(quest->Connection.c_str(), "Upgrade") != 0)
		{
			reponse->SetResponseLine(1002, "Failed");
			server->writeData(quest, reponse, "Connection err", 7);
			return;
		}
		if (strcmp(quest->Upgrade.c_str(), "websocket") != 0)
		{
			reponse->SetResponseLine(1002, "Failed");
			server->writeData(quest, reponse, "Upgrade err", 7);
			return;
		}
		quest->temp_str.clear();
		quest->connectstate = EC_WEBSOCKET ;
		reponse->SetResponseLine(101, "Switching Protocols");
		server->writeWebSocketData(quest, reponse, "", 0);
		quest->temp_str.clear();

	}
}