#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class BackendBridge;
class EventListWidget;
class AlarmCatalogWidget;
class QTabWidget;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void updateStatusBar();

private:
    void setupUI();

    BackendBridge*      bridge_;
    QTabWidget*         tabs_;
    EventListWidget*    eventListPage_;
    AlarmCatalogWidget* catalogPage_;
    QLabel*             statusLabel_;
    QTimer*             statusTimer_;
};

#endif
