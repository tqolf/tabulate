#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <unordered_map>
#include <chrono>
#include <iostream>

class IOStatistics {
  public:
    static std::map<std::string, size_t> Parse(pid_t pid);
};

class NETStatistics {
  public:
    struct netstat {
        char device[64];
        uint64_t rxBytes;
        uint64_t rxPackets;
        uint64_t rxErrors;
        uint64_t rxDrop;
        uint64_t rxFifo;
        uint64_t rxFrame;
        uint64_t rxCompressed;
        uint64_t rxMulticast;
        uint64_t txBytes;
        uint64_t txPackets;
        uint64_t txErrors;
        uint64_t txDrop;
        uint64_t txFifo;
        uint64_t txCollisions;
        uint64_t txCarrier;
        uint64_t txCompressed;

        friend std::ostream &operator<<(std::ostream &os, const netstat &stats);
    };

    static netstat Parse(const std::string &file = "/proc/net/dev");
};

class CPUStatistics {
  public:
    struct cpustat {
        std::string name;
        uint64_t user;
        uint64_t nice;
        uint64_t system;
        uint64_t idle;
        uint64_t iowait;
        uint64_t irq;
        uint64_t softirq;
        uint64_t steal;
        uint64_t guest;
        uint64_t guest_nice;
    };

    CPUStatistics(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));

    std::string CPUName(const std::string &file = "/proc/cpuinfo");

    std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> CPUTimes()
    {
        auto CPUStatistics_ = this->Parse(this->procFile);
        return std::make_tuple(CPUStatistics_.at("cpu").at("user"), CPUStatistics_.at("cpu").at("nice"),
                               CPUStatistics_.at("cpu").at("system"), CPUStatistics_.at("cpu").at("idle"));
    }

    std::map<std::string, std::unordered_map<std::string, uint64_t>> Parse(const std::string &file = "/proc/stat");

    double CurrentUsage(int processid = -1);
    std::vector<double> CurrentMultiCoreUsage();

    static int GetProcessID(pid_t tid = gettid(), pid_t pid = getpid());

  private:
    void CalculateUsage();

    std::string cpuName;
    std::string procFile;
    std::chrono::milliseconds updateTime;
    std::chrono::system_clock::time_point timestamp_of_measurement;
    std::map<std::string, double> cpuUsage;
    std::map<std::string, std::unordered_map<std::string, uint64_t>> CPUStatisticsMap;
    std::map<std::string, std::unordered_map<std::string, uint64_t>> OldCPUStatisticsMap;
};
