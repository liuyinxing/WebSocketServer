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


	//初始化socket
	int WebSocketServer::InitSocket()
	{
#ifdef ____WIN32_
		//1、初始化Windows Sockets DLL
		WSADATA  wsData;
		int errorcode = WSAStartup(MAKEWORD(2, 2), &wsData);
		if (errorcode != 0) return -1;
#endif
		//2、创建监听socket
		m_Listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (m_Listenfd < 0) return -2;

		//3、禁用Ngle算法 没有延迟
		int value = 1;
		setsockopt(m_Listenfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&value), sizeof(value));

		//4、设置非阻塞
		setNonblockingSocket(m_Listenfd);

		//5、启动端口号重复使用
		int flag = 1;
		int ret = setsockopt(m_Listenfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&flag), sizeof(int));

		//6、绑定IP 端口
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

		//7、监听
		err = listen(m_Listenfd, SOMAXCONN);
		if (err < 0) return -4;

		return 0;
	}



	void websocket::WebSocketServer::runServer()
	{
		initPath();
		m_ConnectCount = 0;
		//1、初始化socket
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
		//2、初始化请求 响应数据
		for (int i = 0; i < MAX_THREAD_COUNT; i++)
		{
			m_Request[i] = new S_WEBSOCKET_BASE();
			m_Response[i] = new S_WEBSOCKET_BASE();
			m_Request[i]->Reset();
			m_Response[i]->Reset();
		
		}

		//3、运行线程池
		runThread();

		//4、开启一个线程监听新的连接
		std::thread  th(&WebSocketServer::runAccept, this);
		th.detach();

		//5、运行主线程监听新的连接
		//runAccept();
	}


	//主线程监听新的连接 并派发任务
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
			//输出打印
			log_UpdateConnect(this->m_ConnectCount, this->m_Socketfds.size());

			this->m_Condition.notify_one();
		}
	}

	//*****************************************************
	//*****************************************************
	//初始化线程池
	void WebSocketServer::runThread()
	{
		//运行线程
		for (int i = 0; i < MAX_THREAD_COUNT; i++)
			m_Thread[i].reset(new std::thread(WebSocketServer::run, this, i));

		//分离
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
				//输出打印
				log_UpdateConnect(s->m_ConnectCount, s->m_Socketfds.size());
			}
			//开始处理socketfd
			s->runSocket(socketfd, id);
		}
	}

	//在工作线程 处理新的连接的数据
	void WebSocketServer::runSocket(Socket socketfd, int tid)
	{
#ifdef ____WIN32_
		//不设置 shutdown调用会不成功
		setsockopt(socketfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&m_Listenfd), sizeof(m_Listenfd));
#endif
		//1、设置非阻塞socket
		setNonblockingSocket(socketfd);
		//3、禁用Ngle算法 没有延迟
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
			//0、有无可读数据
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
			//1、接包 粘包
			if (err > 0)
			{
				err = recvSocket(socketfd, quest);
				if (err < 0)
				{
					closestr = "recvSocket";
					break;
				}
			}
			//2、解包
			if (quest->state <= ER_HEAD && quest->connectstate< EC_WEBSOCKET)
			{
				//解析包 //建立WEBSOCKET链接
				analyData(socketfd, quest, reponse);
			}
			else if(quest->connectstate >= EC_WEBSOCKET)
			{
				//链接已经完成 收发数据
				readBody(socketfd, quest, reponse);
			}
			//3、发包
			err = sendSocket(socketfd, reponse, tid);
			if (err < 0)
			{
				closestr = "sendSocket";
				break;
			}
			//4、发送完毕？
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
			//生存最大时间
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

		//上锁连接数
		{
			std::unique_lock<std::mutex> guard(this->m_ConnectMutex);
			m_ConnectCount--;
#ifdef DEBUG_HTTP
			LOG_MSG("closesocket:%s  connect:%d-%d \n", closestr.c_str(), m_ConnectCount, (int)m_Socketfds.size());
#endif
		}

		//输出队列人数以及
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

