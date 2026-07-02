#ifndef ALARM_CATALOG_WIDGET_H
#define ALARM_CATALOG_WIDGET_H

#include <QWidget>
#include "backend_bridge.h"

#include "ui_alarm_catalog_widget.h"

class AlarmCatalogWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent = nullptr);

public slots:
    void loadCatalog();
    void on_applyBtn_clicked();

private:
    Ui::AlarmCatalogWidget ui;
    BackendBridge* bridge_;
};

#endif
