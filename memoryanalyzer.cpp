#include "memoryanalyzer.h"
#include <psapi.h>
#include <QDateTime>
#include <QDebug>

MemoryAnalyzer::MemoryAnalyzer(DWORD processID) : m_pid(processID) {}

void MemoryAnalyzer::analyze() {
    m_regions.clear();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m_pid);
    if (!hProcess) {
        qCritical() << "Error! Process could not be opened." << m_pid << "Last Error:" << GetLastError();
        return;
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    MEMORY_BASIC_INFORMATION mbi;
    unsigned char* addr = reinterpret_cast<unsigned char*>(si.lpMinimumApplicationAddress);

    while (addr < reinterpret_cast<unsigned char*>(si.lpMaximumApplicationAddress)) {
        if (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT) {
                QString type = "PRIVATE";

                if (mbi.Type == MEM_IMAGE) {
                    type = "IMAGE";
                } else if (mbi.Type == MEM_MAPPED) {
                    type = "MAPPED";
                } else if (isStack(mbi)) {
                    type = "STACK";
                } else if (mbi.Type == MEM_PRIVATE) {
                    type = "PRIVATE";
                }

                m_regions.append({
                    type,
                    toHex(mbi.BaseAddress),
                    static_cast<long>(mbi.RegionSize / 1024)
                });
            }
            addr += mbi.RegionSize;
        } else {
            break;
        }
    }
    CloseHandle(hProcess);
}

QJsonObject MemoryAnalyzer::toJsonObject() const {
    QJsonObject root;
    root["pid"] = static_cast<double>(m_pid);
    root["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonArray regionsArray;
    for (const auto& region : m_regions) {
        QJsonObject regionObj;
        regionObj["type"] = region.type;
        regionObj["base"] = region.base;
        regionObj["size_kb"] = static_cast<double>(region.size_kb);
        regionsArray.append(regionObj);
    }
    root["regions"] = regionsArray;
    return root;
}

QString MemoryAnalyzer::toJsonString() const {
    QJsonDocument doc(toJsonObject());
    return doc.toJson(QJsonDocument::Indented);
}

QString MemoryAnalyzer::toHex(LPCVOID ptr) const {
    return QString("0x%1").arg(reinterpret_cast<uintptr_t>(ptr), 16, 16, QChar('0')).toUpper();
}

bool MemoryAnalyzer::isStack(const MEMORY_BASIC_INFORMATION& mbi) const {
    return (mbi.Protect & PAGE_GUARD);
}
