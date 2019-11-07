//
// Created by Francesco Laurita on 5/30/16.
//

#include <arpa/inet.h>
#include <netdb.h>
#include "SpeedTestClient.h"

SpeedTestClient::SpeedTestClient(const ServerInfo &serverInfo): mServerInfo(serverInfo), mSocketFd(0), mServerVersion(-1.0) {
}

SpeedTestClient::~SpeedTestClient() {
	close();
}

// It connects and initiates client/server handshaking
bool SpeedTestClient::connect() {
	if (mSocketFd)
		return true;

	auto ret = mkSocket();
	if (!ret)
		return ret;

	if (!SpeedTestClient::writeLine(mSocketFd, "HI")) {
		close();
		return false;
	}

	std::string reply;
	if (SpeedTestClient::readLine(mSocketFd, reply)) {
		std::stringstream reply_stream(reply);
		std::string hello;
		reply_stream >> hello >> mServerVersion;
		if (!reply_stream.fail() && "HELLO" == hello)
			return true;
	}

	close();
	return false;
}

// It closes a connection
void SpeedTestClient::close() {
	if (mSocketFd) {
		SpeedTestClient::writeLine(mSocketFd, "QUIT");
		::close(mSocketFd);
	}
}

// It executes PING command
bool SpeedTestClient::ping(long &millisec) {
	millisec = LONG_MAX;
	auto start = std::chrono::high_resolution_clock::now();
	std::stringstream cmd;
	cmd << "PING " << start.time_since_epoch().count() << "\n";
	if (!SpeedTestClient::writeLine(mSocketFd, cmd.str()))
		return false;

	std::string reply;
	//start = std::chrono::high_resolution_clock::now();
	if (SpeedTestClient::readLine(mSocketFd, reply)) {
		if (reply.substr(0, 5) == "PONG ") {
			auto stop = std::chrono::high_resolution_clock::now();
			millisec = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
			return true;
		}
	}

	return false;
}

// It executes DOWNLOAD command
bool SpeedTestClient::download(const long size, const long chunk_size, long &millisec) {
	millisec = LONG_MAX;
	std::stringstream cmd;
	cmd << "DOWNLOAD " << size << "\n";
	if (!SpeedTestClient::writeLine(mSocketFd, cmd.str()))
		return false;

	char *buff = new char[chunk_size];
	for (size_t i = 0; i < static_cast<size_t>(chunk_size); i++)
		buff[i] = '\0';

	long missing = 0;
	auto start = std::chrono::high_resolution_clock::now();
	while (missing < size) {
		auto current = read(mSocketFd, buff, static_cast<ssize_t>(chunk_size));
		if (current < 1) {
			delete[] buff;
			return false;
		}
		missing += current;
	}
	auto stop = std::chrono::high_resolution_clock::now();
	millisec = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
	delete[] buff;

	return missing == size;
}

// It executes UPLOAD command
bool SpeedTestClient::upload(const long size, const long chunk_size, long &millisec) {
	millisec = LONG_MAX;
	std::stringstream cmd;
	cmd << "UPLOAD " << size << "\n";
	if (!SpeedTestClient::writeLine(mSocketFd, cmd.str()))
		return false;

	char *buff = new char[chunk_size];
	for (size_t i = 0; i < static_cast<size_t>(chunk_size); i++)
		buff[i] = static_cast<char>(rand() % 256);

	long missing = size - cmd.str().length();
	ssize_t len;
	auto start = std::chrono::high_resolution_clock::now();
	while (missing > 0) {
		if (missing - chunk_size > 0) {
			len = static_cast<ssize_t>(chunk_size);
		} else {
			len = static_cast<ssize_t>(missing);
			buff[missing - 1] = '\n';
		}
		auto n = write(mSocketFd, buff, len);
		if (n != len) {
			delete[] buff;
			return false;
		}
		missing -= n;
	}
	auto stop = std::chrono::high_resolution_clock::now();
	millisec = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
	delete[] buff;

	std::stringstream ss;
	ss << "OK " << size << " ";
	std::string reply;
	if (!SpeedTestClient::readLine(mSocketFd, reply))
		return false;

	return reply.substr(0, ss.str().length()) == ss.str();
}

bool SpeedTestClient::mkSocket() {
	mSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (!mSocketFd)
		return false;

	auto hostp = hostport();
#if __APPLE__
	struct hostent *server = gethostbyname(hostp.first.c_str());
	if (server == nullptr)
		return false;
#else
	struct hostent server;
	char tmpbuf[BUFSIZ];
	struct hostent *result;
	int errnop;
	if (gethostbyname_r(hostp.first.c_str(), &server, (char *)&tmpbuf, BUFSIZ, &result, &errnop))
		return false;
#endif

	int portno = hostp.second;
	struct sockaddr_in serv_addr{};
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
#if __APPLE__
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, (size_t)server->h_length);
#else
	memcpy(&serv_addr.sin_addr.s_addr, server.h_addr, (size_t)server.h_length);
#endif
	serv_addr.sin_port = htons(static_cast<uint16_t>(portno));

	/* Dial */
	return ::connect(mSocketFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0;
}

float SpeedTestClient::version() {
	return mServerVersion;
}

const std::pair<std::string, int> SpeedTestClient::hostport() {
	std::string targetHost = mServerInfo.host;
	std::size_t found = targetHost.find(':');
	std::string host = targetHost.substr(0, found);
	std::string port = targetHost.substr(found + 1, targetHost.length() - found);
	return std::pair<std::string, int>(host, std::atoi(port.c_str()));
}

bool SpeedTestClient::readLine(int &fd, std::string &buffer) {
	if (!fd)
		return false;

	buffer.clear();
	char c;
	while (true) {
		auto n = read(fd, &c, 1);
		if (n < 1)
			return false;
		if (c == '\n' || c == '\r')
			break;
		buffer += c;
	}
	return buffer.length() > 0;
}

bool SpeedTestClient::writeLine(int &fd, const std::string &buffer) {
	if (!fd)
		return false;

	auto len = static_cast<ssize_t>(buffer.length());
	if (len == 0)
		return false;
	std::string buff_copy = buffer;
	if (buff_copy.find_first_of('\n') == std::string::npos) {
		buff_copy += '\n';
		len += 1;
	}
	auto n = write(fd, buff_copy.c_str(), len);
	return n == len;
}