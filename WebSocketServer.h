#pragma once
#include "IDefine_WebSocket.h"
namespace websocket
{
	class WebSocketServer
	{
	public:
		bool            m_bWbStar;
	private:
		int             m_ConnectCount; //��������
		Socket          m_Listenfd; //����socket
		std::mutex      m_Mutex;//������
		std::mutex      m_ConnectMutex;//���ӻ�����
		std::list<int>  m_Socketfds;//�µ���������
		std::condition_variable         m_Condition;//��������
		std::shared_ptr<std::thread>    m_Thread[MAX_THREAD_COUNT];

		S_WEBSOCKET_BASE* m_Request[MAX_THREAD_COUNT];
		S_WEBSOCKET_BASE* m_Response[MAX_THREAD_COUNT];

		int  recvSocket(Socket socketfd, S_WEBSOCKET_BASE* quest);
		int  analyData(Socket socketfd, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse);
		int  readBody(Socket socketfd, S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse);
	public:
		WebSocketServer();
		virtual ~WebSocketServer();

		int InitSocket();
		void runAccept();
		void runServer();
		void runThread();
		void runSocket(Socket socketfd, int tid);

		void writeData(S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse, const char* body, int size);
		void writeWebSocketData(S_WEBSOCKET_BASE* quest, S_WEBSOCKET_BASE* reponse, const char* body, int size);
		int  sendSocket(Socket socketfd, S_WEBSOCKET_BASE* reponse, int tid);

		static void run(WebSocketServer* s, int id);

	};
}

