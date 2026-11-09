#include <QCoreApplication>
#include <QOpcUaClient>
#include <QDateTime>
#include <QOpcUaProvider>
#include <QOpcUaUserTokenPolicy>
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

// Defaults, overridable via command line: qtcon-ua-sub [endpointUrl] [nodeId]
const QString DefaultEndpoint = "opc.tcp://m3:48400/UA/ComServerWrapper";
const QString DefaultNodeId = "ns=2;s=0:TEST1/SGGN1/OUT.CV";

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

    const QStringList args = a.arguments();
    if (args.contains("-h") || args.contains("--help") || args.size() > 3) {
        qDebug().noquote() << "Usage:" << args.value(0) << "[endpointUrl] [nodeId]\n"
                           << "  endpointUrl  default:" << DefaultEndpoint << "\n"
                           << "  nodeId       default:" << DefaultNodeId;
        return args.size() > 3 ? 4 : 0;
    }
    const QString OpcUaEndpoint = args.value(1, DefaultEndpoint);
    const QString nodeId = args.value(2, DefaultNodeId);

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
    QObject::connect(client, &QOpcUaClient::stateChanged, [client, &node, &a, nodeId](QOpcUaClient::ClientState state) {
        qDebug() << qPrintable(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")) << "Client state changed:" << state;
        if (state == QOpcUaClient::ClientState::Connected) {
            node = client->node(nodeId);
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
                     [client, &a](QList<QOpcUaEndpointDescription> endpoints, QOpcUa::UaStatusCode status, QUrl requestUrl) {
                         qDebug() << "Endpoints returned:" << endpoints.count() << "Status:" << status << "URL:" << requestUrl;
                         static const QString nonePolicy = "http://opcfoundation.org/UA/SecurityPolicy#None";
                         // This client only uses SecurityMode None / SecurityPolicy None
                         for (QOpcUaEndpointDescription ep : endpoints) {
                             if (ep.securityMode() != QOpcUaEndpointDescription::MessageSecurityMode::None
                                 || ep.securityPolicy() != nonePolicy
                                 || !ep.endpointUrl().startsWith("opc.tcp://"))
                                 continue;
                             // Some servers advertise a non-None policy on the anonymous token,
                             // which can't be matched on an unsecured channel
                             QList<QOpcUaUserTokenPolicy> tokens = ep.userIdentityTokens();
                             for (QOpcUaUserTokenPolicy &t : tokens) {
                                 if (t.tokenType() == QOpcUaUserTokenPolicy::TokenType::Anonymous)
                                     t.setSecurityPolicy(nonePolicy);
                             }
                             ep.setUserIdentityTokens(tokens);
                             client->connectToEndpoint(ep);
                             return;
                         }
                         {
                             qDebug() << "No None/None endpoint available";
                             a.exit(3);
                         }
                     });

    QObject::connect(client, &QOpcUaClient::errorChanged, [](QOpcUaClient::ClientError error) {
        qDebug() << "Client error:" << error;
    });

    // Set up keyboard handler for Escape key
    KeyboardHandler *keyHandler = new KeyboardHandler(&a);
    QObject::connect(keyHandler, &KeyboardHandler::escapePressed, [&a, client, &node]() {
        qDebug() << "Escape key pressed. Shutting down...";

        if (node) {
            node->disableMonitoring(QOpcUa::NodeAttribute::Value);
        }

        if (client && client->state() == QOpcUaClient::ClientState::Connected) {
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
