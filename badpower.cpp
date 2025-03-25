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

    badpower.exe -ip 190.190.32.11 -mac 90-4E-2B-CA-0A-53 -wait_min 120 -uptime_min 20  -pathlog "C:\\temp" -exec "shutdown /s /t 180 /c \"badpower script has initiated automatic shutdown because the Huawei router at 190.190.32.11 has not responded for a long time, possibly due to a power outage.\""

    события в журнале логов системы:
    ID 1074 — обычное завершение работы   (например, через shutdown.exe) User32 . В описании есть текст который указан в параметре  /c команды shutdown
    ID 1075 - отмена выключения командой shutdown -a 

*/


// Version: 2.3

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

string getCurrentLogFileName() {
    time_t now = time(0);
    tm local_tm;
    localtime_s(&local_tm, &now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "badpower_%Y%m.log", &local_tm);
    return string(buffer);
}

void logAndPrint(const string& logPath, const string& message) {
    string fullMessage = getCurrentTimestamp() + " - " + message;
    cout << fullMessage << endl;
    string logFile = (fs::path(logPath) / getCurrentLogFileName()).string();
    ofstream log(logFile, ios::app);
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
                fs::remove(entry);
            }
        }
    }
}

void showHelp() {
    cout << "Usage: badpower [options]\n"
        << "\nRequired Parameters:\n"
        << "  -ip <ip_address>          Target device IP address (e.g. 192.168.1.100)\n"
        << "  -mac <mac_address>        Expected MAC address (e.g. AA-BB-CC-DD-EE-FF)\n"
        << "  -wait_min <minutes>       Max allowed minutes without ARP success\n"
        << "  -uptime_min <minutes>     Min system uptime in minutes\n"
        << "  -exec \"<command>\"       Command to execute when conditions are met\n"
        << "  -pathlog <path>           Directory to store logs and badpower_last_success.txt\n"
        << "\nOptional:\n"
        << "  -? /? ?                   Show this help text\n";
}

int main(int argc, char* argv[]) {
    map<string, string> args;
    vector<string> flags(argv + 1, argv + argc);

    if (flags.empty() || find(flags.begin(), flags.end(), "-?") != flags.end() ||
        find(flags.begin(), flags.end(), "/?") != flags.end() ||
        find(flags.begin(), flags.end(), "?") != flags.end()) {
        showHelp();
        return 0;
    }

    for (size_t i = 0; i < flags.size(); i++) {
        if (flags[i].rfind("-", 0) == 0 && i + 1 < flags.size()) {
            args[flags[i]] = flags[i + 1];
            i++;
        }
    }

    if (!args.count("-ip") || !isValidIP(args["-ip"])) {
        cerr << "Invalid or missing -ip parameter." << endl;
        return 1;
    }
    if (!args.count("-mac") || !isValidMAC(args["-mac"])) {
        cerr << "Invalid or missing -mac parameter." << endl;
        return 1;
    }

    int wait_min = 0, uptime_min = 0;
    if (!args.count("-wait_min") || !parseInt(args["-wait_min"], wait_min)) {
        cerr << "Invalid or missing -wait_min parameter." << endl;
        return 1;
    }
    if (!args.count("-uptime_min") || !parseInt(args["-uptime_min"], uptime_min)) {
        cerr << "Invalid or missing -uptime_min parameter." << endl;
        return 1;
    }
    if (!args.count("-exec") || args["-exec"].empty()) {
        cerr << "Missing -exec command." << endl;
        return 1;
    }
    if (!args.count("-pathlog") || args["-pathlog"].empty()) {
        cerr << "Missing -pathlog parameter." << endl;
        return 1;
    }

    string ip = args["-ip"];
    string mac = toUpper(args["-mac"]);
    string exec_cmd = args["-exec"];
    string log_path = args["-pathlog"];
    string timestampFile = (fs::path(log_path) / "badpower_last_success.txt").string();

    fs::create_directories(log_path);
    deleteOldLogs(log_path);

    // Separator line for each run
    logAndPrint(log_path, string(60, '-'));

    logAndPrint(log_path, "Starting check for IP: " + ip + ", MAC: " + mac);

    runCommand("arp -d " + ip);
    runCommand("ping -n 1 " + ip);
    string arpOutput = toUpper(runCommand("arp -a " + ip));

    if (arpOutput.find(mac) != string::npos) {
        ofstream out(timestampFile);
        out << getCurrentTimestamp();
        out.close();
        logAndPrint(log_path, "MAC address found. Timestamp updated.");
        return 0;
    }

    if (!fileExists(timestampFile)) {
        logAndPrint(log_path, "MAC not found. No timestamp file. Exiting.");
        return 0;
    }

    ifstream in(timestampFile);
    string lastTimestamp;
    getline(in, lastTimestamp);
    in.close();

    logAndPrint(log_path, "Last success timestamp read: " + lastTimestamp);

    time_t last = parseTimestamp(lastTimestamp);
    time_t now = time(0);
    double diff_minutes = difftime(now, last) / 60.0;
    ULONGLONG uptime_ms = GetTickCount64();
    ULONGLONG uptime_min_now = uptime_ms / (60 * 1000);

    stringstream ss;
    ss << "MAC not found. Time since last success: " << diff_minutes << " min, Uptime: " << uptime_min_now << " min.";
    logAndPrint(log_path, ss.str());

    stringstream c1;
    c1 << "Time since last success (" << static_cast<int>(diff_minutes) << ") ";
    c1 << (diff_minutes >= wait_min ? ">= " : "< ") << "wait_min (" << wait_min << ")";
    logAndPrint(log_path, c1.str());

    stringstream c2;
    c2 << "System uptime (" << uptime_min_now << ") ";
    c2 << (uptime_min_now >= uptime_min ? ">= " : "< ") << "uptime_min (" << uptime_min << ")";
    logAndPrint(log_path, c2.str());

    if (diff_minutes >= wait_min && uptime_min_now >= uptime_min) {
        logAndPrint(log_path, "Conditions met. Executing command: " + exec_cmd);
        system(exec_cmd.c_str());
    }
    else {
        logAndPrint(log_path, "Conditions NOT met. No action taken.");
    }

    return 0;
}
