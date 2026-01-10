#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "memoryanalyzer.h"

#include <QIntValidator>
#include <QFileDialog>
#include <QProcess>
#include <QRegularExpression>
#include <utility>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Connect input selector to input fields
    ui->modeComboBox->setCurrentIndex(0);
    connect(ui->modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->stackedWidget, &QStackedWidget::setCurrentIndex);

    // Validate PID input
    ui->pidLineEdit->setValidator(new QIntValidator(0, 999999, this));

    // Setup completer
    processModel = new QStringListModel(this);
    processCompleter = new QCompleter(processModel, this);
    processCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    processCompleter->setFilterMode(Qt::MatchContains);
    ui->processNameLineEdit->setCompleter(processCompleter);

    // Setup refresh times
    processRefreshTimer = new QTimer(this);
    connect(processRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshProcessList);
    processRefreshTimer->start(2000); // refresh every 2 secs

    // Load process list initially
    refreshProcessList();

#   // Connect Live filtering with process input
    connect(ui->processNameLineEdit, &QLineEdit::editingFinished,
            this, &MainWindow::resolvePidFromInput);

    // Connect browse button with slot to browse the file
    connect(ui->browseButton1, &QPushButton::clicked, this, [this]() {
        browseExe(ui->exePathLineEdit1);
    });

    // Conect Scan button
    connect(ui->scanButton, &QPushButton::clicked, this, &MainWindow::onScanClicked);

}

// Helper functions
// Extract all running processes
QVector<ProcessInfo> getRunningProcesses()
{
    QVector<ProcessInfo> list;
    QProcess proc;
    proc.start("tasklist", { "/FO", "CSV", "/NH" });
    proc.waitForFinished();

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split("\n", Qt::SkipEmptyParts);

    for (const QString& line : std::as_const(lines)) {
        QString cleaned = line;
        cleaned.remove("\"");
        QStringList parts = cleaned.split(",");
        if (parts.size() >= 2) {
            list.push_back({ parts[0].trimmed(), parts[1].toInt() });
        }
    }
    return list;
}

// Analyze the PID's memory usage (reference based)
void updateMemoryInfo(DWORD pid, long &pvt, long &stk, long &img, long &map) {
    MemoryAnalyzer analyzer(pid);
    analyzer.analyze();

    // Reset values to zero at the start
    pvt = 0; stk = 0; img = 0; map = 0;

    QJsonObject data = analyzer.toJsonObject();
    QJsonArray regions = data["regions"].toArray();

    for (const QJsonValue &value : regions) {
        QJsonObject reg = value.toObject();
        QString type = reg["type"].toString();
        long size = static_cast<long>(reg["size_kb"].toDouble());

        if (type == "PRIVATE") pvt += size;
        else if (type == "STACK") stk += size;
        else if (type == "IMAGE") img += size;
        else if (type == "MAPPED") map += size;
    }
}



// Slot functions
// Update the process list
void MainWindow::refreshProcessList()
{
    processCache = getRunningProcesses();
    QStringList items;
    for (const auto& proc : std::as_const(processCache)) {
        items << QString("%1 (PID %2)").arg(proc.name).arg(proc.pid);
    }
    processModel->setStringList(items);
}

void MainWindow::onScanClicked()
{
    int currentMode = ui->stackedWidget->currentIndex();

    // 1. Resolve the PID based on the active Tab
    if (currentMode == 0) { // Launch and Observe
        QString programPath = ui->exePathLineEdit1->text();
        if (programPath.isEmpty()) {
            ui->infoLabel->setText("Error: No executable selected.");
            return;
        }

        qint64 pid;
        if (QProcess::startDetached(programPath, QStringList(), QString(), &pid)) {
            currentPID = static_cast<int>(pid);
        } else {
            ui->infoLabel->setText("Error: Failed to launch program.");
            return;
        }
    }
    else if (currentMode == 1) { // Search/Observe
        resolvePidFromInput();
    }
    else if (currentMode == 2) { // PID Input
        currentPID = ui->pidLineEdit->text().toInt();
    }

    // 2. Analyze and Display
    if (currentPID > 0) {
        long pvt = 0, stk = 0, img = 0, map = 0;

        // Use your reference-based function
        updateMemoryInfo(static_cast<DWORD>(currentPID), pvt, stk, img, map);

        // Calculate Total
        long total = pvt + stk + img + map;

        // 3. Update infoLabel with your requested format
        QString displayContent = QString(
                                     "PID: %1\n"
                                     "PRIVATE: %2 KB\n"
                                     "STACK: %3 KB\n"
                                     "IMAGE: %4 KB\n"
                                     "MAPPED: %5 KB\n"
                                     "------------------\n"
                                     "TOTAL: %6 KB"
                                     ).arg(currentPID).arg(pvt).arg(stk).arg(img).arg(map).arg(total);

        ui->infoLabel->setText(displayContent);

    } else {
        ui->infoLabel->setText("Invalid PID. Please select or launch a process.");
    }
}

// Live filtering
void MainWindow::resolvePidFromInput()
{
    QRegularExpression re("PID (\\d+)");
    auto match = re.match(ui->processNameLineEdit->text());

    if (match.hasMatch()) {
        currentPID = match.captured(1).toInt();
    }
}

// Check if PID is valid
void MainWindow::validatePidInput()
{
    // TO DO
}

// Browse functions to select exec file
void MainWindow::browseExe(QLineEdit* target)
{
    QString path = QFileDialog::getOpenFileName(this, "Select Executable", "", "Executables (*.exe)");
    if (!path.isEmpty()) target->setText(path);
}



MainWindow::~MainWindow()
{
    delete ui;
}
