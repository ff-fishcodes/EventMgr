#include "mainwindow.h"
#include "backend_bridge.h"
#include "event_list_widget.h"
#include "alarm_catalog_widget.h"
#include <QTabWidget>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), bridge_(nullptr) {
    // 初始化后端
    bridge_ = new BackendBridge(this);
    bridge_->initialize();

    setupUI();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    setWindowTitle("事件管理中心");
    resize(900, 600);

    // Tab 页面
    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);

    eventListPage_ = new EventListWidget(bridge_, this);
    tabs_->addTab(eventListPage_, "事件列表");

    catalogPage_ = new AlarmCatalogWidget(bridge_, this);
    tabs_->addTab(catalogPage_, "报警配置");

    // 状态栏
    statusLabel_ = new QLabel();
    statusBar()->addPermanentWidget(statusLabel_);

    statusTimer_ = new QTimer(this);
    connect(statusTimer_, SIGNAL(timeout()), this, SLOT(updateStatusBar()));
    statusTimer_->start(1000);
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    int shieldCount = bridge_->shieldCount();
    statusLabel_->setText(QString("当前屏蔽: %1 个报警").arg(shieldCount));
}
