#include "memoryanalyzer.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <limits.h>

// Memory Analyzer to well... analyze memory
// Static functions only.
// Get the original path of the process
QString MemoryAnalyzer::getExePath(ProcessID pid)
{
    if (pid <= 0) return QString();

    char buf[PATH_MAX];
    std::string exePath = "/proc/" + std::to_string(pid) + "/exe";

    ssize_t len = readlink(exePath.c_str(), buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return QString::fromUtf8(buf);
    }

    return QString();
}

// Get the name of the process through PID
QString MemoryAnalyzer::getProcessName(ProcessID pid)
{
    if (pid <= 0) return QString();

    QString commPath = QString("/proc/%1/comm").arg(pid);
    QFile file(commPath);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString name = in.readLine().trimmed();
        file.close();
        return name;
    }

    return QString();
}

// Analyze ONLY the PID
ProcessMemorySummary MemoryAnalyzer::analyzeSinglePid(ProcessID pid, bool usePSS)
{
    ProcessMemorySummary s;
    s.pid = pid;
    s.processName = getProcessName(pid);

    if (pid <= 0) {
        qWarning() << "Invalid PID:" << pid;
        return s;
    }

    // Parse smaps for detailed breakdown
    std::string smapsPath = "/proc/" + std::to_string(pid) + "/smaps";
    std::ifstream smaps(smapsPath);

    if (smaps.is_open()) {
        std::string line;
        std::string currentPath;
        std::string currentPerms;

        while (std::getline(smaps, line)) {
            if (line.empty()) continue;

            // Check if this is a memory region header (has address range)
            if (line.find('-') != std::string::npos) {
                // Parse the header line:
                // address range perms offset dev inode pathname
                std::istringstream iss(line);
                std::string range, perms, offset, dev, inode;
                iss >> range >> perms >> offset >> dev >> inode;

                currentPerms = perms;
                currentPath.clear();

                // Get the rest of the line as the path
                std::string remaining;
                if (std::getline(iss, remaining)) {
                    currentPath = remaining;
                    size_t start = currentPath.find_first_not_of(" \t"); // Trim leading whitespace
                    if (start != std::string::npos) {
                        currentPath = currentPath.substr(start);
                    } else {
                        currentPath.clear();
                    }
                }
            }
            // Use PSS for multiple processes to avoid double-counting, RSS for single process
            else if ((usePSS && line.find("Pss:") == 0) || (!usePSS && line.find("Rss:") == 0)) {
                std::istringstream iss(line);
                std::string label;
                long memKB;
                std::string unit;
                iss >> label >> memKB >> unit;

                if (memKB <= 0) continue;

                QString qpath = QString::fromStdString(currentPath).trimmed();

                // Categorize based on the path and permissions
                if (qpath.contains("[stack")) {
                    s.stk += memKB;
                } else if (qpath.endsWith(".so") || qpath.contains("/lib") ||
                           qpath.contains(".so.") || qpath.startsWith("/usr/lib") ||
                           qpath.startsWith("/lib")) {
                    s.img += memKB;
                } else if (qpath.isEmpty() || qpath == "[heap]" || qpath == "[anon]") {
                    s.pvt += memKB;
                } else if (!qpath.isEmpty() && !qpath.startsWith("[")) {
                    // Files that are memory mapped
                    s.map += memKB;
                } else {
                    // Other anonymous mappings
                    s.pvt += memKB;
                }
            }
        }
        smaps.close();
        s.total = s.pvt + s.stk + s.img + s.map;

        if (s.total > 0) {
            return s;
        }
    }

    // Fallback to /proc/[pid]/status if smaps failed
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status(statusPath);

    if (status.is_open()) {
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string label;
                long rssKB;
                iss >> label >> rssKB;
                s.pvt = rssKB;
                s.total = rssKB;
                break;
            }
        }
        status.close();
    }

    return s;
}

// Analyze entire application
ProcessMemorySummary MemoryAnalyzer::analyzeApplication(ProcessID rootPid, bool usePSS)
{
    if (rootPid <= 0) {
        qWarning() << "Invalid root PID:" << rootPid;
        return ProcessMemorySummary();
    }

    QList<ProcessID> pids = findRelatedPids(rootPid);

    ProcessMemorySummary total;
    total.pid = rootPid;

    QString rootName = getProcessName(rootPid);
    total.processName = rootName.isEmpty() ? QString("PID %1").arg(rootPid)
                                           : QString("%1 (Group)").arg(rootName);

    for (ProcessID p : std::as_const(pids)) {
        // Use PSS to avoid counting shared libraries multiple times across related processes
        ProcessMemorySummary s = analyzeSinglePid(p, usePSS);
        total.pvt += s.pvt;
        total.stk += s.stk;
        total.img += s.img;
        total.map += s.map;
    }

    total.total = total.pvt + total.stk + total.img + total.map;
    return total;
}

// Get all the PIDs related to the selected process
// We check the source file and then see all the processes originating from thier
QList<ProcessID> MemoryAnalyzer::findRelatedPids(ProcessID pid)
{
    if (pid <= 0) {
        return QList<ProcessID>();
    }

    QString targetExe = getExePath(pid);
    if (targetExe.isEmpty()) {
        return { pid };
    }

    QList<ProcessID> result;
    QDir procDir("/proc");

    if (!procDir.exists()) {
        qWarning() << "Cannot access /proc directory";
        return { pid };
    }

    QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &entry : std::as_const(entries)) {
        bool ok;
        int otherPid = entry.toInt(&ok);

        if (!ok || otherPid <= 0) continue;

        QString otherExe = getExePath(otherPid);
        if (!otherExe.isEmpty() && otherExe == targetExe) {
            result.append(otherPid);
        }
    }

    // Ensure we at least have the original PID
    if (result.isEmpty()) {
        result.append(pid);
    }

    return result;
}
