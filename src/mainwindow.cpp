#include "mainwindow.h"
#include "image_utils.h"
#include "crypto_utils.h"

#include <QInputDialog>
#include <QDir>
#include <QMessageBox>
#include <QStyle>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QStatusBar>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QToolTip>
#include <iostream>

// ImageViewerDialog Implementation
ImageViewerDialog::ImageViewerDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Image Viewer");
    setModal(true);
    resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);
    
    scene = new QGraphicsScene(this);
    graphicsView = new QGraphicsView(scene);
    graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    graphicsView->setBackgroundBrush(QBrush(QColor(45, 45, 45)));
    
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setRange(10, 200);
    zoomSlider->setValue(100);
    zoomSlider->setTickInterval(10);
    zoomSlider->setTickPosition(QSlider::TicksBelow);
    
    QHBoxLayout *controlLayout = new QHBoxLayout();
    QPushButton *fitButton = new QPushButton("Fit to Window");
    QPushButton *actualSizeButton = new QPushButton("Actual Size");
    
    controlLayout->addWidget(new QLabel("Zoom:"));
    controlLayout->addWidget(zoomSlider);
    controlLayout->addWidget(fitButton);
    controlLayout->addWidget(actualSizeButton);
    
    layout->addWidget(graphicsView);
    layout->addLayout(controlLayout);
    
    connect(zoomSlider, &QSlider::valueChanged, this, &ImageViewerDialog::onZoomChanged);
    connect(fitButton, &QPushButton::clicked, [this]() {
        graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
        currentZoom = 1.0;
        zoomSlider->setValue(100);
    });
    connect(actualSizeButton, &QPushButton::clicked, [this]() {
        graphicsView->resetTransform();
        currentZoom = 1.0;
        zoomSlider->setValue(100);
    });
}

void ImageViewerDialog::setImage(const QPixmap &pixmap) {
    scene->clear();
    scene->addPixmap(pixmap);
    scene->setSceneRect(pixmap.rect());
    graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void ImageViewerDialog::onZoomChanged(int value) {
    qreal factor = value / 100.0;
    QTransform transform;
    transform.scale(factor, factor);
    graphicsView->setTransform(transform);
    currentZoom = factor;
}

// ClickableLabel Implementation
ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip("Click to view full image");
}

void ClickableLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}

MainWindow::~MainWindow() {
    // Clean up resources if needed
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Password Encryption Tool");
    resize(600, 400);
    
    if (QGuiApplication::screens().size() > 0) {
        QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        setGeometry(
            QStyle::alignedRect(
                Qt::LeftToRight,
                Qt::AlignCenter,
                size(),
                screenGeometry
            )
        );
    }
    
    checkAccessPassword();
    setupUI();
    onRefreshFileList();
    
    statusBar()->showMessage("Ready", 3000);
}

