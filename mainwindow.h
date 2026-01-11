#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <QMainWindow>
#include <QMainWindow>
#include <QLineEdit>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QStringListModel>
#include <QCompleter>
#include <QLabel>
#include "memorybar.h"

struct ProcessInfo {
    QString name;
    int pid;
};

struct MemoryStats {
    long pvt = 0, stk = 0, img = 0, map = 0, total = 0;
};

QVector<ProcessInfo> getRunningProcesses();

void updateMemoryInfo(DWORD pid, long &pvt, long &stk, long &img, long &map);

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void validatePidInput();
    void resolvePidFromInput();
    void refreshProcessList();
    void onScanClicked();
    void updateChangeLabel(QLabel* label, long current, long previous);

private:
    Ui::MainWindow *ui;

    MemoryStats lastStats;
    int lastAnalyzedPID = -1;

    MemoryBar* memoryBar = nullptr;
    QVector<ProcessInfo> processCache;
    QCompleter* processCompleter = nullptr;
    QStringListModel* processModel = nullptr;
    QTimer* processRefreshTimer = nullptr;
    int currentPID = -1;
};

#endif // MAINWINDOW_H
