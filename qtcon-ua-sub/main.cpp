#include <QCoreApplication>
#include <QOpcUaClient>
#include <QOpcUaProvider>
#include <QStringList>
#include <QDebug>
#include <QObject>
#include <QTimer>

// Define the server endpoint URL
const QString OpcUaEndpoint = "opc.tcp://m3:48400/UA/ComServerWrapper";

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

    // Connect to the stateChanged signal
    QObject::connect(client, &QOpcUaClient::stateChanged, [client](QOpcUaClient::ClientState state) {
        qDebug() << "Client state changed:" << state;
        if (state == QOpcUaClient::ClientState::Connected) {
            QOpcUaNode *node = client->node("ns=2;s=0:TEST1/SGGN1/OUT.CV");
            if (node) {
                qDebug() << "Node object created, enabling monitoring";

                // Connect to the attributeUpdated signal for subscription updates
                QObject::connect(node, &QOpcUaNode::attributeUpdated,
                                 [node](QOpcUa::NodeAttribute attr, QVariant value) {
                                     if (attr == QOpcUa::NodeAttribute::Value) {
                                         qDebug() << "Subscription update - Value:" << value;
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
        }
    });

    // Handle endpoint request completion
    QObject::connect(client, &QOpcUaClient::endpointsRequestFinished,
                     [client](QList<QOpcUaEndpointDescription> endpoints) {
                         qDebug() << "Endpoints returned:" << endpoints.count();
                         if (endpoints.size()) {
                             client->connectToEndpoint(endpoints.first());
                         } else {
                             qDebug() << "No endpoints available";
                         }
                     });

    // Set up a timer to keep the subscription running for demonstration
    QTimer *timer = new QTimer(&a);
    QObject::connect(timer, &QTimer::timeout, []() {
        qDebug() << "Subscription still active...";
    });
    timer->start(10000); // Print status every 10 seconds

    qDebug() << "Requesting endpoints from" << OpcUaEndpoint;
    client->requestEndpoints(QUrl(OpcUaEndpoint));

    return a.exec();
}