void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    
    // Setup encrypt tab
    encryptTab = new QWidget();
    QVBoxLayout* encryptLayout = new QVBoxLayout(encryptTab);
    encryptLayout->setContentsMargins(20, 20, 20, 20);
    encryptLayout->setSpacing(15);
    
    QGroupBox* encryptGroupBox = new QGroupBox("Encrypt Password", encryptTab);
    encryptGroupBox->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* groupLayout = new QVBoxLayout(encryptGroupBox);
    groupLayout->setSpacing(12);
    groupLayout->setContentsMargins(15, 20, 15, 15);
    
    QLabel* passwordLabel = new QLabel("Password to encrypt:", encryptTab);
    passwordInput = new QLineEdit(encryptTab);
    passwordInput->setPlaceholderText("Type your password here");
    passwordInput->setMinimumHeight(30);
    
    encryptButton = new QPushButton("Encrypt", encryptTab);
    encryptButton->setMinimumHeight(36);
    encryptButton->setIcon(QIcon::fromTheme("document-encrypt", QIcon::fromTheme("lock")));
    
    groupLayout->addWidget(passwordLabel);
    groupLayout->addWidget(passwordInput);
    groupLayout->addWidget(encryptButton);
    
    encryptLayout->addWidget(encryptGroupBox);
    
    encryptPreviewGroup = new QGroupBox("Encryption Image Preview", encryptTab);
    encryptPreviewGroup->setVisible(false);
    encryptPreviewGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* encryptPreviewLayout = new QVBoxLayout(encryptPreviewGroup);
    encryptPreviewLayout->setContentsMargins(15, 20, 15, 15);
    
    QScrollArea* encryptScrollArea = new QScrollArea();
    encryptScrollArea->setWidgetResizable(true);
    encryptScrollArea->setFrameShape(QFrame::NoFrame);
    encryptScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    encryptScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    encryptImagePreview = new ClickableLabel(encryptTab);
    encryptImagePreview->setAlignment(Qt::AlignCenter);
    encryptImagePreview->setMinimumSize(IMAGE_PREVIEW_WIDTH, IMAGE_PREVIEW_HEIGHT);
    encryptImagePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Apply shadow effect
    QGraphicsDropShadowEffect* encryptShadow = new QGraphicsDropShadowEffect();
    encryptShadow->setBlurRadius(15);
    encryptShadow->setColor(QColor(0, 0, 0, 80));
    encryptShadow->setOffset(0, 0);
    encryptImagePreview->setGraphicsEffect(encryptShadow);
    
    encryptImagePreview->setStyleSheet("QLabel { background-color: #2d2d2d; border: 1px solid #444444; border-radius: 5px; padding: 8px; }");
    encryptImagePreview->setText("No image to display");
    
    encryptScrollArea->setWidget(encryptImagePreview);
    encryptPreviewLayout->addWidget(encryptScrollArea);
    
    encryptLayout->addWidget(encryptPreviewGroup, 1);
    
    // Setup decrypt tab
    decryptTab = new QWidget();
    QVBoxLayout* decryptLayout = new QVBoxLayout(decryptTab);
    decryptLayout->setContentsMargins(20, 20, 20, 20);
    decryptLayout->setSpacing(15);
    
    QLabel* filesLabel = new QLabel("Encrypted files:", decryptTab);
    filesLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    
    fileList = new QListWidget(decryptTab);
    fileList->setAlternatingRowColors(true);
    fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    fileList->setStyleSheet(fileList->styleSheet() + " QListWidget { border-radius: 5px; }");
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh", decryptTab);
    refreshButton->setMinimumHeight(36);
    refreshButton->setIcon(QIcon::fromTheme("view-refresh", QIcon::fromTheme("refresh")));
    
    deleteButton = new QPushButton("Delete", decryptTab);
    deleteButton->setMinimumHeight(36);
    deleteButton->setIcon(QIcon::fromTheme("edit-delete", QIcon::fromTheme("delete")));
    deleteButton->setStyleSheet("QPushButton { background-color: #d32f2f; }"
                              "QPushButton:hover { background-color: #b71c1c; }"
                              "QPushButton:pressed { background-color: #9a0007; }");
    
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    
    QFrame* resultFrame = new QFrame(decryptTab);
    resultFrame->setFrameShape(QFrame::StyledPanel);
    resultFrame->setFrameShadow(QFrame::Raised);
    resultFrame->setStyleSheet("QFrame { background-color: #3d3d3d; border-radius: 6px; padding: 12px; }");
    
    QVBoxLayout* resultLayout = new QVBoxLayout(resultFrame);
    resultLayout->setSpacing(10);
    QLabel* resultLabel = new QLabel("Decrypted password:", decryptTab);
    resultLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    
    decryptedPasswordLabel = new QLabel(decryptTab);
    decryptedPasswordLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    decryptedPasswordLabel->setStyleSheet("QLabel { font-family: 'Consolas', monospace; padding: 10px; background-color: #2d2d2d; color: #f0f0f0; border: 1px solid #444444; border-radius: 4px; font-size: 14px; }");
    decryptedPasswordLabel->setMinimumHeight(40);
    decryptedPasswordLabel->setAlignment(Qt::AlignCenter);
    
    resultLayout->addWidget(resultLabel);
    resultLayout->addWidget(decryptedPasswordLabel);
    
    decryptPreviewGroup = new QGroupBox("Decryption Image Preview", decryptTab);
    decryptPreviewGroup->setVisible(false);
    decryptPreviewGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* decryptPreviewLayout = new QVBoxLayout(decryptPreviewGroup);
    decryptPreviewLayout->setContentsMargins(15, 20, 15, 15);
    
    QScrollArea* decryptScrollArea = new QScrollArea();
    decryptScrollArea->setWidgetResizable(true);
    decryptScrollArea->setFrameShape(QFrame::NoFrame);
    decryptScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    decryptScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    decryptImagePreview = new ClickableLabel(decryptTab);
    decryptImagePreview->setAlignment(Qt::AlignCenter);
    decryptImagePreview->setMinimumSize(IMAGE_PREVIEW_WIDTH, IMAGE_PREVIEW_HEIGHT);
    decryptImagePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Apply shadow effect
    QGraphicsDropShadowEffect* decryptShadow = new QGraphicsDropShadowEffect();
    decryptShadow->setBlurRadius(15);
    decryptShadow->setColor(QColor(0, 0, 0, 80));
    decryptShadow->setOffset(0, 0);
    decryptImagePreview->setGraphicsEffect(decryptShadow);
    
    decryptImagePreview->setStyleSheet("QLabel { background-color: #2d2d2d; border: 1px solid #444444; border-radius: 5px; padding: 8px; }");
    decryptImagePreview->setText("No image to display");
    
    decryptScrollArea->setWidget(decryptImagePreview);
    decryptPreviewLayout->addWidget(decryptScrollArea);
    
    decryptLayout->addWidget(filesLabel);
    decryptLayout->addWidget(fileList, 1);
    decryptLayout->addLayout(buttonLayout);
    decryptLayout->addWidget(resultFrame);
    decryptLayout->addWidget(decryptPreviewGroup, 1);
    
    tabWidget->addTab(encryptTab, QIcon::fromTheme("document-encrypt", QIcon::fromTheme("lock")), "Encrypt");
    tabWidget->addTab(decryptTab, QIcon::fromTheme("document-decrypt", QIcon::fromTheme("unlock")), "Decrypt");
    
    mainLayout->addWidget(tabWidget);
    
    setCentralWidget(centralWidget);
    
    connect(encryptButton, &QPushButton::clicked, this, &MainWindow::onEncryptPassword);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshFileList);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteFile);
    connect(fileList, &QListWidget::itemClicked, this, &MainWindow::onFileSelected);
    connect(passwordInput, &QLineEdit::returnPressed, this, &MainWindow::onEncryptPassword);
    connect(encryptImagePreview, &ClickableLabel::clicked, this, &MainWindow::showEncryptedImageFullscreen);
    connect(decryptImagePreview, &ClickableLabel::clicked, this, &MainWindow::showDecryptedImageFullscreen);
}

