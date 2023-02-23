#include "WebSocketServer.h"
#ifdef ____WIN32_
#include <MSWSock.h>
using namespace std::chrono;
#endif

#include <chrono>
using namespace std::chrono;


namespace websocket
{
	websocket::WebSocketServer::WebSocketServer()
	{
		m_bWbStar = true;
	}


	websocket::WebSocketServer::~WebSocketServer()
	{
#ifdef ____WIN32_
		if (m_Listenfd != INVALID_SOCKET)
		{
			closesocket(m_Listenfd);
			m_Listenfd = INVALID_SOCKET;
		}
		WSACleanup();
#else
		close(m_Listenfd);
#endif
	}


	//��ʼ��socket
	int WebSocketServer::InitSocket()
	{
#ifdef ____WIN32_
		//1����ʼ��Windows Sockets DLL
		WSADATA  wsData;
		int errorcode = WSAStartup(MAKEWORD(2, 2), &wsData);
		if (errorcode != 0) return -1;
#endif
		//2����������socket
		m_Listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (m_Listenfd < 0) return -2;

		//3������Ngle�㷨 û���ӳ�
		int value = 1;
		setsockopt(m_Listenfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&value), sizeof(value));

		//4�����÷�����
		setNonblockingSocket(m_Listenfd);

		//5�������˿ں��ظ�ʹ��
		int flag = 1;
		int ret = setsockopt(m_Listenfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&flag), sizeof(int));

		//6����IP �˿�
		struct sockaddr_in serAddr;
		memset(&serAddr, 0, sizeof(sockaddr_in));

		serAddr.sin_family = AF_INET;
		serAddr.sin_port = htons(8080);
#ifdef ____WIN32_
		serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
#else
		serAddr.sin_addr.s_addr = INADDR_ANY;
#endif
		int err = ::bind(m_Listenfd, (struct sockaddr*) & serAddr, sizeof(serAddr));
		if (err < 0) return -3;

		//7������
		err = listen(m_Listenfd, SOMAXCONN);
		if (err < 0) return -4;

		return 0;
	}



	void websocket::WebSocketServer::runServer()
	{
		initPath();
		m_ConnectCount = 0;
		//1����ʼ��socket
		int err = InitSocket();
		if (err < 0)
		{
#ifdef ____WIN32_
			if (m_Listenfd != INVALID_SOCKET)
			{
				closesocket(m_Listenfd);
				m_Listenfd = INVALID_SOCKET;
			}
			WSACleanup();
#else
			close(m_Listenfd);
#endif

			LOG_MSG("InitSocket err....%d\n", err);
			return;
		}
		//2����ʼ������ ��Ӧ����
		for (int i = 0; i < MAX_THREAD_COUNT; i++)
		{
			m_Request[i] = new S_WEBSOCKET_BASE();
			m_Response[i] = new S_WEBSOCKET_BASE();
			m_Request[i]->Reset();
			m_Response[i]->Reset();
		
		}

		//3�������̳߳�
		runThread();

		//4������һ���̼߳����µ�����
		std::thread  th(&WebSocketServer::runAccept, this);
		th.detach();

		//5���������̼߳����µ�����
		//runAccept();
	}


	//���̼߳����µ����� ���ɷ�����
	void WebSocketServer::runAccept()
	{
		while (true)
		{
			int value = select_isread(m_Listenfd, 0, 5000);
			if (value == 0) continue;

			socklen_t clilen = sizeof(struct sockaddr);
			struct sockaddr_in clientaddr;

			int socketfd = accept(m_Listenfd, (struct sockaddr*) & clientaddr, &clilen);
			if (socketfd < 0)
			{
				
				if (errno == EMFILE)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));

					LOG_MSG("errno == EMFILE...\n");
					continue;
				}
				LOG_MSG("errno %d %d-%d\n", m_Listenfd, socketfd, errno);
				break;
			}

			{
				std::unique_lock<std::mutex> guard(this->m_Mutex);
				this->m_Socketfds.push_back(socketfd);
			}
			{
				std::unique_lock<std::mutex> guard(this->m_ConnectMutex);
				m_ConnectCount++;
			}
			//�����ӡ
			log_UpdateConnect(this->m_ConnectCount, this->m_Socketfds.size());

			this->m_Condition.notify_one();
		}
	}

	//*****************************************************
	//*****************************************************
	//��ʼ���̳߳�
	void WebSocketServer::runThread()
	{
		//�����߳�
		for (int i = 0; i < MAX_THREAD_COUNT; i++)
			m_Thread[i].reset(new std::thread(WebSocketServer::run, this, i));

		//����
		for (int i = 0; i < MAX_THREAD_COUNT; i++)
			m_Thread[i]->detach();
	}





	void WebSocketServer::run(WebSocketServer* s, int id)
	{

		int socketfd = -1;
		while (true)
		{
			{
				std::unique_lock<std::mutex>   guard(s->m_Mutex);
				while (s->m_Socketfds.empty())
				{
					//LOG_MSG("************************ thread wait....%d\n", id);
					s->m_Condition.wait(guard);
				}
				socketfd = s->m_Socketfds.front();
				s->m_Socketfds.pop_front();

				//LOG_MSG("************************ thread awake....%d-%d\n", (int)socketfd, id);
				//�����ӡ
				log_UpdateConnect(s->m_ConnectCount, s->m_Socketfds.size());
			}
			//��ʼ����socketfd
			s->runSocket(socketfd, id);
		}
	}

	//�ڹ����߳� �����µ����ӵ�����
	void WebSocketServer::runSocket(Socket socketfd, int tid)
	{
#ifdef ____WIN32_
		//������ shutdown���û᲻�ɹ�
		setsockopt(socketfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&m_Listenfd), sizeof(m_Listenfd));
