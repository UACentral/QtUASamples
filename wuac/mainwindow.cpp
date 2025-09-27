#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Connect the Exit action to the exit slot
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::exitApplication);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::exitApplication()
{
    QApplication::quit();
}
