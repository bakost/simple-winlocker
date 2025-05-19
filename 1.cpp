// 1.cpp
// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#define IDC_CONNECT_BTN     1001
#define IDC_DISCONNECT_BTN  1002
#define IDC_RESULT_STATIC   1003
#define IDC_DLL_EDIT        1004

// Имя класса и заголовок главного окна
static TCHAR szWindowClass[] = _T("DesktopApp");
static TCHAR szTitle[] = _T("DLL Hook");

HINSTANCE hInst = NULL;
HINSTANCE hDll = NULL;

// Прототип оконной процедуры
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    WNDCLASSEX wcex = {0};

    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(wcex.hInstance, IDI_APPLICATION);
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Error"),
            MB_OK);
        return 1;
    }

    hInst = hInstance;

    HWND hWnd = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        700, 500,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Error"),
            MB_OK);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Основной цикл обработки сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

// Оконная процедура главного окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hEditDll; // Поле для ввода имени DLL

    switch (message)
    {
    case WM_CREATE:
        {
            // Статусный текста для индикации состояния (On/Off)
            CreateWindow(_T("STATIC"), _T("Hook Status:"),
                         WS_CHILD | WS_VISIBLE,
                         500, 20, 90, 25, hWnd, NULL, hInst, NULL);
            CreateWindow(_T("STATIC"), _T("Off"),
                         WS_CHILD | WS_VISIBLE | SS_CENTER,
                         590, 20, 50, 25, hWnd, (HMENU)IDC_RESULT_STATIC, hInst, NULL);
            
            // Кнопки для подключения и отключения DLL
            CreateWindow(_T("BUTTON"), _T("Install Hook"),
                         WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                         50, 50, 120, 30, hWnd, (HMENU)IDC_CONNECT_BTN, hInst, NULL);
                         
            CreateWindow(_T("BUTTON"), _T("Uninstall Hook"),
                         WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         50, 100, 120, 30, hWnd, (HMENU)IDC_DISCONNECT_BTN, hInst, NULL);
                         
            // При старте кнопка "Uninstall" недоступна
            EnableWindow(GetDlgItem(hWnd, IDC_DISCONNECT_BTN), FALSE);
            
            // Поле для ввода имени DLL
            CreateWindow(_T("STATIC"), _T("DLL:"),
                         WS_CHILD | WS_VISIBLE,
                         50, 150, 50, 25, hWnd, NULL, hInst, NULL);
                         
            hEditDll = CreateWindow(_T("EDIT"), _T("WinLockerDLL.dll"),
                         WS_CHILD | WS_VISIBLE | WS_BORDER,
                         110, 150, 200, 25, hWnd, (HMENU)IDC_DLL_EDIT, hInst, NULL);
        }
        break;
    
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_CONNECT_BTN)
            {
                TCHAR dllName[256] = {0};
                GetWindowText(GetDlgItem(hWnd, IDC_DLL_EDIT), dllName, 256);

                hDll = LoadLibrary(dllName);
                if (!hDll)
                {
                    DWORD err = GetLastError();
                    TCHAR errMsg[256];
                    _stprintf_s(errMsg, 256, _T("Error loading DLL (code: %d)"), err);
                    MessageBox(hWnd, errMsg, _T("Error"), MB_OK);
                    break;
                }
                // В новой версии DLL вся инициализация (установка хуков, создание экрана блокировки)
                // происходит в DllMain при загрузке, поэтому дополнительных вызовов не требуется.
                SetWindowText(GetDlgItem(hWnd, IDC_RESULT_STATIC), _T("Locked"));
                EnableWindow(GetDlgItem(hWnd, IDC_CONNECT_BTN), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDC_DISCONNECT_BTN), TRUE);
            }
            else if (wmId == IDC_DISCONNECT_BTN)
            {
                if (hDll)
                {
                    // Освобождение DLL вызовет DLL_PROCESS_DETACH, где производится снятие перехвата.
                    FreeLibrary(hDll);
                    hDll = NULL;
                    SetWindowText(GetDlgItem(hWnd, IDC_RESULT_STATIC), _T("Off"));
                    EnableWindow(GetDlgItem(hWnd, IDC_CONNECT_BTN), TRUE);
                    EnableWindow(GetDlgItem(hWnd, IDC_DISCONNECT_BTN), FALSE);
                }
            }
        }
        break;
    
    case WM_DESTROY:
        if (hDll)
        {
            FreeLibrary(hDll);
            hDll = NULL;
        }
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