void MainWindow::checkAccessPassword() {
    bool ok;
    QString password = QInputDialog::getText(this, "Access Required",
                                           "Enter program access password:",
                                           QLineEdit::Password, "", &ok);
    if (!ok || !verifyAccessPassword(password.toStdString())) {
        QMessageBox::critical(this, "Access Denied", "Invalid password. Application will exit.");
        exit(1);
    }
}

bool MainWindow::verifyAccessPassword(const std::string& password) {
    return password == "admin123"; // Example password, replace with your secure method
}

void MainWindow::onEncryptPassword() {
    QString password = passwordInput->text();
    if (password.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter a password to encrypt");
        return;
    }
    
    try {
        encryptPassword(password.toStdString());
        passwordInput->clear();
        QMessageBox::information(this, "Success", "Password encrypted successfully");
        statusBar()->showMessage("Password encrypted successfully", 3000);
        
        std::vector<std::string> files = listEncFiles();
        if (!files.empty()) {
            QString mostRecentFile = QString::fromStdString(files.back());
            displayEncryptedImage(mostRecentFile);
        }
        
        onRefreshFileList();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Error encrypting password: %1").arg(e.what()));
        statusBar()->showMessage("Encryption failed", 3000);
    }
}

void MainWindow::onDecryptPassword() {
    if (selectedFile.isEmpty()) {
        QMessageBox::warning(this, "Selection Error", "Please select a file to decrypt");
        return;
    }
    
    try {
        std::string decrypted = decryptPassword(selectedFile.toStdString());
        decryptedPasswordLabel->setText(QString::fromStdString(decrypted));
        statusBar()->showMessage("Password decrypted successfully", 3000);
        
        displayDecryptedImage(selectedFile);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Error decrypting password: %1").arg(e.what()));
        decryptedPasswordLabel->setText("Error decrypting");
        statusBar()->showMessage("Decryption failed", 3000);
        
        decryptPreviewGroup->setVisible(false);
    }
}

