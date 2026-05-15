#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("RuanKoDBMS Client");
    app.setStyle(QStyleFactory::create("Fusion"));

    app.setStyleSheet(R"(
        QWidget {
            color: #43566b;
        }

        QMainWindow {
            background: #f6f8fb;
        }

        QMenuBar {
            background: #ffffff;
            color: #43566b;
            border-bottom: 1px solid #d7e0e8;
        }

        QMenu {
            background: #ffffff;
            color: #43566b;
            border: 1px solid #d7e0e8;
        }

        QMenu::item:selected {
            background: #edf3f8;
            color: #314559;
        }

        QToolBar {
            background: #ffffff;
            border-bottom: 1px solid #d7e0e8;
            spacing: 6px;
            padding: 4px;
        }

        QStatusBar {
            background: #ffffff;
            color: #43566b;
            border-top: 1px solid #d7e0e8;
        }

        QTreeWidget, QTableWidget, QPlainTextEdit, QTextEdit {
            background: #ffffff;
            color: #43566b;
            selection-background-color: #8aa8c3;
            selection-color: #ffffff;
            border: 1px solid #d7e0e8;
            border-radius: 4px;
        }

        QTabWidget::pane {
            border: 1px solid #d7e0e8;
            background: #ffffff;
        }

        QTabBar::tab {
            background: #eef4f8;
            color: #43566b;
            border: 1px solid #d7e0e8;
            padding: 8px 16px;
            min-width: 100px;
        }

        QTabBar::tab:selected {
            background: #ffffff;
            color: #314559;
            border-bottom: 1px solid #ffffff;
        }

        QPushButton {
            background: #ffffff;
            color: #43566b;
            border: 1px solid #bfccd8;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background: #edf3f8;
        }

        QLabel#panelTitle {
            font-size: 14px;
            font-weight: 600;
            color: #43566b;
            padding: 2px 0 6px 0;
        }
    )");

    MainWindow window;
    window.show();

    return app.exec();
}
