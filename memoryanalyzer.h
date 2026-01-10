#ifndef MEMORYANALYZER_H
#define MEMORYANAL_H

#include <windows.h>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

struct MemoryRegion {
    QString type;
    QString base;
    long size_kb;
};

class MemoryAnalyzer {
public:
    explicit MemoryAnalyzer(DWORD processID);
    void analyze();

    QString toJsonString() const;
    QJsonObject toJsonObject() const;

private:
    DWORD m_pid;
    QList<MemoryRegion> m_regions;

    QString toHex(LPCVOID ptr) const;
    bool isStack(const MEMORY_BASIC_INFORMATION& mbi) const;
};

#endif // MEMORYANALYZER_H
