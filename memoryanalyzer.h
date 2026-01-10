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

    // Core analysis function
    void analyze();

    // Returns data as a formatted JSON string (Qt style)
    QString toJsonString() const;

    // Returns data as a QJsonObject for further Qt processing
    QJsonObject toJsonObject() const;

private:
    DWORD m_pid;
    QList<MemoryRegion> m_regions;

    QString toHex(LPCVOID ptr) const;
    bool isStack(const MEMORY_BASIC_INFORMATION& mbi) const;
};

#endif // MEMORYANALYZER_H
