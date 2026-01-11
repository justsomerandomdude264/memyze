#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QStringListModel>
#include <QCompleter>
#include "memorybar.h"

struct ProcessInfo {
    QString name;
    int pid;
};

QVector<ProcessInfo> getRunningProcesses();

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

private:
    Ui::MainWindow *ui;

    MemoryBar* memoryBar = nullptr;
    QVector<ProcessInfo> processCache;
    QCompleter* processCompleter = nullptr;
    QStringListModel* processModel = nullptr;
    QTimer* processRefreshTimer = nullptr;
    int currentPID = -1;
};

#endif // MAINWINDOW_H
