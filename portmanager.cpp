#include "portmanager.h"
#include <QDebug>
#include <QtConcurrent>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

PortManager::PortManager(QObject *parent) : QObject(parent)
{
}

// Get all the ports
QList<PortInfo> PortManager::getOpenPorts() {
    QList<PortInfo> result;

    // Build inode to PID mapping
    QMap<unsigned long, ProcessID> inodeMap = getInodeToPidMap();

    // Files to scan for networking info
    QStringList files = {"/proc/net/tcp", "/proc/net/udp", "/proc/net/tcp6", "/proc/net/udp6"};

    for (const QString& filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Cannot open" << filePath;
            continue;
        }

        QTextStream stream(&file);
        QString headerLine = stream.readLine(); // Skip header line

        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.trimmed().isEmpty()) continue;

            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            // Expected format:
            // sl local_address rem_address st tx_queue:rx_queue tr:tm->when retrnsmt uid timeout inode
            if (parts.size() < 10) continue;

            QString localAddr = parts.value(1);
            QString state = parts.value(3);
            QString inodeStr = parts.value(9);

            bool ok;
            unsigned long inode = inodeStr.toULong(&ok);
            if (!ok || inode == 0) continue;

            // Parse hex port from local address (format: ADDRESS:PORT in hex)
            int colonPos = localAddr.indexOf(':');
            if (colonPos == -1) continue;

            QString portHex = localAddr.mid(colonPos + 1);
            int port = portHex.toInt(&ok, 16);
            if (!ok) continue;

            PortInfo info;
            info.port = port;
            info.protocol = filePath.contains("tcp") ? "TCP" : "UDP";
            info.pid = inodeMap.value(inode, 0);
            info.processName = (info.pid > 0) ? getProcessNameByPID(info.pid) : "Unknown";
            info.state = state;
            info.localAddress = localAddr;

            result.append(info);
        }

        file.close();
    }

    emit portScanCompleted(result.size());
    return result;
}

// Get PID to inode (metadata) mapping
QMap<unsigned long, ProcessID> PortManager::getInodeToPidMap() {
    QMap<unsigned long, ProcessID> map;
    QDir procDir("/proc");

    if (!procDir.exists()) {
        qWarning() << "Cannot access /proc directory";
        return map;
    }

    QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& pidStr : std::as_const(pids)) {
        bool ok;
        ProcessID pid = pidStr.toLong(&ok);
        if (!ok || pid <= 0) continue;

        QString fdPath = QString("/proc/%1/fd").arg(pidStr);
        QDir fdDir(fdPath);

        if (!fdDir.exists()) continue;

        QStringList fds = fdDir.entryList(QDir::Files | QDir::System);

        for (const QString& fd : std::as_const(fds)) {
            QString fdFilePath = fdDir.absoluteFilePath(fd);

            char linkPath[256];
            ssize_t len = readlink(fdFilePath.toLocal8Bit().constData(),
                                   linkPath, sizeof(linkPath) - 1);

            if (len > 0) {
                linkPath[len] = '\0';
                QString path = QString::fromLocal8Bit(linkPath);

                // Check if it's "socket:[inode]"
                if (path.startsWith("socket:[")) {
                    // Extract inode number
                    int start = 8; // Length of "socket:["
                    int end = path.indexOf(']', start);
                    if (end > start) {
                        QString inodeStr = path.mid(start, end - start);
                        bool inodeOk;
                        unsigned long inode = inodeStr.toULong(&inodeOk);
                        if (inodeOk && inode > 0) {
                            map.insert(inode, pid);
                        }
                    }
                }
            }
        }
    }

    return map;
}

// Get the process name by its PID
QString PortManager::getProcessNameByPID(ProcessID pid) {
    if (pid <= 0) {
        return "Unknown";
    }

    QString commPath = QString("/proc/%1/comm").arg(pid);
    QFile commFile(commPath);

    if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&commFile);
        QString name = stream.readLine().trimmed();
        commFile.close();

        if (!name.isEmpty()) {
            return name;
        }
    }

    return QString("PID_%1").arg(pid);
}

// Kill process through its PID
bool PortManager::killProcess(ProcessID pid) {
    if (pid <= 0) {
        emit errorOccurred("Invalid PID");
        return false;
    }

    // First try graceful termination with SIGTERM
    if (kill(pid, SIGTERM) == 0) {
        qDebug() << "Sent SIGTERM to process" << pid;

        // Give it a moment to terminate gracefully
        usleep(100000); // 0.1s

        if (kill(pid, 0) != 0) {
            emit processKilled(pid, true);
            return true;
        }
    } else {
        // SIGTERM failed, log the reason
        qWarning() << "SIGTERM failed for PID" << pid << ":" << strerror(errno);
    }

    // If still running, try SIGKILL
    if (kill(pid, SIGKILL) == 0) {
        qDebug() << "Sent SIGKILL to process" << pid;
        emit processKilled(pid, true);
        return true;
    }

    // Both attempts failed
    QString error = QString("Failed to kill process %1: %2").arg(pid).arg(strerror(errno));
    qWarning() << error;
    emit errorOccurred(error);
    emit processKilled(pid, false);
    return false;
}

// find process through its port
PortInfo PortManager::findProcessByPort(int port, const QString& protocol) {
    if (port <= 0 || port > 65535) {
        qWarning() << "Invalid port number:" << port;
        return PortInfo();
    }

    QList<PortInfo> ports = getOpenPorts();

    for (const auto& info : std::as_const(ports)) {
        if (info.port == port &&
            info.protocol.compare(protocol, Qt::CaseInsensitive) == 0) {
            return info;
        }
    }

    return PortInfo();
}

// Kill the process on a specific port
bool PortManager::killProcessOnPort(int port, const QString& protocol) {
    PortInfo info = findProcessByPort(port, protocol);

    if (info.pid <= 0) {
        QString error = QString("No process found on port %1 (%2)").arg(port).arg(protocol);
        qWarning() << error;
        emit errorOccurred(error);
        return false;
    }

    return killProcess(info.pid);
}

// Run get ports asynchronously
QFuture<QList<PortInfo>> PortManager::getOpenPortsAsync() {
    return QtConcurrent::run([this]() {
        return getOpenPorts();
    });
}
