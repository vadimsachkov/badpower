/*
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

    badpower.exe -ip 190.190.32.11 -mac 90-4E-2B-CA-0A-53 -wait_min 120 -uptime_min 20  -prefix huaweiWS322 -clear -pathlog "C:\\temp" -exec "shutdown /s /t 180 /c \"badpower script has initiated automatic shutdown because the Huawei router at 190.190.32.11 has not responded for a long time, possibly due to a power outage.\""
    события в журнале логов системы:
    ID 1074 — обычное завершение работы   (например, через shutdown.exe) User32 . В описании есть текст который указан в параметре  /c команды shutdown
    ID 1075 - отмена выключения командой shutdown -a
*/

// Version: 2.11

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <windows.h>
#include <regex>
#include <sstream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <set>
#include <filesystem> // Requires C++17

namespace fs = std::filesystem;
using namespace std;

bool isValidIP(const string& ip) {
    regex ip_pattern("^(25[0-5]|2[0-4][0-9]|[0-1]?[0-9]?[0-9])\\."
        "(25[0-5]|2[0-4][0-9]|[0-1]?[0-9]?[0-9])\\."
        "(25[0-5]|2[0-4][0-9]|[0-1]?[0-9]?[0-9])\\."
        "(25[0-5]|2[0-4][0-9]|[0-1]?[0-9]?[0-9])$");
    return regex_match(ip, ip_pattern);
}

bool isValidMAC(const string& mac) {
    regex mac_pattern("^([0-9A-Fa-f]{2}-){5}([0-9A-Fa-f]{2})$");
    return regex_match(mac, mac_pattern);
}

template <typename T>
bool parseInt(const string& s, T& value) {
    try {
        size_t idx;
        value = stoi(s, &idx);
        return idx == s.length() && value > 0;
    }
    catch (...) {
        return false;
    }
}

string toUpper(const string& s) {
    string out = s;
    transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
}

string sanitizeFilename(const string& input) {
    string out = input;
    out.erase(remove_if(out.begin(), out.end(), [](char c) {
        return c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|';
        }), out.end());
    return out;
}

bool fileExists(const string& filename) {
    return fs::exists(filename);
}

string getCurrentTimestamp() {
    time_t now = time(0);
    tm local_tm;
    localtime_s(&local_tm, &now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return string(buffer);
}

string getCurrentLogFileName(const string& prefixOrIp) {
    time_t now = time(0);
    tm local_tm;
    localtime_s(&local_tm, &now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), ("badpower_" + prefixOrIp + "_%Y%m.log").c_str(), &local_tm);
    return string(buffer);
}

string g_logFilePath;

void logAndPrint(const string& message) {
    string fullMessage = getCurrentTimestamp() + " - " + message;
    cout << fullMessage << endl;
    ofstream log(g_logFilePath, ios::app);
    log << fullMessage << endl;
}

string runCommand(const string& cmd) {
    string result;
    char buffer[128];
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    _pclose(pipe);
    return result;
}

time_t parseTimestamp(const string& timestamp) {
    tm t = {};
    istringstream ss(timestamp);
    ss >> get_time(&t, "%Y-%m-%d %H:%M:%S");
    return mktime(&t);
}

void deleteOldLogs(const string& directory, int maxAgeDays = 365) {
    auto now = chrono::system_clock::now();
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry) && entry.path().filename().string().rfind("badpower_", 0) == 0) {
            auto ftime = fs::last_write_time(entry);
            auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(ftime - decltype(ftime)::clock::now() + chrono::system_clock::now());
            auto age = chrono::duration_cast<chrono::hours>(now - sctp).count() / 24;
            if (age > maxAgeDays) {
                logAndPrint("Old log file deleted: " + entry.path().string());
                fs::remove(entry);
            }
        }
    }
}

void showHelp() {
    cout << "This program badpower.exe v2.11 monitors the availability of a target host using ARP.\n";
    cout << "If the host becomes unreachable for a specified time, a user-defined action (e.g., shutdown) is executed.\n";
    cout << "The program creates two auxiliary files: a log file and a timestamp file in the directory specified by -pathlog.\n";
    cout << "Log files are created monthly and are automatically deleted after one year.\n\n";
    cout << "Usage: badpower [parameters]\n\n";
    cout << "Required Parameters:\n";
    cout << "  -ip <ip_address>          Target device IP address (e.g. 192.168.1.100)\n";
    cout << "  -mac <mac_address>        Expected MAC address of the target device (e.g. AA-BB-CC-DD-EE-FF)\n";
    cout << "  -wait_min <minutes>       Maximum allowed time without ARP success (in minutes)\n";
    cout << "  -uptime_min <minutes>     Required system uptime before action (in minutes)\n";
    cout << "  -exec \"<command>\"         Command to execute when conditions are met\n";
    cout << "  -pathlog <path>           Directory to store logs and timestamp files\n\n";
    cout << "Optional Parameters:\n";
    cout << "  -prefix <name>            Prefix used instead of IP in filenames under -pathlog (log and timestamp files)\n";
    cout << "  -clear                    Clear timestamp file before executing\n";
    cout << "  -?                        Show this help text\n";
}

