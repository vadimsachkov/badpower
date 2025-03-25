/*
    автор Вадим Сачков:
	Program: badpower - ARP Monitoring & Conditional Execution Tool

    Description:
    This console application monitors a specified IP address for the expected MAC address
    in the system ARP table. If the MAC is missing for a specified duration and the system
    uptime exceeds a minimum threshold, it executes a specified command (e.g., shutdown).

    The last successful ARP detection is stored in a timestamp file.
    Logs are written to a monthly log file and old logs are auto-deleted after 1 year.

    IMPORTANT:
    This program requires C++17. Be sure to set the language standard to ISO C++17
    separately for both Debug and Release configurations:

    Project Properties → C/C++ → Language → C++ Language Standard → ISO C++17 (/std:c++17)

    badpower.exe -ip 190.190.32.11 -mac 90-4E-2B-CA-0A-53 -max_min 120 -uptime_min 20  -pathlog "C:\\temp" -exec "shutdown /s /t 180 /c \"badpower script has initiated automatic shutdown because the Huawei router at 190.190.32.11 has not responded for a long time, possibly due to a power outage.\""

    события в журнале логов системы:
    ID 1074 — обычное завершение работы   (например, через shutdown.exe) User32 . В описании есть текст который указан в параметре  /c команды shutdown
    ID 1075 - отмена выключения командой shutdown -a 

*/