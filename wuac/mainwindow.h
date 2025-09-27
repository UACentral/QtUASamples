#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QOpcUaClient>
#include <QOpcUaProvider>
#include <QOpcUaNode>
#include <QDateTime>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

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
    
    // Chart components
    QChart *m_chart;
    QLineSeries *m_series;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
    QDateTime m_startTime;
    
    void setupChart();
    void addDataPoint(double value);
};
#endif // MAINWINDOW_H
