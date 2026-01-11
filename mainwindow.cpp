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

    // Connect live filtering with process input
    connect(ui->processNameLineEdit, &QLineEdit::editingFinished,
            this, &MainWindow::resolvePidFromInput);

    // Conect Scan button
    connect(ui->scanButton, &QPushButton::clicked, this, &MainWindow::onScanClicked);

    // Add layout to our memeory bar widget
    memoryBar = new MemoryBar(this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(ui->memoryBarPlaceholder->layout());
    if (!layout) {
        layout = new QVBoxLayout(ui->memoryBarPlaceholder);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(memoryBar);


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

    pvt = 0; stk = 0; img = 0; map = 0;

    QJsonObject data = analyzer.toJsonObject();
    QJsonArray regions = data["regions"].toArray();

    // Compute totals for all memory locations
    for (const QJsonValue &value : std::as_const(regions)) {
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

// Analyzes the current PID and changes the infoLabel
void MainWindow::onScanClicked()
{
    int currentMode = ui->stackedWidget->currentIndex();

    // Idx 0 - Observe running program
    if (currentMode == 0) {
        resolvePidFromInput();
    }
    // Idx 1 - Manual PID Input
    else if (currentMode == 1) {
        currentPID = ui->pidLineEdit->text().toInt();
    }

    // Analyze and Display
    if (currentPID > 0) {
        long pvt = 0, stk = 0, img = 0, map = 0;
        updateMemoryInfo(static_cast<DWORD>(currentPID), pvt, stk, img, map);

        memoryBar->setValues(pvt, stk, img, map);
        ui->infoLabel->setText(QString("Data from PID: %1").arg(currentPID));
    } else {
        ui->infoLabel->setText("Invalid PID!");
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

MainWindow::~MainWindow()
{
    delete ui;
}
