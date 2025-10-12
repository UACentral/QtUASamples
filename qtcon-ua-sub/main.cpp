#include <QCoreApplication>
#include <QOpcUaClient>
#include <QOpcUaProvider>
#include <QStringList>
#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QTextStream>
#include <QSocketNotifier>
#include <QThread>

#ifdef Q_OS_WIN
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// Define the server endpoint URL
const QString OpcUaEndpoint = "opc.tcp://m3:48400/UA/ComServerWrapper";

class KeyboardHandler : public QObject
{
    Q_OBJECT

public:
    KeyboardHandler(QObject *parent = nullptr) : QObject(parent)
    {
#ifdef Q_OS_WIN
        // For Windows, we'll use a timer to periodically check for key presses
        keyCheckTimer = new QTimer(this);
        connect(keyCheckTimer, &QTimer::timeout, this, &KeyboardHandler::checkForKeyPress);
        keyCheckTimer->start(100); // Check every 100ms
#else
        // For Unix-like systems, set up non-blocking stdin
        setupUnixInput();
#endif
    }

public slots:
    void checkForKeyPress()
    {
#ifdef Q_OS_WIN
        if (_kbhit()) {
            int key = _getch();
            if (key == 27) { // Escape key
                emit escapePressed();
            }
        }
#endif
    }

signals:
    void escapePressed();

private:
#ifdef Q_OS_WIN
    QTimer *keyCheckTimer;
#else
    void setupUnixInput()
    {
        // Set stdin to non-blocking mode
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        
        QSocketNotifier *notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, &KeyboardHandler::readStdin);
    }
    
    void readStdin()
    {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 27) { // Escape key
                emit escapePressed();
            }
        }
    }
#endif
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QOpcUaProvider provider;
    if (provider.availableBackends().isEmpty()) {
        qDebug() << "No OPC UA backends available";
        return 1;
    }
    
    QOpcUaClient *client = provider.createClient(provider.availableBackends()[0]);
    if (!client) {
        qDebug() << "Failed to create OPC UA client";
        return 2;
    }

    QOpcUaNode *node = nullptr;

    // Connect to the stateChanged signal
    QObject::connect(client, &QOpcUaClient::stateChanged, [client, &node, &a](QOpcUaClient::ClientState state) {
        qDebug() << "Client state changed:" << state;
        if (state == QOpcUaClient::ClientState::Connected) {
            node = client->node("ns=2;s=0:TEST1/SGGN1/OUT.CV");
            if (node) {
                qDebug() << "Node object created, enabling monitoring";

                // Connect to the attributeUpdated signal for subscription updates
                QObject::connect(node, &QOpcUaNode::attributeUpdated,
                                 [node](QOpcUa::NodeAttribute attr, QVariant value) {
                                     if (attr == QOpcUa::NodeAttribute::Value) {
                                        qDebug() << qPrintable(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz AP") + " Subscription update - Value: " + value.toString());
                                     }
                                 });

                // Connect to enableMonitoringFinished to confirm subscription is active
                QObject::connect(node, &QOpcUaNode::enableMonitoringFinished,
                                 [](QOpcUa::NodeAttribute attr, QOpcUa::UaStatusCode status) {
                                     qDebug() << "Monitoring enabled for attribute" << attr << "Status:" << status;
                                 });

                // Enable monitoring (subscription) for the Value attribute
                QOpcUaMonitoringParameters parameters;
                parameters.setSamplingInterval(1000); // 1 second sampling interval
                node->enableMonitoring(QOpcUa::NodeAttribute::Value, parameters);
            } else {
                qDebug() << "Failed to create node object";
            }
        } else if (state == QOpcUaClient::ClientState::Disconnected) {
            qDebug() << "Disconnected state, ESC key or server exited";
            a.quit();
        }
    });

    // Handle endpoint request completion
    QObject::connect(client, &QOpcUaClient::endpointsRequestFinished,
                     [client, &node](QList<QOpcUaEndpointDescription> endpoints) {
                         qDebug() << "Endpoints returned:" << endpoints.count();
                         if (endpoints.size()) {
                             client->connectToEndpoint(endpoints.first());
                         } else {
                             qDebug() << "No endpoints available";
                         }
                     });

    // Set up keyboard handler for Escape key
    KeyboardHandler *keyHandler = new KeyboardHandler(&a);
    QObject::connect(keyHandler, &KeyboardHandler::escapePressed, [&a, client, node]() {
        qDebug() << "Escape key pressed. Shutting down...";

        if (node) {
            node->disableMonitoring(QOpcUa::NodeAttribute::Value);
        }


        if (client && client->state() == QOpcUaClient::ClientState::Connected) {
        //if (client) {
            qDebug() << "Disconnecting from endpoint...";
            client->disconnectFromEndpoint();
        }
    });

    // Set up a timer to keep the subscription running for demonstration
    QTimer *timer = new QTimer(&a);
    QObject::connect(timer, &QTimer::timeout, []() {
        qDebug() << "Subscription active... (Press Escape to quit)";
    });
    timer->start(10000); // Print status every 10 seconds

    qDebug() << "Requesting endpoints from" << OpcUaEndpoint;
    qDebug() << "Press Escape key to quit the application";
    client->requestEndpoints(QUrl(OpcUaEndpoint));

    return a.exec();
}

#include "main.moc"
