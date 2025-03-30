#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QIcon>
#include <QFrame>
#include <QScrollArea>
#include <QStatusBar>
#include <QGroupBox>
#include <QPixmap>
#include <QSize>
#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QSlider>

// Enhanced Image Viewer Dialog
class ImageViewerDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ImageViewerDialog(QWidget *parent = nullptr);
    void setImage(const QPixmap &pixmap);

private:
    QGraphicsView *graphicsView;
    QGraphicsScene *scene;
    QSlider *zoomSlider;
    qreal currentZoom = 1.0;

private slots:
    void onZoomChanged(int value);
};

// Custom label to capture mouse clicks
class ClickableLabel : public QLabel {
    Q_OBJECT
    
public:
    explicit ClickableLabel(QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onEncryptPassword();
    void onDecryptPassword();
    void onRefreshFileList();
    void onFileSelected(QListWidgetItem* item);
    void showEncryptedImageFullscreen();
    void showDecryptedImageFullscreen();
    void onDeleteFile();

private:
    void setupUI();
    void checkAccessPassword();
    bool verifyAccessPassword(const std::string& password);
    void displayEncryptedImage(const QString& filePath);
    void displayDecryptedImage(const QString& filePath);
    void setupImagePreview();
    
    // UI Elements
    QTabWidget* tabWidget;
    
    // Encrypt tab
    QWidget* encryptTab;
    QLineEdit* passwordInput;
    QPushButton* encryptButton;
    ClickableLabel* encryptImagePreview;
    QGroupBox* encryptPreviewGroup;
    QPixmap currentEncryptPixmap;
    
    // Decrypt tab
    QWidget* decryptTab;
    QListWidget* fileList;
    QPushButton* refreshButton;
    QPushButton* deleteButton;  // New delete button
    QLabel* decryptedPasswordLabel;
    QFrame* resultFrame;
    ClickableLabel* decryptImagePreview;
    QGroupBox* decryptPreviewGroup;
    QPixmap currentDecryptPixmap;
    
    // Currently selected file
    QString selectedFile;
    
    // Constants for image preview
    const int IMAGE_PREVIEW_WIDTH = 350;
    const int IMAGE_PREVIEW_HEIGHT = 220;
};