#ifndef MEMORYANALYZER_H
#define MEMORYANALYZER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QMetaType>

typedef int ProcessID;

// Holds all memory of a single process (in KB)
struct ProcessMemorySummary {
    ProcessID pid = 0;
    QString processName;
    long pvt = 0;   // Private or Heap memory
    long stk = 0;   // Stack memory
    long img = 0;   // Executable images and shared libraries (.so)
    long map = 0;   // Memory mapped files
    long total = 0; // Total
};

// Static only, only utility class no instances
class MemoryAnalyzer
{
public:
    // Main Analysis
    static ProcessMemorySummary analyzeSinglePid(ProcessID pid, bool usePSS = false);
    static ProcessMemorySummary analyzeApplication(ProcessID rootPid, bool usePSS = true);

    // Helper Functions
    static QString getExePath(ProcessID pid);
    static QString getProcessName(ProcessID pid);
    static QList<ProcessID> findRelatedPids(ProcessID pid);

private:
    MemoryAnalyzer() = delete;
    ~MemoryAnalyzer() = delete;
    MemoryAnalyzer(const MemoryAnalyzer&) = delete;
    MemoryAnalyzer& operator=(const MemoryAnalyzer&) = delete;
};

Q_DECLARE_METATYPE(ProcessMemorySummary)

#endif // MEMORYANALYZER_H
