#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "memoryanalyzer.h"

#include <QIntValidator>
#include <QFileDialog>
#include <QProcess>
#include <QRegularExpression>
#include <QMessageBox>
#include <QHeaderView>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>
#include <QIcon>
#include <QVBoxLayout>
#include <QCompleter>
#include <QStringListModel>
#include <QDir>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set Style to avoid ui errors when bundling
    QApplication::setStyle("Fusion");
    // Fix ComboBox popup styling in AppImage (for some reason it glitches out, due to the css not being inherited)
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        combo->view()->window()->setStyleSheet(
            "QWidget { background-color: #2d2d2d; border: 1px solid #0e639c; }"
            );
        combo->view()->setStyleSheet(
            "QAbstractItemView { "
            "background-color: #2d2d2d; "
            "color: #ffffff; "
            "selection-background-color: #0e639c; "
            "border: none; "
            "outline: none; "
            "margin: 0px; "
            "padding: 0px; "
            "} "
            "QAbstractItemView::item { "
            "background-color: #2d2d2d; "
            "color: #ffffff; "
            "padding: 8px 12px; "
            "border: none; "
            "min-height: 25px; "
            "} "
            "QAbstractItemView::item:selected { "
            "background-color: #0e639c; "
            "} "
            "QAbstractItemView::item:hover { "
            "background-color: #3e3e3e; "
            "}"
            );
    }

    // Set mode
    ui->modeComboBox->setCurrentIndex(0);
    connect(ui->modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->stackedWidget, &QStackedWidget::setCurrentIndex);

    // Select process
    ui->pidLineEdit->setValidator(new QIntValidator(0, 999999, this));
    processModel = new QStringListModel(this);
    processCompleter = new QCompleter(processModel, this);
    processCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    processCompleter->setFilterMode(Qt::MatchContains);
    ui->processNameLineEdit->setCompleter(processCompleter);

    // Refresh timers
    processRefreshTimer = new QTimer(this);
    connect(processRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshProcessList);
    processRefreshTimer->start(2000);

    portRefreshTimer = new QTimer(this);
    connect(portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshPortList);

    // Memory Visualization
    memoryBar = new MemoryBar(this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(ui->memoryBarPlaceholder->layout());
    if (!layout) {
        layout = new QVBoxLayout(ui->memoryBarPlaceholder);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(memoryBar);

    // Set port manager
    portManager = new PortManager(this);
    setupPortTable();

    // Set future watchers for single and multi thread analysis
    singleAnalysisWatcher = new QFutureWatcher<ProcessMemorySummary>(this);
    multiAnalysisWatcher = new QFutureWatcher<ProcessMemorySummary>(this);

    connect(singleAnalysisWatcher, &QFutureWatcher<ProcessMemorySummary>::finished,
            this, &MainWindow::handleSingleAnalysisResult);
    connect(multiAnalysisWatcher, &QFutureWatcher<ProcessMemorySummary>::finished,
            this, &MainWindow::handleMultiAnalysisResult);

    // Connect slots to ui
    connect(ui->processNameLineEdit, &QLineEdit::editingFinished, this, &MainWindow::resolvePidFromInput);
    connect(ui->scanButton, &QPushButton::clicked, this, &MainWindow::onScanClicked);

    if (ui->portRefreshButton)
        connect(ui->portRefreshButton, &QPushButton::clicked, this, &MainWindow::refreshPortList);

    if (ui->killPortButton) {
        connect(ui->killPortButton, &QPushButton::clicked, this, &MainWindow::onKillPortProcess);
        connect(ui->portTableWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::onPortTableSelectionChanged);
        ui->killPortButton->setEnabled(false);
    }

    if (ui->portAutoRefreshCheck) {
        connect(ui->portAutoRefreshCheck, &QCheckBox::toggled, [this](bool checked) {
            if (checked) {
                portRefreshTimer->start(3000);
            } else {
                portRefreshTimer->stop();
            }
        });
    }

    // Modes
    ui->analysisModeCombo->clear();
    ui->analysisModeCombo->addItem("Single Process Mode");
    ui->analysisModeCombo->addItem("Application Group Mode (Related PIDs)");
    ui->analysisModeCombo->addItem("System-wide Mode (All Processes)");
    connect(ui->analysisModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAnalysisModeChanged);

    // INITIAL LOAD
    refreshProcessList();
    refreshPortList();
}

MainWindow::~MainWindow() {
    // Stop timers and running futures
    if (processRefreshTimer) processRefreshTimer->stop();
    if (portRefreshTimer) portRefreshTimer->stop();
    cleanupWatchers();
}

// Cleans up the watchers
void MainWindow::cleanupWatchers() {
    if (singleAnalysisWatcher && singleAnalysisWatcher->isRunning()) {
        singleAnalysisWatcher->cancel();
        singleAnalysisWatcher->waitForFinished();
    }
    if (multiAnalysisWatcher && multiAnalysisWatcher->isRunning()) {
        multiAnalysisWatcher->cancel();
        multiAnalysisWatcher->waitForFinished();
    }
}

// proc is a virtual file system in linux that contains all the insformation
// about every running process with each process having its own folder (named after its PID)
// Refresh procees list
void MainWindow::refreshProcessList() {
    QVector<ProcessInfo> list;
    QDir procDir("/proc");

    if (!procDir.exists()) {
        qWarning() << "Cannot access /proc directory";
        return;
    }

    // Store name and PID for all processes through /proc
    for (const QString& entry : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok;
        int pid = entry.toInt(&ok);
        if (ok && pid > 0) {
            QString name = MemoryAnalyzer::getProcessName(pid);
            if (!name.isEmpty()) {
                list.push_back({name, pid});
            }
        }
    }
    processCache = list;

    QStringList names;
    names.reserve(processCache.size());
    for (const auto& p : std::as_const(processCache)) {
        names << QString("%1 (PID %2)").arg(p.name).arg(p.pid);
    }
    processModel->setStringList(names);
}

// Set the process user has selected
void MainWindow::resolvePidFromInput() {
    QString text = ui->processNameLineEdit->text().trimmed();
    if (text.isEmpty()) return;

    QRegularExpression re("PID (\\d+)");
    auto match = re.match(text);
    if (match.hasMatch()) {
        currentPID = match.captured(1).toInt();
    }
}

// Change analysis mode
void MainWindow::onAnalysisModeChanged(int index) {
    if (index == 0) {
        currentMode = SingleThreadMode;
        ui->infoLabel->setText("Single Process Mode: Analyzing only the selected process");
    } else if (index == 1) {
        currentMode = ApplicationGroupMode;
        ui->infoLabel->setText("Application Group Mode: Analyzing selected process and all related processes");
    } else {
        currentMode = MultiThreadMode;
        ui->infoLabel->setText("System-wide Mode: Analyzing all accessible processes");
    }
}

// slot for SCAN BUTTON CLICKED
void MainWindow::onScanClicked() {
    ui->scanButton->setEnabled(false);

    if (currentMode == SingleThreadMode || currentMode == ApplicationGroupMode) {
        // Get PID
        if (ui->stackedWidget->currentIndex() == 0) {
            resolvePidFromInput();
        } else {
            currentPID = ui->pidLineEdit->text().toInt();
        }

        if (currentPID <= 0) {
            ui->infoLabel->setText("Error: Select a valid process first.");
            ui->scanButton->setEnabled(true);
            return;
        }

        if (currentMode == SingleThreadMode) {
            ui->infoLabel->setText(QString("Analyzing single PID %1...").arg(currentPID));
        } else {
            ui->infoLabel->setText(QString("Analyzing PID %1 and related processes...").arg(currentPID));
        }

        // Cancel any existing analysis
        if (singleAnalysisWatcher->isRunning()) {
            singleAnalysisWatcher->cancel();
            singleAnalysisWatcher->waitForFinished();
        }

        // Run async analysis
        auto future = QtConcurrent::run([](int pid, bool groupMode) {
            if (groupMode) {
                // Analyze the all related PIDs (application group)
                return MemoryAnalyzer::analyzeApplication(pid);
            } else {
                // Analyze ONLY the single PID
                return MemoryAnalyzer::analyzeSinglePid(pid);
            }
        }, currentPID, currentMode == ApplicationGroupMode);

        singleAnalysisWatcher->setFuture(future);

    } else {
        // Analye ALL processes
        QList<int> pids;
        pids.reserve(processCache.size());
        for (const auto& p : std::as_const(processCache)) {
            pids << p.pid;
        }

        if (pids.isEmpty()) {
            ui->infoLabel->setText("Error: No processes found to analyze.");
            ui->scanButton->setEnabled(true);
            return;
        }

        ui->infoLabel->setText(QString("Performing system-wide analysis of %1 processes...").arg(pids.size()));

        // Cancel any existing analysis
        if (multiAnalysisWatcher->isRunning()) {
            multiAnalysisWatcher->cancel();
            multiAnalysisWatcher->waitForFinished();
        }

        auto future = QtConcurrent::run([pids]() {
            ProcessMemorySummary total;
            total.processName = "System-wide Analysis";

            for (int pid : pids) {
                // se PSS (Proportional Set Size) to avoid double-counting shared memory
                ProcessMemorySummary s = MemoryAnalyzer::analyzeSinglePid(pid, true);
                total.pvt += s.pvt;
                total.stk += s.stk;
                total.img += s.img;
                total.map += s.map;
            }
            total.total = total.pvt + total.stk + total.img + total.map;
            return total;
        });

        multiAnalysisWatcher->setFuture(future);
    }
}

// RESULT HANDLERS after analysis
// Single PID
void MainWindow::handleSingleAnalysisResult() {
    if (singleAnalysisWatcher->isCanceled()) {
        ui->scanButton->setEnabled(true);
        return;
    }

    ProcessMemorySummary s = singleAnalysisWatcher->result();
    updateUIWithStats(s);
    ui->infoLabel->setText(QString("Analysis Complete: %1 (PID %2) - %3")
                               .arg(s.processName)
                               .arg(currentPID)
                               .arg(formatMemory(s.total)));
    ui->scanButton->setEnabled(true);
}

// ALL processes
void MainWindow::handleMultiAnalysisResult() {
    if (multiAnalysisWatcher->isCanceled()) {
        ui->scanButton->setEnabled(true);
        return;
    }

    ProcessMemorySummary s = multiAnalysisWatcher->result();
    updateUIWithStats(s);
    ui->infoLabel->setText(QString("Global Analysis Complete (%1 total)")
                               .arg(formatMemory(s.total)));
    ui->scanButton->setEnabled(true);
}

// Show memory in MB if bigger than 1024KB and GB if bigger than 1024MB
QString MainWindow::formatMemory(qint64 kb) const {
    if (kb >= 1024LL * 1024LL)
        return QString::number(kb / 1024.0 / 1024.0, 'f', 2) + " GB";
    if (kb >= 1024)
        return QString::number(kb / 1024.0, 'f', 1) + " MB";
    return QString::number(kb) + " KB";
}

// Update ui stats
void MainWindow::updateUIWithStats(const ProcessMemorySummary& s) {
    updateChangeLabel(ui->totalChangeLabel, s.total, lastStats.total);
    updateChangeLabel(ui->pvtChangeLabel, s.pvt, lastStats.pvt);
    updateChangeLabel(ui->stkChangeLabel, s.stk, lastStats.stk);
    updateChangeLabel(ui->imgChangeLabel, s.img, lastStats.img);
    updateChangeLabel(ui->mapChangeLabel, s.map, lastStats.map);

    memoryBar->setValues(s.pvt, s.stk, s.img, s.map);
    lastStats = {s.pvt, s.stk, s.img, s.map, s.total};
}
void MainWindow::updateChangeLabel(QLabel* label, long current, long previous) {
    if (!label) return;

    long diff = current - previous;
    QString sign = (diff > 0) ? "+" : "";
    QString color = (diff > 0) ? "red" : (diff < 0 ? "green" : "gray");
    label->setText(QString("%1%2 KB").arg(sign).arg(diff));
    label->setStyleSheet(QString("color: %1; font-weight: bold; font-family: 'Consolas';").arg(color));
}

// Functions related to Port table
// Initial port table setup (when changing modes)
void MainWindow::setupPortTable() {
    if (!ui->portTableWidget) return;

    ui->portTableWidget->setColumnCount(4);
    ui->portTableWidget->setHorizontalHeaderLabels({"Port", "PID", "Process", "Proto"});

    // Distribute space proportionally
    QHeaderView* header = ui->portTableWidget->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Interactive); // Port - user can resize
    header->setSectionResizeMode(1, QHeaderView::Interactive); // PID - user can resize
    header->setSectionResizeMode(2, QHeaderView::Stretch);     // Process - takes remaining space
    header->setSectionResizeMode(3, QHeaderView::Fixed);       // Proto - fixed size

    // Set initial widths
    ui->portTableWidget->setColumnWidth(0, 80);  // Port
    ui->portTableWidget->setColumnWidth(1, 80);  // PID
    ui->portTableWidget->setColumnWidth(3, 70);  // Proto (fixed)
    // Process column will autofill remaining

    ui->portTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->portTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->portTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->portTableWidget->setAlternatingRowColors(true); // Alternating colours for better readabilty

    ui->portTableWidget->setSortingEnabled(true); // Sorting enabled
}

