/*
	Автор: Вадим Сачков
	Программа: badpower — инструмент для мониторинга ARP и условного выполнения команд
	создал и буду применять чтобы автоматически выключать питание, если пропало электричество и через пару часов, пока не сдохли ИБП успеть выключить

	Описание:
	Эта консольная программа отслеживает заданный IP-адрес на наличие ожидаемого MAC-адреса в ARP-таблице системы.
	Если MAC-адрес отсутствует в течение заданного времени, и время работы системы превышает минимальный порог, выполняется указанная команда (например, выключение системы).

	Время последнего успешного обнаружения ARP сохраняется в отдельном файле с временной меткой.
	Логи записываются в ежемесячный лог-файл, а старые логи автоматически удаляются через 1 год.:

    Project Properties → C/C++ → Language → C++ Language Standard → ISO C++17 (/std:c++17)
	
	пример запуска:
    badpower.exe -ip 190.190.32.11 -mac 90-4E-2B-CA-0A-53 -wait_min 120 -uptime_min 20  -pathlog "C:\\temp" -exec "shutdown /s /t 180 /c \"badpower script has initiated automatic shutdown because the Huawei router at 190.190.32.11 has not responded for a long time, possibly due to a power outage.\""

    события в журнале логов системы:
    ID 1074 — обычное завершение работы   (например, через shutdown.exe) User32 . В описании есть текст который указан в параметре  /c команды shutdown
    ID 1075 - отмена выключения командой shutdown -a 


Usage: badpower [options]
  Required Parameters:
	-ip <ip_address>          Target device IP address (e.g. 192.168.1.100)
	-mac <mac_address>        Expected MAC address (e.g. AA-BB-CC-DD-EE-FF)
	-wait_min <minutes>        Max allowed minutes without ARP success
	-uptime_min <minutes>     Min system uptime in minutes
	-exec \"<command>\"         Command to execute when conditions are met
	-pathlog <path>           Directory to store logs and badpower_last_success.txt
  Optional:
	-? /? ?                   Show this help text;



*/