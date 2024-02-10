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
#include <psapi.h>
#include <windows.h>

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
		auto tcpClient = accept(serverSd, nullptr, nullptr);
		if (tcpClient < 0)
			continue;

		std::lock_guard<std::mutex> lock(tcpClientsLock);
		tcpClients.push_back(tcpClient);
	}
}

bool isStringASubtitle(int idx)
{
	const char* subtitlePrefix = "~z~";
	return strstr(subtitles[idx].text, subtitlePrefix) != nullptr;
}

int checkStringsIdx(int idx1, int idx2)
{
	if (subtitles[idx1].text == nullptr)
		return -1;

	if (!isStringASubtitle(idx1))
		return -1;

	if (subtitles[idx2].text == nullptr)
		return idx1;

	if (strstr(subtitles[idx2].text, subtitles[idx1].text) != nullptr)
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

bool areStringsEqual(const char* str1, const char* str2)
{
	if (str1 == str2)
		return true;
	if (str1 == nullptr || str2 == nullptr)
		return false;
	return strcmp(str1, str2) == 0;
}

void tick()
{
	std::lock_guard<std::mutex> lock(tcpClientsLock);

	int idxToSend = -1;
	for (int i = 0; i < SUBTITLES_ARR_SIZE; i++)
	{
		if (subtitles[i].text == nullptr)
			continue;

		if (!areStringsEqual(subtitles[i].text, subtitlesBuffer[i]))
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

// https://github.com/scripthookvdotnet/scripthookvdotnet/blob/main/source/core/NativeMemory.cs#L206
std::vector<int> CreateShiftTableForBmh(const std::vector<int>& pattern) {
	std::vector<int> skipTable(256);
	int lastIndex = pattern.size() - 1;

	auto it = std::find(pattern.rbegin(), pattern.rend(), -1);
	int diff = lastIndex - (it != pattern.rend() ? pattern.rend() - it - 1 : 0);
	if (diff == 0)
		diff = 1;

	for (int i = 0; i < skipTable.size(); i++)
		skipTable[i] = diff;

	for (int i = lastIndex - diff; i < lastIndex; i++)
	{
		short patternVal = pattern[i];
		if (patternVal >= 0)
			skipTable[patternVal] = lastIndex - i;
	}

	return skipTable;
}

void ScriptMain()
{
	std::thread t(&runTcpServer);

	std::vector<int> pattern = { 0x8B, 0x0D, -1, -1, -1, -1, 0x48, 0x8D, 0x3D, -1, -1, -1, -1, 0x48, 0x63, 0x05, -1, -1, -1, -1, 0x3B, 0xC8, 0x72 };

	// https://github.com/scripthookvdotnet/scripthookvdotnet/blob/main/source/core/NativeMemory.cs#L157
	HANDLE hProcess = GetCurrentProcess();
	HMODULE hModule = GetModuleHandle(nullptr);
	MODULEINFO moduleInfo;

	if (GetModuleInformation(hProcess, hModule, &moduleInfo, sizeof(moduleInfo)))
	{
		DWORD moduleSize = moduleInfo.SizeOfImage;
		char* startAddr = (char*)hModule;
		char* endAddr = startAddr + moduleSize;

		int lastPatternIndex = pattern.size() - 1;
		std::vector<int> skipTable = CreateShiftTableForBmh(pattern);

		auto endAddressToScan = endAddr - pattern.size();

		for (char* curHeadAddress = startAddr;
			curHeadAddress <= endAddressToScan;
			curHeadAddress += (skipTable[static_cast<unsigned char>(curHeadAddress[lastPatternIndex])] > 1)
			? skipTable[static_cast<unsigned char>(curHeadAddress[lastPatternIndex])]
			: 1)
		{

			for (int i = lastPatternIndex; pattern[i] < 0 || static_cast<unsigned char>(curHeadAddress[i]) == pattern[i]; --i)
			{
				if (i == 0)
				{
					auto offset = *(DWORD*)(curHeadAddress + 9);
					subtitles = (s_subtitle*)(curHeadAddress + offset + 13);

					goto skip_static_offset;
				}
			}
		}
	}

	subtitles = (s_subtitle*)((UINT_PTR)hModule + 0x4A66050L);

skip_static_offset:
	CloseHandle(hProcess);

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
