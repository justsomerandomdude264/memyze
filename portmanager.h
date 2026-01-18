#ifndef PORTMANAGER_H
#define PORTMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QFuture>
#include <sys/types.h>

typedef pid_t ProcessID;

struct PortInfo {
    int port = 0;
    QString protocol;
    QString localAddress;
    QString remoteAddress;
    QString state;
    ProcessID pid = 0;
    QString processName;
};

class PortManager : public QObject {
    Q_OBJECT

public:
    explicit PortManager(QObject *parent = nullptr);
    ~PortManager() override = default;

    QList<PortInfo> getOpenPorts();

    PortInfo findProcessByPort(int port, const QString& protocol = "TCP");
    bool killProcess(ProcessID pid);
    bool killProcessOnPort(int port, const QString& protocol = "TCP");
    QFuture<QList<PortInfo>> getOpenPortsAsync();

signals:
    void portScanCompleted(int totalPorts);
    void processKilled(ProcessID pid, bool success);
    void errorOccurred(const QString& error);

private:
    QString getProcessNameByPID(ProcessID pid);
    QMap<unsigned long, ProcessID> getInodeToPidMap();
};

#endif // PORTMANAGER_H