// Refresh port list
void MainWindow::refreshPortList() {
    if (!portManager || !ui->portTableWidget) return;

    auto ports = portManager->getOpenPorts();

    // Disable sorting while updating to prevent issues
    ui->portTableWidget->setSortingEnabled(false);

    // Clear and rebuild the table
    ui->portTableWidget->setRowCount(0);
    ui->portTableWidget->setRowCount(ports.size());

    for (int i = 0; i < ports.size(); ++i) {
        const auto& p = ports[i];

        // Create items with proper data for sorting
        QTableWidgetItem* portItem = new QTableWidgetItem();
        portItem->setData(Qt::DisplayRole, p.port); // Store as number for proper sorting

        QTableWidgetItem* pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, p.pid); // Store as number for proper sorting

        QTableWidgetItem* processItem = new QTableWidgetItem(p.processName);
        QTableWidgetItem* protoItem = new QTableWidgetItem(p.protocol);

        ui->portTableWidget->setItem(i, 0, portItem);
        ui->portTableWidget->setItem(i, 1, pidItem);
        ui->portTableWidget->setItem(i, 2, processItem);
        ui->portTableWidget->setItem(i, 3, protoItem);
    }

    // Set total ports
    ui->portCountLabel->setText(QString("Total Ports: %1").arg(ports.size()));

    // Reenable sorting
    ui->portTableWidget->setSortingEnabled(true);
}

