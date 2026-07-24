#ifndef EVENT_LIST_WIDGET_H
#define EVENT_LIST_WIDGET_H

#include <QWidget>
#include "backend_bridge.h"

// UI 生成的头文件（qmake 自动从 .ui 生成）
#include "ui_event_list_widget.h"

class EventListWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventListWidget(BackendBridge* bridge, QWidget* parent = nullptr);

public slots:
    void refresh();
    void renderEvents(QVector<BackendBridge::EventEntry> events);

private slots:
    void on_simBtn_clicked();

private:
    void fillCombo();

    Ui::EventListWidget ui;
    BackendBridge* bridge_;
};

#endif
