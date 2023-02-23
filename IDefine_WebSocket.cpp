#include "IDefine_WebSocket.h"
#ifdef ____WIN32_
#else
#include<sys/stat.h>
#include<unistd.h>
#endif

namespace websocket
{
	char FileExePath[MAX_EXE_LEN];
	void initPath()
	{
		memset(FileExePath, 0, MAX_EXE_LEN);
#ifdef ____WIN32_
		GetModuleFileNameA(NULL, (LPSTR)FileExePath, MAX_EXE_LEN);

		std::string str(FileExePath);
		size_t pos = str.find_last_of("\\");
		str = str.substr(0, pos + 1);
		memcpy(FileExePath, str.c_str(), MAX_EXE_LEN);
		printf("FileExePath:%s \n", FileExePath);
#else
		int ret = readlink("/proc/self/exe", FileExePath, MAX_EXE_LEN);
		std::string str(FileExePath);
		size_t pos = str.find_last_of("/");
		str = str.substr(0, pos + 1);
		memcpy(FileExePath, str.c_str(), MAX_EXE_LEN);

		printf("FileExePath:%s \n", FileExePath);
#endif

	}

	//删除空字符串
	std::string deleteString(std::string s, char c)
	{
		std::string sss;
		if (!s.empty())
		{
			int index = 0;
			while ((index = s.find(c, index)) >= 0)
			{
				s.erase(index, 1);
			}

			sss = s;
		}
		return sss;
	}

	void log_UpdateConnect(int a, int b)
	{
#ifdef ____WIN32_
		//char ss[50];
		//memset(ss, 0, 50);
		//sprintf_s(ss, "connect-%d queue-%d", a, b);
		//SetWindowTextA(GetConsoleWindow(), ss);
#endif
	}
	std::vector<std::string> split(std::string src, std::string pattern, bool isadd)
	{
		std::string::size_type pos = 0;
		std::vector<std::string> arr;
		if (isadd) src += pattern;
	
		int size = src.size();
		for (int i = 0; i < size; i++)
		{
			pos = src.find(pattern, i);
			if (pos < size)
			{
				std::string s = src.substr(i, (pos - i));
				arr.push_back(s);
				i = pos + pattern.size() - 1;
			}
		}

		return arr;
	}
	std::vector<std::string> split2(std::string src, std::string pattern)
	{
		std::string::size_type pos = 0;
		std::vector<std::string> arr;
		int size = src.size();

		pos = src.find(pattern);
		if (pos < 0) return arr;
		
		std::string s = src.substr(0, pos);
		arr.push_back(s);

		if (pos + 1 >= size) return arr;

		s = src.substr(pos+1);
		arr.push_back(s);

		return arr;
	}
	bool is_file(const std::string& path)
	{
		struct stat st;
		return stat(path.c_str(), &st) >= 0 && S_ISREG(st.st_mode);
	}
	//读取文件
	void read_file(const std::string& path, std::string& out)
	{
		std::ifstream fs(path, std::ios_base::binary);
		fs.seekg(0, std::ios_base::end);

		auto size = fs.tellg();
		fs.seekg(0);
		out.resize(static_cast<size_t>(size));
		fs.read(&out[0], static_cast<std::streamsize>(size));
	}

	//读取请求文件
	bool read_Quest(const std::string& filename, std::string& out)
	{
		std::string filepath(FileExePath);
#ifdef ____WIN32_
		std::string sub_path = filepath + "res\\" + filename;
#else
		std::string sub_path = filepath + "res/" + filename;
#endif

		if (sub_path.back() == '/') { sub_path += "index.html"; }
		if (is_file(sub_path))
		{
			read_file(sub_path, out);
			return true;
		}
		return false;
	}
	int writeFile(std::string filename, char* c, int len)
	{
		std::string fff(FileExePath);
#ifdef ____WIN32_
		std::string path = fff + "res\\" + filename;
#else
		std::string path = fff + "res/" + filename;
#endif
		std::ofstream fs(path, std::ios_base::binary);
		if (!fs) return -1;
		fs.write(c, len);
		
		return 0;
	}
	bool setNonblockingSocket(Socket socketfd)
	{
#ifdef ____WIN32_
		unsigned long ul = 1;
		int err = ioctlsocket(socketfd, FIONBIO, (unsigned long*)& ul);
		if (err == SOCKET_ERROR) return false;
		return true;
#else
		int flags = fcntl(socketfd, F_GETFL);
		if (flags < 0) return false;
		flags |= O_NONBLOCK;

		if (fcntl(socketfd, F_SETFL, flags) < 0) //设置文件状态标志
			return false;
		return true;
#endif
	}
	int select_isread(Socket socketfd, int timeout_s, int timeout_u)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(socketfd, &fds);

		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 1000 * 5;

		while (true)
		{
			int err = select(socketfd + 1, &fds, NULL, NULL, &tv);
			if (err < 0 && errno == EINTR) continue;
			return err;
		}

		return -1;
	}

	char* Utf8ToUnicode(const char* szU8)
	{
#ifdef ____WIN32_
		//UTF8 to Unicode
		//预转换，得到所需空间的大小
		int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), NULL, 0);
		//分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
		wchar_t* wszString = new wchar_t[wcsLen + 1];
		//转换
		::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), wszString, wcsLen);
		//最后加上'\0'
		wszString[wcsLen] = '\0';

		char* m_char;
		int len = WideCharToMultiByte(CP_ACP, 0, wszString, wcslen(wszString), NULL, 0, NULL, NULL);
		m_char = new char[len + 1];
		WideCharToMultiByte(CP_ACP, 0, wszString, wcslen(wszString), m_char, len, NULL, NULL);
		m_char[len] = '\0';
		return m_char;
#else
		return "";
#endif
	}
	void UnicodeToUtf8(const wchar_t* unicode, char* src)
	{
#ifdef ____WIN32_
		int len;
		len = WideCharToMultiByte(CP_UTF8, 0, unicode, -1, NULL, 0, NULL, NULL);
		WideCharToMultiByte(CP_UTF8, 0, unicode, -1, src, len, NULL, NULL);
#else
#endif
	}
}