int main(int argc, char* argv[]) {
	
	
	set<string> allowedParams = {
		"-ip", "-mac", "-wait_min", "-uptime_min",
		"-exec", "-pathlog", "-prefix", "-clear", "-?", "/?", "?"
	};
 

	if (argc == 1) {
        showHelp();
        return 0;
    }

    map<string, string> args;
    vector<string> flags(argv + 1, argv + argc);


    for (size_t i = 0; i < flags.size(); i++) {
        if (flags[i].rfind("-", 0) == 0) {
            if (i + 1 < flags.size() && flags[i + 1].rfind("-", 0) != 0)
                args[flags[i]] = flags[i + 1], i++;
            else
                args[flags[i]] = "";
        }
    }
    if (args.count("-?") ) {
        showHelp();
        return 0;
    }

    if (!args.count("-ip") || !isValidIP(args["-ip"])) {
        cerr << "Invalid or missing -ip parameter." << endl;
        return 1;
    }
    if (!args.count("-mac") || !isValidMAC(args["-mac"])) {
        cerr << "Invalid or missing -mac parameter." << endl;
        return 1;
    }
    if (!args.count("-wait_min")) {
        cerr << "Missing -wait_min parameter." << endl;
        return 1;
    }
    if (!args.count("-uptime_min")) {
        cerr << "Missing -uptime_min parameter." << endl;
        return 1;
    }
    if (!args.count("-exec")) {
        cerr << "Missing -exec command." << endl;
        return 1;
    }
    if (!args.count("-pathlog")) {
        cerr << "Missing -pathlog parameter." << endl;
        return 1;
    }

    int wait_min = stoi(args["-wait_min"]);
    int uptime_min = stoi(args["-uptime_min"]);

    string ip = args["-ip"];
    string mac = toUpper(args["-mac"]);
    string exec_cmd = args["-exec"];
    string log_path = args["-pathlog"];
    string prefix = args.count("-prefix") ? sanitizeFilename(args["-prefix"]) : regex_replace(ip, regex("\\."), "_");
    string timestampFile = (fs::path(log_path) / ("badpower_" + prefix + ".txt")).string();
    g_logFilePath = (fs::path(log_path) / getCurrentLogFileName(prefix)).string();
    bool clearFlag = args.count("-clear") > 0;

    fs::create_directories(log_path);
    deleteOldLogs(log_path);

    logAndPrint(string(60, '-'));
    
    // Check for any unknown parameters and log a warning.
    for (const auto& [key, _] : args) {
        if (!allowedParams.count(key)) {
            logAndPrint("Warning: Unknown parameter ignored: " + key);
        }
    }


    logAndPrint("Starting check for IP: " + ip + ", MAC: " + mac);

    runCommand("arp -d " + ip);
    runCommand("ping -n 1 " + ip);
    string arpOutput = toUpper(runCommand("arp -a " + ip));

    if (arpOutput.find(mac) != string::npos) {
        ofstream out(timestampFile);
        out << getCurrentTimestamp();
        out.close();
        logAndPrint("MAC address found. Timestamp updated in file: " + timestampFile);
        return 0;
    }

    if (!fileExists(timestampFile)) {
        logAndPrint("MAC not found. No timestamp file: " + timestampFile + ". Exiting.");
        return 0;
    }

    ifstream in(timestampFile);
    string lastTimestamp;
    getline(in, lastTimestamp);
    in.close();

    time_t last = parseTimestamp(lastTimestamp);
    time_t now = time(0);
    double diff_minutes = difftime(now, last) / 60.0;
    ULONGLONG uptime_min_now = GetTickCount64() / (60 * 1000);

    logAndPrint("MAC not found. Time since last success: " + to_string(diff_minutes) + " min, Uptime: " + to_string(uptime_min_now) + " min.");

    if (diff_minutes >= wait_min && uptime_min_now >= uptime_min) {
        logAndPrint("Conditions met. Executing command: " + exec_cmd);
        if (clearFlag && fileExists(timestampFile)) {
            fs::remove(timestampFile);
            logAndPrint("Timestamp file deleted due to -clear: " + timestampFile);
        }
        system(exec_cmd.c_str());
    }
    else {
        logAndPrint("Conditions NOT met. No action taken.");
    }

    return 0;
}
