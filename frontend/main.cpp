#include <QApplication>
#include "mainwindow.h"

// ============================================================
// 事件管理中心 — 前端 Qt 入口
// 平台: 飞腾 FT/2000 + 银河麒麟 V10, Qt 5.15.2, C++11
// ============================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EventMgr");

    MainWindow window;
    window.show();

    return app.exec();
}
