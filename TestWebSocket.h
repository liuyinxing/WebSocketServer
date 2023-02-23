#pragma once
#include "WebSocketServer.h"
using namespace websocket;

namespace webSocket
{
	extern void onCommand(WebSocketServer* server, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse);
	extern void onWebSocketHandShake(WebSocketServer* server, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse);
	
}