#endif
		//1�����÷�����socket
		setNonblockingSocket(socketfd);
		//3������Ngle�㷨 û���ӳ�
		int value = 1;
		setsockopt(m_Listenfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&value), sizeof(value));

		auto quest = m_Request[tid];
		auto reponse = m_Response[tid];
		quest->Reset();
		reponse->Reset();
		quest->threadid = tid;
		reponse->threadid = tid;

		std::string closestr;
		auto start = std::chrono::steady_clock::now();
		while (true)
		{
			//0�����޿ɶ�����
			int err = select_isread(socketfd, 0, 2000);
			if (err < 0)
			{
				closestr = "select_isread";
				break;
			}
			if (err == 0)
			{
				continue;
			}
			//1���Ӱ� ճ��
			if (err > 0)
			{
				err = recvSocket(socketfd, quest);
				if (err < 0)
				{
					closestr = "recvSocket";
					break;
				}
			}
			//2�����
			if (quest->state <= ER_HEAD && quest->connectstate< EC_WEBSOCKET)
			{
				//������ //����WEBSOCKET����
				analyData(socketfd, quest, reponse);
			}
			else if(quest->connectstate >= EC_WEBSOCKET)
			{
				//�����Ѿ���� �շ�����
				readBody(socketfd, quest, reponse);
			}
			//3������
			err = sendSocket(socketfd, reponse, tid);
			if (err < 0)
			{
				closestr = "sendSocket";
				break;
			}
			//4��������ϣ�
			if (reponse->state == ES_OVER)
			{
				reponse->state = ES_FREE;
				if (quest->state == ER_ERROR)
				{
					closestr = "quest error";
					quest->Init();
					break;
				}
				if (quest->Connection == "close")
				{
					closestr = "close";
					quest->Init();
					break;
				}
			}
			if (quest->connectstate < EC_WEBSOCKET)
			{
				closestr = "close less EC_WEBSOCKET";
				quest->Init();
				break;
			}

			//***********************************************************
			//�������ʱ��
		/*	auto current  = std::chrono::steady_clock::now();
			auto duration = duration_cast<milliseconds>(current - start);
			int max_alive_time = MAX_KEEP_ALIVE;
			if (quest->loadput == EL_FREE)
			{
				max_alive_time = MAX_KEEP_ALIVE;
			}
			else
			{
				max_alive_time = 10 * 60 * 1000;
			}
			if (duration.count() > max_alive_time)
			{
				closestr = "timeout";
				break;
			}*/
		}

		//����������
		{
			std::unique_lock<std::mutex> guard(this->m_ConnectMutex);
			m_ConnectCount--;
#ifdef DEBUG_HTTP
			LOG_MSG("closesocket:%s  connect:%d-%d \n", closestr.c_str(), m_ConnectCount, (int)m_Socketfds.size());
#endif
		}

		//������������Լ�
		log_UpdateConnect(this->m_ConnectCount, this->m_Socketfds.size());
#ifdef ____WIN32_
		shutdown(socketfd, SD_BOTH);
		closesocket(socketfd);
#else
		shutdown(socketfd, SHUT_RDWR);
		close(socketfd);
#endif

	}
}

