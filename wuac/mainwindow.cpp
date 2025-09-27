#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

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
        qDebug() << "Value updated:" << value;
    }
}
