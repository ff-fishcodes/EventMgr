#include <QApplication>
#include <QMainWindow>
#include "eventmgr_widget.h"

// ============================================================
// 事件管理中心 — 独立测试入口
// 实际集成时宿主项目直接 new EventMgrWidget(parent) 嵌入即可
// ============================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EventMgr");

    // 独立测试：用 QMainWindow 包裹 widget
    QMainWindow window;
    window.setWindowTitle("事件管理中心");
    window.resize(900, 600);

    EventMgrWidget* widget = new EventMgrWidget(&window);
    window.setCentralWidget(widget);

    window.show();
    return app.exec();
}
