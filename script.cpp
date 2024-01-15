#pragma comment (lib, "ws2_32.lib")

#include "script.h"
#include <string>
#include <string.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <charconv>
#include <thread>
#include <mutex>

const int SUBTITLES_ARR_SIZE = 512;
const int SERVER_PORT = 32123;

struct s_subtitle {
	char* text;
	long zero1;
	long unk1;
	long long unk2;
	bool flag1;
	bool flag2;
	bool flag3;
	bool flag4;
	long zero2;
	long long zero3;
	long long zero4;
};

s_subtitle* subtitles;
char** subtitlesBuffer = new char* [SUBTITLES_ARR_SIZE];
std::vector<SOCKET> tcpClients;
std::mutex tcpClientsLock;
bool isRunning = true;

void runTcpServer()
{
	sockaddr_in servAddr;
	memset((char*)&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(SERVER_PORT);

	int serverSd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSd < 0)
		return;

	int bindStatus = bind(serverSd, (struct sockaddr*)&servAddr, sizeof(servAddr));
	if (bindStatus < 0)
		return;

	if (listen(serverSd, 5) < 0)
		return;

	while (isRunning)
	{
		auto tcpClient = accept(serverSd, NULL, NULL);
		if (tcpClient < 0)
			continue;

		std::lock_guard<std::mutex> lock(tcpClientsLock);
		tcpClients.push_back(tcpClient);
	}
}

bool isStringASubtitle(int idx)
{
	const char* subtitlePrefix = "~z~";
	return strstr(subtitles[idx].text, subtitlePrefix) != NULL;
}

int checkStringsIdx(int idx1, int idx2)
{
	if (subtitles[idx1].text == NULL)
		return -1;

	if (!isStringASubtitle(idx1))
		return -1;

	if (subtitles[idx2].text == NULL)
		return idx1;

	if (strstr(subtitles[idx2].text, subtitles[idx1].text) != NULL)
		return idx2;

	return idx1;
}

int sendSubtitle(SOCKET tcpClient, int idx)
{
	auto sub = subtitles[idx];
	uint32_t len = strlen(sub.text);

	char* buffer = new char[sizeof(len) + len];

	memcpy(buffer, &len, sizeof(len));
	memcpy(buffer + sizeof(len), sub.text, len);

	int totalSent = 0;
	int amountToSend = sizeof(len) + len;
	int sendRes = -1;
	while (totalSent < amountToSend)
	{
		sendRes = send(tcpClient, buffer + totalSent, amountToSend - totalSent, 0);
		if (sendRes < 0)
			break;
		totalSent += sendRes;
	}

	delete[] buffer;

	return sendRes;
}

void tick()
{
	std::lock_guard<std::mutex> lock(tcpClientsLock);

	int idxToSend = -1;
	for (int i = 0; i < SUBTITLES_ARR_SIZE; i++)
	{
		if (subtitlesBuffer[i] != subtitles[i].text)
		{
			subtitlesBuffer[i] = subtitles[i].text;

			idxToSend = checkStringsIdx(i, (i + 1) % SUBTITLES_ARR_SIZE);
			if (0 <= idxToSend)
			{
				if (idxToSend != i)
					subtitlesBuffer[idxToSend] = subtitles[idxToSend].text;
				break;
			}
		}
	}

	if (0 <= idxToSend)
	{
		auto it = tcpClients.begin();
		while (it != tcpClients.end())
		{
			if (sendSubtitle(*it, idxToSend) < 0)
			{
				closesocket(*it);
				it = tcpClients.erase(it);
			}
			else
				++it;
		}
	}
}

void ScriptMain()
{
	std::thread t(&runTcpServer);

	auto hBaseAddr = (UINT_PTR)GetModuleHandle(NULL);
	subtitles = (s_subtitle*)(hBaseAddr + 0x4A66040L);

	for (int i = 0; i < SUBTITLES_ARR_SIZE; i++)
		subtitlesBuffer[i] = subtitles[i].text;

	while (isRunning)
	{
		tick();
		WAIT(0);
	}

	isRunning = false;

	for (int i = 0; i < SUBTITLES_ARR_SIZE; i++)
		delete[] subtitlesBuffer[i];
	delete[] subtitlesBuffer;

	t.join();
}
