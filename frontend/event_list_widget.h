#ifndef EVENT_LIST_WIDGET_H
#define EVENT_LIST_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>
#include "backend_bridge.h"

class EventListWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventListWidget(BackendBridge* bridge, QWidget* parent = nullptr);

public slots:
    void refresh();

private slots:
    void onSimulateBtn();
    void onClearSelected();

private:
    void setupUI();

    BackendBridge* bridge_;
    QTableWidget*  table_;
    QLabel*        statusLabel_;
    QTimer*        refreshTimer_;
};

#endif
