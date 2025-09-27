#include <QCoreApplication>
#include <QOpcUaClient>
#include <QOpcUaProvider>
#include <QStringList>
#include <QDebug>
#include <QObject> // Needed for QObject::connect

// Define the server endpoint URL
const QString OpcUaEndpoint = "opc.tcp://m3:48400/UA/ComServerWrapper";

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QOpcUaProvider provider;
    if (provider.availableBackends().isEmpty())
        return 1;
    QOpcUaClient *client = provider.createClient(provider.availableBackends()[0]);
    if (!client)
        return 2;
    // Connect to the stateChanged signal. Compatible slots of QObjects can be used instead of a lambda.
    QObject::connect(client, &QOpcUaClient::stateChanged, [client](QOpcUaClient::ClientState state) {
        qDebug() << "Client state changed:" << state;
        if (state == QOpcUaClient::ClientState::Connected) {
            QOpcUaNode *node = client->node("ns=2;s=0:TEST1/SGGN1/OUT.CV");
            if (node) {
                qDebug() << "A node object has been created";

                QObject::connect(node, &QOpcUaNode::attributeRead,
                                 [node, client](QOpcUa::NodeAttributes attr) {
                                     if (attr == QOpcUa::NodeAttribute::Value) {
                                         qDebug() << "Value: " << node->attribute(QOpcUa::NodeAttribute::Value);

                                         client->deleteLater();
                                         QCoreApplication::quit();
                                     }
                                 });

                node->readValueAttribute();
            }

        }
    });

    QObject::connect(client, &QOpcUaClient::endpointsRequestFinished,
                     [client](QList<QOpcUaEndpointDescription> endpoints) {
                         qDebug() << "Endpoints returned:" << endpoints.count();
                         if (endpoints.size())
                             client->connectToEndpoint(endpoints.first()); // Connect to the first endpoint in the list
                     });

    client->requestEndpoints(QUrl("opc.tcp://m3:48400/UA/ComServerWrapper")); // Request a list of endpoints from the server

    return a.exec();
}
