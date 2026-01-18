#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QStringListModel>
#include <QCompleter>
#include <QFutureWatcher>
#include <QTableWidget>
#include <QScopedPointer>
#include "memoryanalyzer.h"
#include "memorybar.h"
#include "portmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

using ProcessID = int;

enum AnalysisMode {
    SingleThreadMode = 0,
    ApplicationGroupMode,
    MultiThreadMode
};

struct LastStats {
    long pvt = 0;
    long stk = 0;
    long img = 0;
    long map = 0;
    long total = 0;
};

struct ProcessInfo {
    QString name;
    ProcessID pid;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // --- UI  ---
    void onScanClicked();
    void onAnalysisModeChanged(int index);
    void resolvePidFromInput();

    // --- Port management ---
    void onKillPortProcess();
    void onPortTableSelectionChanged();

    // --- Helpers ---
    void refreshProcessList();
    void refreshPortList();
    void setupPortTable();
    QString formatMemory(qint64 kb) const;
    void updateUIWithStats(const ProcessMemorySummary& s);
    void updateChangeLabel(QLabel* label, long current, long previous);

    // --- Async analysis result handlers ---
    void handleSingleAnalysisResult();
    void handleMultiAnalysisResult();

private:
    QScopedPointer<Ui::MainWindow> ui;

    // --- Process Selection ---
    QVector<ProcessInfo> processCache;
    QStringListModel* processModel = nullptr;
    QCompleter* processCompleter = nullptr;
    ProcessID currentPID = 0;
    AnalysisMode currentMode = SingleThreadMode;

    // --- Memory Visualization ---
    MemoryBar* memoryBar = nullptr;
    LastStats lastStats;

    // --- Timers ---
    QTimer* processRefreshTimer = nullptr;
    QTimer* portRefreshTimer = nullptr;

    // --- Port Management ---
    PortManager* portManager = nullptr;

    // --- Future Watchers ---
    QFutureWatcher<ProcessMemorySummary>* singleAnalysisWatcher = nullptr;
    QFutureWatcher<ProcessMemorySummary>* multiAnalysisWatcher = nullptr;

    // --- Helper methods ---
    void cleanupWatchers();
};

#endif // MAINWINDOW_H
