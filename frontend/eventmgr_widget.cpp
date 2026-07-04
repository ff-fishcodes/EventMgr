#include "eventmgr_widget.h"
#include "backend_bridge.h"
#include "event_list_widget.h"
#include "alarm_catalog_widget.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QTimer>

EventMgrWidget::EventMgrWidget(QWidget* parent)
    : QWidget(parent), bridge_(nullptr) {
    bridge_ = new BackendBridge(this);
    bridge_->initialize();
    setupUI();
}

EventMgrWidget::~EventMgrWidget() {}

void EventMgrWidget::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Tab 页面
    tabs_ = new QTabWidget(this);
    eventListPage_ = new EventListWidget(bridge_, this);
    tabs_->addTab(eventListPage_, "事件列表");
    catalogPage_ = new AlarmCatalogWidget(bridge_, this);
    tabs_->addTab(catalogPage_, "报警配置");
    layout->addWidget(tabs_);

    // 底部屏蔽状态条
    shieldLabel_ = new QLabel();
    shieldLabel_->setStyleSheet("background:#f0f0f0; padding:4px;");
    layout->addWidget(shieldLabel_);

    // Tab 切换时自动刷新目标页面
    connect(tabs_, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));

    statusTimer_ = new QTimer(this);
    connect(statusTimer_, SIGNAL(timeout()), this, SLOT(updateShieldStatus()));
    statusTimer_->start(1000);
    updateShieldStatus();
}

void EventMgrWidget::onTabChanged(int index) {
    switch (index) {
        case 0: eventListPage_->refresh();     break;
        case 1: catalogPage_->loadCatalog();   break;
    }
}

void EventMgrWidget::updateShieldStatus() {
    int count = bridge_->shieldCount();
    shieldLabel_->setText(QString("  当前屏蔽: %1 个报警").arg(count));
}