void MainWindow::onDeleteFile() {
    if (selectedFile.isEmpty()) {
        QMessageBox::warning(this, "Selection Error", "Please select a file to delete");
        return;
    }
    
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("Confirm Deletion");
    confirmBox.setText("Are you sure you want to delete this file?");
    confirmBox.setInformativeText("File: " + selectedFile + "\nThis action cannot be undone.");
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmBox.setDefaultButton(QMessageBox::No);
    confirmBox.setIcon(QMessageBox::Warning);
    
    if (confirmBox.exec() == QMessageBox::Yes) {
        QFile file(selectedFile);
        if (file.remove()) {
            statusBar()->showMessage("File deleted successfully", 3000);
            
            // Clear the preview and decrypted password
            decryptedPasswordLabel->clear();
            decryptPreviewGroup->setVisible(false);
            selectedFile.clear();
            
            // Refresh the file list
            onRefreshFileList();
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete the file");
            statusBar()->showMessage("Deletion failed", 3000);
        }
    }
}

void MainWindow::displayEncryptedImage(const QString& filePath) {
    currentEncryptPixmap = QPixmap();
    
    if (currentEncryptPixmap.load(filePath)) {
        QSize displaySize = QSize(IMAGE_PREVIEW_WIDTH, IMAGE_PREVIEW_HEIGHT);
        QSize imageSize = currentEncryptPixmap.size();
        
        // Only scale down if image is larger than display area
        if (imageSize.width() > displaySize.width() || imageSize.height() > displaySize.height()) {
            currentEncryptPixmap = currentEncryptPixmap.scaled(
                displaySize, 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
        }
        
        encryptImagePreview->setPixmap(currentEncryptPixmap);
        encryptImagePreview->setText("");
    } else {
        encryptImagePreview->setText("Encrypted data (not viewable as image)");
        encryptImagePreview->setPixmap(QPixmap());
    }
    
    encryptPreviewGroup->setVisible(true);
}

void MainWindow::displayDecryptedImage(const QString& filePath) {
    currentDecryptPixmap = QPixmap();
    
    if (currentDecryptPixmap.load(filePath)) {
        QSize displaySize = QSize(IMAGE_PREVIEW_WIDTH, IMAGE_PREVIEW_HEIGHT);
        QSize imageSize = currentDecryptPixmap.size();
        
        // Only scale down if image is larger than display area
        if (imageSize.width() > displaySize.width() || imageSize.height() > displaySize.height()) {
            currentDecryptPixmap = currentDecryptPixmap.scaled(
                displaySize, 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
        }
        
        decryptImagePreview->setPixmap(currentDecryptPixmap);
        decryptImagePreview->setText("");
    } else {
        decryptImagePreview->setText("Encrypted data (not viewable as image)");
        decryptImagePreview->setPixmap(QPixmap());
    }
    
    decryptPreviewGroup->setVisible(true);
}

void MainWindow::showEncryptedImageFullscreen() {
    if (!currentEncryptPixmap.isNull()) {
        ImageViewerDialog *viewer = new ImageViewerDialog(this);
        viewer->setImage(currentEncryptPixmap.scaled(currentEncryptPixmap.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
    }
}

void MainWindow::showDecryptedImageFullscreen() {
    if (!currentDecryptPixmap.isNull()) {
        ImageViewerDialog *viewer = new ImageViewerDialog(this);
        viewer->setImage(currentDecryptPixmap.scaled(currentDecryptPixmap.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
    }
}

void MainWindow::onRefreshFileList() {
    fileList->clear();
    decryptedPasswordLabel->clear();
    selectedFile.clear();
    
    decryptPreviewGroup->setVisible(false);
    
    std::vector<std::string> files = listEncFiles();
    for (const auto& file : files) {
        fileList->addItem(QString::fromStdString(file));
    }
    
    if (files.empty()) {
        statusBar()->showMessage("No encrypted files found", 3000);
    } else {
        statusBar()->showMessage(QString("Found %1 encrypted file(s)").arg(files.size()), 3000);
    }
}

void MainWindow::onFileSelected(QListWidgetItem* item) {
    if (!item) return;
    
    selectedFile = item->text();
    statusBar()->showMessage("Selected: " + selectedFile, 3000);
    onDecryptPassword();
}