#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <QEventLoop>
#include <QSettings>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <QOpcUaUserTokenPolicy>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_provider(nullptr)
    , m_client(nullptr)
    , m_node(nullptr)
    , m_connected(false)
{
    ui->setupUi(this);
    
    // Connect the Exit action to the exit slot
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::exitApplication);
    
    // Connect the Connect/Disconnect button
    connect(ui->pushButtonConnectDisconned, &QPushButton::clicked, this, &MainWindow::connectDisconnect);
    
    // Initialize OPC UA provider
    m_provider = new QOpcUaProvider(this);
    if (m_provider->availableBackends().isEmpty()) {
        QMessageBox::critical(this, "Error", "No OPC UA backends available");
        return;
    }
    
    // Set initial UI state
    ui->lineEditValue->setReadOnly(true);
    
    // Setup chart
    setupChart();

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveConfig);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::saveConfigAs);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openConfig);
    updateRecentMenu();

    // Restore the last opened/saved configuration
    const QStringList recent = recentFiles();
    if (!recent.isEmpty())
        loadConfig(recent.first());
}

QStringList MainWindow::recentFiles() const
{
    QStringList files = QSettings().value("recent/files").toStringList();
    files.erase(std::remove_if(files.begin(), files.end(),
                               [](const QString &f) { return !QFileInfo::exists(f); }),
                files.end());
    return files;
}

void MainWindow::addRecentFile(const QString &path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > 10)
        files.removeLast();
    QSettings().setValue("recent/files", files);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    ui->menuRecent->clear();
    const QStringList files = recentFiles();
    for (const QString &f : files) {
        QAction *a = ui->menuRecent->addAction(QDir::toNativeSeparators(f));
        connect(a, &QAction::triggered, this, [this, f]() {
            if (loadConfig(f))
                addRecentFile(f);
        });
    }
    ui->menuRecent->setEnabled(!files.isEmpty());
}

bool MainWindow::writeConfig(const QString &path)
{
    QSettings cfg(path, QSettings::IniFormat);
    cfg.setValue("connection/url", ui->lineEditUrl->text().trimmed());
    cfg.setValue("connection/nodeId", ui->lineEditNodeId->text().trimmed());
    cfg.setValue("window/geometry", saveGeometry());
    cfg.setValue("window/pos", pos());
    cfg.setValue("window/size", size());
    cfg.sync();
    if (cfg.status() != QSettings::NoError) {
        QMessageBox::critical(this, "Error", "Failed to write " + QDir::toNativeSeparators(path));
        return false;
    }
    m_cfgPath = path;
    addRecentFile(path);
    return true;
}

bool MainWindow::loadConfig(const QString &path)
{
    QSettings cfg(path, QSettings::IniFormat);
    if (cfg.status() != QSettings::NoError) {
        QMessageBox::critical(this, "Error", "Failed to read " + QDir::toNativeSeparators(path));
        return false;
    }
    ui->lineEditUrl->setText(cfg.value("connection/url", ui->lineEditUrl->text()).toString());
    ui->lineEditNodeId->setText(cfg.value("connection/nodeId", ui->lineEditNodeId->text()).toString());
    if (!restoreGeometry(cfg.value("window/geometry").toByteArray())) {
        if (cfg.contains("window/size"))
            resize(cfg.value("window/size").toSize());
        if (cfg.contains("window/pos"))
            move(cfg.value("window/pos").toPoint());
    }
    m_cfgPath = path;
    return true;
}

void MainWindow::saveConfig()
{
    if (m_cfgPath.isEmpty())
        saveConfigAs();
    else
        writeConfig(m_cfgPath);
}

void MainWindow::saveConfigAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Configuration", m_cfgPath,
                                                "Config files (*.cfg)");
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += ".cfg";
    writeConfig(path);
}

void MainWindow::openConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open Configuration", m_cfgPath,
                                                      "Config files (*.cfg)");
    if (!path.isEmpty() && loadConfig(path))
        addRecentFile(path);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    cleanupClient();
    QMainWindow::closeEvent(event);
}

