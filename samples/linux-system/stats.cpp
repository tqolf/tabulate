#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "stats.h"

std::map<std::string, size_t> IOStatistics::Parse(pid_t pid)
{
    std::map<std::string, size_t> iostatistics;
    std::ifstream rf("/proc/" + std::to_string(pid) + "/io");
    for (std::string line; std::getline(rf, line);) {
        std::string tag;
        size_t size;
        std::stringstream ss(line);
        if (ss >> tag >> size) {
            iostatistics[tag.substr(0, tag.size() - sizeof(":") + 1)] = size;
        }
    }

    return iostatistics;
}

NETStatistics::netstat NETStatistics::Parse(const std::string &file)
{
    NETStatistics::netstat stats;
    std::ifstream rf(file);
    if (!rf.good()) {
        return stats;
    }
    std::string text((std::istreambuf_iterator<char>(rf)), std::istreambuf_iterator<char>());

    int n = 0;
    char *line, *nextline, *token, *nexttoken;
    for (char *lines = (char *)text.data();; lines = nullptr) {
        line = strtok_r(lines, "\n", &nextline);
        if (line == nullptr) {
            break;
        }
        if (++n <= 2) {
            continue; // drop the first two lines
        }

        int j = 0;
        uint64_t *current = &stats.rxBytes;
        for (char *tokens = line;; tokens = nullptr, j++) {
            token = strtok_r(tokens, ": ", &nexttoken);
            if (token == nullptr) {
                break;
            }

            if (j == 0) {
                strncpy(stats.device, token, sizeof(stats.device) - 1);
            } else {
                *current++ = (uint64_t)strtoull(token, nullptr, 10);
            }
        }
    }

    return stats;
}

std::ostream &operator<<(std::ostream &os, const NETStatistics::netstat &stats)
{
    os << "Device: " << stats.device << std::endl;
    os << "  rxBytes:      " << stats.rxBytes << std::endl;
    os << "  rxPackets:    " << stats.rxPackets << std::endl;
    os << "  rxErrors:     " << stats.rxErrors << std::endl;
    os << "  rxDrop:       " << stats.rxDrop << std::endl;
    os << "  rxFifo:       " << stats.rxFifo << std::endl;
    os << "  rxFrame:      " << stats.rxFrame << std::endl;
    os << "  rxCompressed: " << stats.rxCompressed << std::endl;
    os << "  rxMulticast:  " << stats.rxMulticast << std::endl;
    os << "  txBytes:      " << stats.txBytes << std::endl;
    os << "  txPackets:    " << stats.txPackets << std::endl;
    os << "  txErrors:     " << stats.txErrors << std::endl;
    os << "  txDrop:       " << stats.txDrop << std::endl;
    os << "  txFifo:       " << stats.txFifo << std::endl;
    os << "  txCollisions: " << stats.txCollisions << std::endl;
    os << "  txCarrier:    " << stats.txCarrier << std::endl;
    os << "  txCompressed: " << stats.txCompressed << std::endl;

    return os;
}


const std::vector<std::string> cpuIdentifiers{"user", "nice",    "system", "idle",  "iowait",
                                              "irq",  "softirq", "steal",  "guest", "guest_nice"};

CPUStatistics::CPUStatistics(std::chrono::milliseconds interval)
    : procFile("/proc/stat"), cpuName(""), updateTime(interval)
{
    this->OldCPUStatisticsMap = this->Parse(this->procFile);
    std::this_thread::sleep_for(this->updateTime);
    this->CPUStatisticsMap = this->Parse(this->procFile);
    this->CalculateUsage();
    this->timestamp_of_measurement = std::chrono::system_clock::now() - this->updateTime;
}

double CPUStatistics::CurrentUsage(int processid)
{
    if ((std::chrono::system_clock::now() - this->timestamp_of_measurement) >= this->updateTime) {
        this->OldCPUStatisticsMap = this->CPUStatisticsMap;
        this->timestamp_of_measurement = std::chrono::system_clock::now();
        this->CPUStatisticsMap = this->Parse(this->procFile);
        this->CalculateUsage();
    }
    if (processid < 0 || !this->cpuUsage.count("cpu" + std::to_string(processid))) {
        return this->cpuUsage.at("cpu");
    } else {
        return this->cpuUsage.at("cpu" + std::to_string(processid));
    }
}

std::vector<double> CPUStatistics::CurrentMultiCoreUsage()
{
    CurrentUsage();
    std::vector<double> percents;
    for (const auto &elem : this->cpuUsage) {
        if (elem.first == "cpu") {
            continue;
        }
        percents.push_back(elem.second);
    }

    return percents;
}

