#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
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
}

MainWindow::~MainWindow()
{
    if (m_client && m_connected) {
        m_client->disconnectFromEndpoint();
    }
    delete ui;
}

void MainWindow::exitApplication()
{
    QApplication::quit();
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
                    m_client->connectToEndpoint(endpoints.first());
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
    m_chart->setTitle("OPC UA Data");
    m_chart->setAnimationOptions(QChart::NoAnimation);
    
    // Create line series
    m_series = new QLineSeries();
    m_series->setName("Value");
    
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
