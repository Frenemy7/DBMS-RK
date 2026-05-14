#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("RuanKoDBMS Client");
    app.setStyle(QStyleFactory::create("Fusion"));

    app.setStyleSheet(R"(
        QMainWindow {
            background: #f5f7fb;
        }

        QMenuBar {
            background: #ffffff;
            border-bottom: 1px solid #d9dee7;
        }

        QToolBar {
            background: #ffffff;
            border-bottom: 1px solid #d9dee7;
            spacing: 6px;
            padding: 4px;
        }

        QStatusBar {
            background: #ffffff;
            border-top: 1px solid #d9dee7;
        }

        QTreeWidget, QTableWidget, QPlainTextEdit, QTextEdit {
            background: #ffffff;
            border: 1px solid #d9dee7;
            border-radius: 4px;
        }

        QTabWidget::pane {
            border: 1px solid #d9dee7;
            background: #ffffff;
        }

        QTabBar::tab {
            background: #eef2f7;
            border: 1px solid #d9dee7;
            padding: 8px 16px;
            min-width: 100px;
        }

        QTabBar::tab:selected {
            background: #ffffff;
            border-bottom: 1px solid #ffffff;
        }

        QPushButton {
            background: #ffffff;
            border: 1px solid #c8d0db;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background: #f2f6fc;
        }

        QLabel#panelTitle {
            font-size: 14px;
            font-weight: 600;
            color: #243447;
            padding: 2px 0 6px 0;
        }
    )");

    MainWindow window;
    window.show();

    return app.exec();
}