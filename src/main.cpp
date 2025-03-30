#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include "mainwindow.h"
#include <ctime>
#include <cstdlib>

int main(int argc, char *argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    QApplication app(argc, argv);
    
    app.setStyle(QStyleFactory::create("Fusion"));
    
    QFont defaultFont("Segoe UI", 10);
    app.setFont(defaultFont);
    
    app.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #1e1e1e;
            color: #f0f0f0;
        }
        QTabWidget::pane {
            border: 1px solid #444444;
            border-radius: 4px;
            background-color: #2d2d2d;
        }
        QTabBar::tab {
            background-color: #2d2d2d;
            color: #f0f0f0;
            border: 1px solid #444444;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #3d3d3d;
        }
        QTabBar::tab:hover:!selected {
            background-color: #353535;
        }
        QPushButton {
            background-color: #0078d7;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #0063b1;
        }
        QPushButton:pressed {
            background-color: #004c8c;
        }
        QLineEdit {
            border: 1px solid #444444;
            border-radius: 4px;
            padding: 6px;
            background-color: #2d2d2d;
            color: #f0f0f0;
        }
        QLineEdit:focus {
            border: 1px solid #0078d7;
        }
        QListWidget {
            border: 1px solid #444444;
            border-radius: 4px;
            background-color: #2d2d2d;
            color: #f0f0f0;
            outline: none;
        }
        QListWidget::item {
            padding: 6px;
            border-bottom: 1px solid #3d3d3d;
        }
        QListWidget::item:selected {
            background-color: #0078d7;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background-color: #353535;
        }
        QGroupBox {
            border: 1px solid #444444;
            border-radius: 4px;
            margin-top: 1.5ex;
            color: #f0f0f0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top center;
            padding: 0 5px;
            color: #f0f0f0;
            font-weight: bold;
        }
        QLabel {
            color: #f0f0f0;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QScrollBar:horizontal {
            border: none;
            background: #2d2d2d;
            height: 10px;
            margin: 0px 10px 0px 10px;
        }
        QScrollBar:vertical {
            border: none;
            background: #2d2d2d;
            width: 10px;
            margin: 10px 0px 10px 0px;
        }
        QScrollBar::handle {
            background: #555555;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            min-width: 20px;
        }
        QScrollBar::handle:vertical {
            min-height: 20px;
        }
        QScrollBar::handle:hover {
            background: #777777;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            border: none;
            background: none;
        }
        QFrame {
            background-color: #2d2d2d;
            color: #f0f0f0;
        }
        QStatusBar {
            background-color: #2d2d2d;
            color: #f0f0f0;
            border-top: 1px solid #444444;
        }
        QGraphicsView {
            border: 1px solid #444444;
            border-radius: 4px;
            background-color: #2d2d2d;
        }
        QSlider::groove:horizontal {
            border: 1px solid #444444;
            height: 8px;
            background: #2d2d2d;
            margin: 2px 0;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background: #0078d7;
            border: none;
            width: 16px;
            margin: -4px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #0063b1;
        }
        QDialog {
            background-color: #1e1e1e;
            border: 1px solid #444444;
            border-radius: 4px;
        }
        QToolTip {
            border: 1px solid #444444;
            border-radius: 3px;
            background-color: #2d2d2d;
            color: #f0f0f0;
            padding: 4px;
        }
        ClickableLabel {
            transition: all 0.2s ease;
        }
        ClickableLabel:hover {
            border: 1px solid #0078d7 !important;
        }
        
        /* Image Viewer Dialog specific styles */
        ImageViewerDialog QPushButton {
            padding: 6px 12px;
            min-width: 100px;
        }
        ImageViewerDialog QSlider {
            min-width: 200px;
        }
    )");
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}