// Kill selected port process after displaying confirmation box
void MainWindow::onKillPortProcess() {
    auto selected = ui->portTableWidget->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    int row = selected.first().row();
    QTableWidgetItem* pidItem = ui->portTableWidget->item(row, 1);
    QTableWidgetItem* portItem = ui->portTableWidget->item(row, 0);

    if (!pidItem || !portItem) return;

    int pid = pidItem->text().toInt();
    int port = portItem->text().toInt();

    if (pid <= 0) {
        QMessageBox::warning(this, "Invalid PID", "Cannot kill process with invalid PID.");
        return;
    }

    auto reply = QMessageBox::question(this, "Kill Process",
                                       QString("Are you sure you want to kill process %1 on port %2?")
                                           .arg(pid).arg(port),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (portManager->killProcess(pid)) {
            QMessageBox::information(this, "Success",
                                     QString("Process %1 terminated successfully.").arg(pid));
            QTimer::singleShot(500, this, &MainWindow::refreshPortList);
        } else {
            QMessageBox::warning(this, "Failed",
                                 QString("Failed to kill process %1. You may need root privileges.").arg(pid));
        }
    }
}

// UI update when port table's selection changes
void MainWindow::onPortTableSelectionChanged() {
    if (!ui->killPortButton) return;

    bool hasSelection = !ui->portTableWidget->selectionModel()->selectedRows().isEmpty();
    ui->killPortButton->setEnabled(hasSelection);
}
