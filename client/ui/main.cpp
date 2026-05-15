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
            color: #778ea7;
        }

        QMainWindow {
            background: #f6f8fb;
        }

        QMenuBar {
            background: #ffffff;
            color: #778ea7;
            border-bottom: 1px solid #d7e0e8;
        }

        QMenu {
            background: #ffffff;
            color: #778ea7;
            border: 1px solid #d7e0e8;
        }

        QMenu::item:selected {
            background: #edf3f8;
            color: #778ea7;
        }

        QDialog {
            background: #20252b;
            color: #dbe7f2;
        }

        QDialog QLabel {
            color: #dbe7f2;
        }

        QDialog QLineEdit, QDialog QSpinBox {
            background: #2f363d;
            color: #eef6fb;
            selection-background-color: #6f8fa9;
            selection-color: #ffffff;
            border: 1px solid #4b5864;
            border-radius: 4px;
            padding: 5px 6px;
        }

        QDialog QLineEdit:focus, QDialog QSpinBox:focus {
            border: 1px solid #8fb0ca;
        }

        QDialog QPushButton {
            background: #f4f8fb;
            color: #40556b;
            border: 1px solid #aebfce;
            border-radius: 4px;
            padding: 6px 16px;
        }

        QDialog QPushButton:hover {
            background: #dce8f1;
            color: #263f58;
        }

        QToolBar {
            background: #ffffff;
            border-bottom: 1px solid #d7e0e8;
            spacing: 6px;
            padding: 4px;
        }

        QStatusBar {
            background: #ffffff;
            color: #778ea7;
            border-top: 1px solid #d7e0e8;
        }

        QTreeWidget, QTableWidget, QPlainTextEdit, QTextEdit {
            background: #ffffff;
            color: #43566b;
            selection-background-color: #c7dbea;
            selection-color: #263f58;
            border: 1px solid #c8d7e4;
            border-radius: 4px;
            gridline-color: #c8d7e4;
            alternate-background-color: #f3f7fa;
        }

        QTableWidget::item:selected, QTreeWidget::item:selected {
            background: #c7dbea;
            color: #263f58;
        }

        QHeaderView::section {
            background: #dce8f1;
            color: #314559;
            border: 0;
            border-right: 1px solid #b8cadd;
            border-bottom: 1px solid #b8cadd;
            padding: 5px 8px;
            font-weight: 600;
        }

        QHeaderView::section:vertical {
            background: #e6eef5;
            color: #4f6680;
            padding: 5px 6px;
            font-weight: 500;
        }

        QTableWidget QHeaderView {
            background: #e6eef5;
        }

        QTableCornerButton::section {
            background: #dce8f1;
            border: 0;
            border-right: 1px solid #b8cadd;
            border-bottom: 1px solid #b8cadd;
        }

        QTabWidget::pane {
            border: 1px solid #d7e0e8;
            background: #ffffff;
        }

        QTabBar::tab {
            background: #e4edf4;
            color: #778ea7;
            border: 1px solid #c8d7e4;
            padding: 8px 16px;
            min-width: 100px;
        }

        QTabBar::tab:selected {
            background: #ffffff;
            color: #778ea7;
            border-bottom: 1px solid #ffffff;
        }

        QPushButton {
            background: #ffffff;
            color: #778ea7;
            border: 1px solid #bfccd8;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background: #e2edf5;
        }

        QLabel#panelTitle {
            font-size: 14px;
            font-weight: 600;
            color: #778ea7;
            padding: 2px 0 6px 0;
        }
    )");

    MainWindow window;
    window.show();

    return app.exec();
}