void MainWindow::cleanupClient()
{
    m_connectAttemptsLeft = 0;
    if (!m_client)
        return;

    if (m_node) {
        m_node->disableMonitoring(QOpcUa::NodeAttribute::Value);
        delete m_node;
        m_node = nullptr;
    }

    if (m_client->state() != QOpcUaClient::ClientState::Disconnected) {
        // Disconnect is asynchronous; wait (bounded) for it to complete
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(m_client, &QOpcUaClient::stateChanged, &loop, [&](QOpcUaClient::ClientState s) {
            if (s == QOpcUaClient::ClientState::Disconnected)
                loop.quit();
        });
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        m_client->disconnectFromEndpoint();
        timeout.start(3000);
        if (m_client->state() != QOpcUaClient::ClientState::Disconnected)
            loop.exec();
    }
    m_connected = false;
    delete m_client;
    m_client = nullptr;
}

MainWindow::~MainWindow()
{
    cleanupClient();
    delete ui;
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::connectDisconnect()
{
    if (!m_connected) {
        // Connect
        QString url = ui->lineEditUrl->text().trimmed();
        if (url.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Please enter OPC UA URL");
            return;
        }
        
        if (m_client) {
            m_client->deleteLater();
        }
        
        m_client = m_provider->createClient(m_provider->availableBackends()[0]);
        if (!m_client) {
            QMessageBox::critical(this, "Error", "Failed to create OPC UA client");
            return;
        }
        
        connect(m_client, &QOpcUaClient::stateChanged, this, &MainWindow::onClientStateChanged);
        connect(m_client, &QOpcUaClient::endpointsRequestFinished,
                [this](QList<QOpcUaEndpointDescription> endpoints) {
                    if (endpoints.isEmpty()) {
                        QMessageBox::critical(this, "Error", "No endpoints available");
                        return;
                    }
                    // This build has no security support, so only an unsecured endpoint can be used
                    const QString nonePolicy = QStringLiteral("http://opcfoundation.org/UA/SecurityPolicy#None");
                    for (QOpcUaEndpointDescription endpoint : endpoints) {
                        if (endpoint.securityPolicy() == nonePolicy
                            && endpoint.securityMode() == QOpcUaEndpointDescription::MessageSecurityMode::None
                            && endpoint.endpointUrl().startsWith("opc.tcp://")) {
                            // Some servers advertise a non-None policy on the anonymous token,
                            // which can't be matched on an unsecured channel
                            QList<QOpcUaUserTokenPolicy> tokens = endpoint.userIdentityTokens();
                            for (QOpcUaUserTokenPolicy &t : tokens) {
                                if (t.tokenType() == QOpcUaUserTokenPolicy::TokenType::Anonymous)
                                    t.setSecurityPolicy(nonePolicy);
                            }
                            endpoint.setUserIdentityTokens(tokens);
                            m_endpoint = endpoint;
                            m_connectAttemptsLeft = 3;
                            m_client->connectToEndpoint(endpoint);
                            return;
                        }
                    }
                    QMessageBox::critical(this, "Error", "Server offers no unsecured (SecurityPolicy None) endpoint");
                    ui->pushButtonConnectDisconned->setText("Connect");
                    ui->pushButtonConnectDisconned->setEnabled(true);
                });
        
        ui->pushButtonConnectDisconned->setText("Connecting...");
        ui->pushButtonConnectDisconned->setEnabled(false);
        
        m_client->requestEndpoints(QUrl(url));
    } else {
        // Disconnect
        if (m_node) {
            m_node->disableMonitoring(QOpcUa::NodeAttribute::Value);
        }
        if (m_client) {
            m_client->disconnectFromEndpoint();
        }
    }
}

void MainWindow::onClientStateChanged(QOpcUaClient::ClientState state)
{
    qDebug() << "Client state changed:" << state;
    
    switch (state) {
    case QOpcUaClient::ClientState::Connected:
        {
            m_connected = true;
            ui->pushButtonConnectDisconned->setText("Disconnect");
            ui->pushButtonConnectDisconned->setEnabled(true);
            
            QString nodeId = ui->lineEditNodeId->text().trimmed();
            if (!nodeId.isEmpty()) {
                m_node = m_client->node(nodeId);
                if (m_node) {
                    connect(m_node, &QOpcUaNode::attributeUpdated, this, &MainWindow::onValueUpdated);
                    
                    QOpcUaMonitoringParameters parameters;
                    parameters.setSamplingInterval(1000); // 1 second
                    m_node->enableMonitoring(QOpcUa::NodeAttribute::Value, parameters);
                }
            }
        }
        break;
        
    case QOpcUaClient::ClientState::Disconnected:
        // The server can time out the first attempt, so retry a few times
        if (!m_connected && m_connectAttemptsLeft > 0 && m_client
            && m_client->error() != QOpcUaClient::NoError) {
            --m_connectAttemptsLeft;
            QPointer<QOpcUaClient> client = m_client;
            QTimer::singleShot(1000, this, [this, client]() {
                if (client && client == m_client && !m_connected)
                    client->connectToEndpoint(m_endpoint);
            });
            break;
        }
        m_connectAttemptsLeft = 0;
        m_connected = false;
        ui->pushButtonConnectDisconned->setText("Connect");
        ui->pushButtonConnectDisconned->setEnabled(true);
        ui->lineEditValue->clear();
        // Clear chart data
        m_series->clear();
        m_startTime = QDateTime::currentDateTime();
        break;
        
    case QOpcUaClient::ClientState::Connecting:
        ui->pushButtonConnectDisconned->setText("Connecting...");
        ui->pushButtonConnectDisconned->setEnabled(false);
        break;
        
    default:
        break;
    }
}

void MainWindow::onValueUpdated(QOpcUa::NodeAttribute attr, QVariant value)
{
    if (attr == QOpcUa::NodeAttribute::Value) {
        ui->lineEditValue->setText(value.toString());
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz AP") << "Value updated:" << value;
        
        // Add data point to chart
        bool ok;
        double numericValue = value.toDouble(&ok);
        if (ok) {
            addDataPoint(numericValue);
        }
    }
}

void MainWindow::setupChart()
{
    // Create chart
    m_chart = new QChart();
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->legend()->hide();
    
    // Create line series
    m_series = new QLineSeries();
    
    // Enable markers on data points
    m_series->setPointsVisible(true);
    m_series->setMarkerSize(4.0); // Size of the markers (50% smaller)
    
    m_chart->addSeries(m_series);
    
    // Create axes
    m_axisX = new QValueAxis();
    m_axisX->setTitleText("Time (seconds)");
    m_axisX->setRange(0, 60); // Show last 60 seconds
    m_axisX->setTickCount(7); // Show 7 tick marks
    
    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Value");
    m_axisY->setRange(0, 100); // Initial range, will auto-adjust
    
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);
    
    // Set chart to chart view
    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
    
    // Initialize start time
    m_startTime = QDateTime::currentDateTime();
}

void MainWindow::addDataPoint(double value)
{
    // Calculate time elapsed since start in seconds
    qint64 elapsed = m_startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
    
    // Add point to series
    m_series->append(elapsed, value);
    
    // Keep only last 100 points for performance
    if (m_series->count() > 100) {
        m_series->removePoints(0, m_series->count() - 100);
    }
    
    // Auto-adjust Y axis range
    if (!m_series->points().isEmpty()) {
        QList<QPointF> points = m_series->points();
        double minY = points.first().y();
        double maxY = points.first().y();
        
        for (const QPointF &point : points) {
            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }
        
        // Add some padding
        double padding = (maxY - minY) * 0.1;
        if (padding == 0) padding = 1; // Minimum padding
        
        m_axisY->setRange(minY - padding, maxY + padding);
    }
    
    // Auto-adjust X axis to show last 60 seconds
    if (elapsed > 60) {
        m_axisX->setRange(elapsed - 60, elapsed);
    } else {
        m_axisX->setRange(0, 60);
    }
}