void CPUStatistics::CalculateUsage()
{
    for (const auto &elem : this->CPUStatisticsMap) {
        if (this->CPUStatisticsMap.at(elem.first).at("user") > this->OldCPUStatisticsMap.at(elem.first).at("user")
            || this->CPUStatisticsMap.at(elem.first).at("nice") > this->OldCPUStatisticsMap.at(elem.first).at("nice")
            || this->CPUStatisticsMap.at(elem.first).at("system")
                   > this->OldCPUStatisticsMap.at(elem.first).at("system")
            || this->CPUStatisticsMap.at(elem.first).at("idle") > this->OldCPUStatisticsMap.at(elem.first).at("idle")) {
            auto total =
                (this->CPUStatisticsMap.at(elem.first).at("user") - this->OldCPUStatisticsMap.at(elem.first).at("user"))
                + (this->CPUStatisticsMap.at(elem.first).at("nice")
                   - this->OldCPUStatisticsMap.at(elem.first).at("nice"))
                + (this->CPUStatisticsMap.at(elem.first).at("system")
                   - this->OldCPUStatisticsMap.at(elem.first).at("system"));

            double percent = total;
            total += (this->CPUStatisticsMap.at(elem.first).at("idle")
                      - this->OldCPUStatisticsMap.at(elem.first).at("idle"));
            percent /= total;
            percent *= 100.0;
            this->cpuUsage[elem.first] = percent;
        }
    }
}

std::map<std::string, std::unordered_map<std::string, uint64_t>> CPUStatistics::Parse(const std::string &fileName)
{
    std::map<std::string, std::unordered_map<std::string, uint64_t>> CPUStatistics_;

    try {
        std::ifstream cpuFile(fileName);

        uint32_t lineCnt = 0;
        bool infoValid = true;
        for (std::string line; std::getline(cpuFile, line) && infoValid; lineCnt++) {
            std::stringstream strStream(line);
            std::string strPart;
            std::string cpuNum;
            auto it = cpuIdentifiers.begin();
            std::unordered_map<std::string, uint64_t> cpuValues;
            while (strStream >> strPart) {
                if (cpuNum.empty()) {
                    if (strPart.find("cpu") != std::string::npos) {
                        cpuNum = strPart;
                        continue;
                    } else {
                        infoValid = false;
                        break;
                    }
                }
                if (it != cpuIdentifiers.end()) {
                    cpuValues[it->data()] = std::stoull(strPart);
                }
                if (it->data() == cpuIdentifiers.at(4)) {
                    break;
                }
                it++;
            }
            if (!cpuNum.empty()) {
                CPUStatistics_[cpuNum] = cpuValues;
            }
        }
    } catch (std::ifstream::failure &e) {
        throw std::runtime_error("Exception: " + fileName + std::string(e.what()));
    }
    return CPUStatistics_;
}

std::string CPUStatistics::CPUName(const std::string &cpuNameFile)
{
    if (!this->cpuName.empty()) {
        return this->cpuName;
    }

    std::ifstream file;
    file.open(cpuNameFile);

    if (!file.is_open()) {
        throw std::runtime_error("unable to open " + cpuNameFile);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                this->cpuName = line.substr(pos, line.size());
                return this->cpuName;
            }
        }
    }
    return std::string();
}

int CPUStatistics::GetProcessID(pid_t tid, pid_t pid)
{
    int fd, processid = -1;
    char filename[256];
    sprintf(filename, "/proc/%d/task/%d/stat", pid, tid);
    if ((fd = open(filename, O_RDONLY)) >= 0) {
        int size;
        char proc_stat[512] = {0};
        if ((size = read(fd, proc_stat, sizeof(proc_stat))) > 0) {
            const char *spcifiers =
                "%*d %*s %*c %*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %*ld"
                "%*ld %*llu %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*d %d";
            sscanf(proc_stat, spcifiers,
                   &processid // (39) processor  %d  (since Linux 2.2.8)
                              //         CPU number last executed on.
            );
        }
        close(fd);
    }

    return processid;
}

#if 0
void PrintMap(const std::map<std::string, size_t> &map)
{
    for (auto [key, value] : map) {
        std::cout << key << ": " << value << std::endl;
    }
}

void PrintMap(const std::map<std::string, std::unordered_map<std::string, uint64_t>> &map)
{
    for (auto [key, value] : map) {
        std::cout << key << ":" << std::endl;
        for (auto [tag, val] : value) {
            std::cout << "  " << tag << ": " << val << std::endl;
        }
    }
}

int main(int argc, char **argv)
{
    std::cout << NETStatistics::Parse() << std::endl;

    CPUStatistics monitor;
    int processid = CPUStatistics::GetProcessID();
    PrintMap(IOStatistics::Parse(getpid()));
    // std::cout << "processid=" << processid << ", pid=" << getpid() << ", tid=" << gettid() << std::endl;
    for (int i = 0; i < 5; i++) {
        std::cout << monitor.CurrentUsage(processid) << std::endl;
        // std::cout << "  " << JoinList(monitor.CurrentMultiCoreUsage(), " ") << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        PrintMap(IOStatistics::Parse(getpid()));
    }

    PrintMap(CPUStatistics().Parse());

    return 0;
}
#endif
