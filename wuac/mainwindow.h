#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QOpcUaClient>
#include <QOpcUaProvider>
#include <QOpcUaNode>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void exitApplication();
    void connectDisconnect();
    void onClientStateChanged(QOpcUaClient::ClientState state);
    void onValueUpdated(QOpcUa::NodeAttribute attr, QVariant value);

private:
    Ui::MainWindow *ui;
    QOpcUaProvider *m_provider;
    QOpcUaClient *m_client;
    QOpcUaNode *m_node;
    bool m_connected;
};
#endif // MAINWINDOW_H
