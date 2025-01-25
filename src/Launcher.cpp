#include <windows.h>
#include <filesystem>
#include <QApplication>
#include <QMediaPlayer>
#include <QGraphicsBlurEffect>
#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QBoxLayout>
#include <QLabel>
#include <QVideoFrame>
#include <QVideoSink>
#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QLineEdit>
#include "log/Logger.h"
#include "launcher/PipeHelper.h"
#include "qt/DraggableLabel.h"
#include "n3n/N3NHelper.h"

#define WIDTH 1152
#define HEIGHT 648

// get path we execute from
static TCHAR szUnquotedPath[MAX_PATH];
static const bool foundPath = GetModuleFileName( nullptr, szUnquotedPath, MAX_PATH );
static std::filesystem::path fullpath(szUnquotedPath);
static const auto folder = fullpath.remove_filename().string();

static LOGGER logger(folder);

// SETTINGS
// GROUP: N3N
bool useN3NManager = true;
std::string network = "main";

// GROUP: EVOLVE
std::string evolveLaunchArgs;
std::string pathToBin64Folder;
// end SETTINGS

// CONSOLE ARG OVERRIDES
// GROUP: N3N
bool overrideN3NManager = false;
bool overrideNetwork = false;
std::string overriddenNetwork;

// GROUP: EVOLVE
std::string consoleEvolveLaunchArgs;
bool overridePathToBin64Folder = false;
std::string overriddenPathToBin64Folder;
// end CONSOLE ARG OVERRIDES

bool n3nManagerConnected = false;

bool gameFound = false;
std::filesystem::path gameFolder;

enum UIState {
    DEFAULT,
    SETTINGS
};

UIState uiState = DEFAULT;

QFont ui("Segoe UI", 13);
QFont boldUI("Segoe UI", 13, QFont::Bold);

void readSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "EvolveReunited", "EvolveLauncher");
    settings.beginGroup("N3N");

    auto const s_useN3NManager = settings.value("useN3NManager", true);
    if (s_useN3NManager.isNull()) {
        useN3NManager = true;
    } else {
        useN3NManager = s_useN3NManager.toBool();
    }

    auto const s_network = settings.value("network", "main").toString();
    if (s_network.isNull() || s_network.isEmpty()) {
        network = "main";
    } else {
        network = s_network.toStdString();
    }

    settings.endGroup();
    settings.beginGroup("EVOLVE");

    auto const s_evolveLaunchArgs = settings.value("evolveLaunchArgs", "").toString();
    if (s_evolveLaunchArgs.isNull() || s_evolveLaunchArgs.isEmpty()) {
        evolveLaunchArgs = "";
    } else {
        evolveLaunchArgs = s_evolveLaunchArgs.toStdString();
    }

    auto const s_pathToBin64Folder = settings.value("pathToBin64Folder", "").toString();
    if (s_pathToBin64Folder.isNull() || s_pathToBin64Folder.isEmpty()) {
        pathToBin64Folder = folder;
    } else {
        pathToBin64Folder = s_pathToBin64Folder.toStdString();
    }

    settings.endGroup();
}

void readArgv(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-manager") == 0 || std::strcmp(argv[i], "-M") == 0) {
            overrideN3NManager = true;
            logger.info("Not using manager because of argv");
        } else if (std::strcmp(argv[i], "--network") == 0 || std::strcmp(argv[i], "-N") == 0) {
            i++;
            overrideNetwork = true;
            overriddenNetwork = argv[i];
            logger.info("Using custom network because of argv");
        } else if (std::strcmp(argv[i], "--bin64") == 0 || std::strcmp(argv[i], "-B") == 0) {
            i++;
            overridePathToBin64Folder = true;
            overriddenPathToBin64Folder = argv[i];
            logger.info("Using custom bin64 path because of argv");
        } else {
            consoleEvolveLaunchArgs.append(" ").append(argv[i]);
        }
    }
}

void writeSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "EvolveReunited", "EvolveLauncher");
    settings.beginGroup("N3N");
    settings.setValue("useN3NManager", useN3NManager);
    settings.setValue("network", network.c_str());
    settings.endGroup();
    settings.beginGroup("EVOLVE");
    settings.setValue("evolveLaunchArgs", evolveLaunchArgs.c_str());
    settings.setValue("pathToBin64Folder", pathToBin64Folder.c_str());
    settings.endGroup();
    settings.sync();
}

void checkGameFolder() {
    gameFolder = std::filesystem::path((overridePathToBin64Folder ? overriddenPathToBin64Folder : pathToBin64Folder));
    gameFound = false;

    if (is_directory(gameFolder)) {
        if((gameFolder.filename().string() == "bin64_SteamRetail" || gameFolder.filename().string() == "Bin64_SteamRetail") &&
            exists(std::filesystem::path(gameFolder).append("CryD3DCompilerStub.dll"))) {
            // We should be running in the actual evolve install now.
            // If we aren't the user just has to change it in the settings I guess
            gameFound = true;
        } else if (exists(std::filesystem::path(gameFolder).append("bin64_SteamRetail")) &&
                    exists(std::filesystem::path(gameFolder).append("bin64_SteamRetail").append("CryD3DCompilerStub.dll"))) {
            gameFolder.append("bin64_SteamRetail");
            gameFound = true;
        } else if (exists(std::filesystem::path(gameFolder).append("Bin64_SteamRetail")) &&
                    exists(std::filesystem::path(gameFolder).append("Bin64_SteamRetail").append("CryD3DCompilerStub.dll"))) {
            gameFolder.append("Bin64_SteamRetail");
            gameFound = true;
        }
    }
}

const char* getShortenedBin64Path() {
    auto bin64Path = (overridePathToBin64Folder ? overriddenPathToBin64Folder : pathToBin64Folder);

    if (bin64Path.length() < 30) {
        return strdup(bin64Path.c_str());
    } else {
        std::string shortString = bin64Path.substr(0, 12) + "{...}" + bin64Path.substr(bin64Path.length() - 12, 12);
        return strdup(shortString.c_str());
    }
}

void deployLauncher() {
    if (!gameFound) return;

    // Copy over launcher, manager, resources and bin folder
    // We do NOT re-create the service for now as that would require prompting for admin perms and the N3NManager doesn't care from where it's running
    // If the user deletes the old N3NManager they can still just re-create the service from the settings
    // This does copy all files manually right now as it's the only way to ensure we only copy what is needed

    try {
        // Copy folders
        std::filesystem::copy("resources", std::filesystem::path(gameFolder).append("resources"), std::filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);
        std::filesystem::copy("bin", std::filesystem::path(gameFolder).append("bin"), std::filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);
        std::filesystem::copy("patch", std::filesystem::path(gameFolder).append("patch"), std::filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);

        // Copy files:
        std::filesystem::copy("console_usage.txt", std::filesystem::path(gameFolder).append("console_usage.txt"), std::filesystem::copy_options::update_existing);
        std::filesystem::copy("EvolveLauncher.exe", std::filesystem::path(gameFolder).append("EvolveLauncher.exe"), std::filesystem::copy_options::update_existing);
        std::filesystem::copy("EvolveN3NManager.exe", std::filesystem::path(gameFolder).append("EvolveN3NManager.exe"), std::filesystem::copy_options::update_existing);

        // Apply the patch
        std::filesystem::copy(std::filesystem::path(fullpath).append("patch/bin64_SteamRetail"), gameFolder, std::filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);
        std::filesystem::copy(std::filesystem::path(fullpath).append("patch/ca-bundle.crt"), gameFolder.parent_path().append("ca-bundle.crt"), std::filesystem::copy_options::update_existing);
    } catch (std::filesystem::filesystem_error &error) {
        logger.error("Failed to copy files");
        logger.error(error.path1().string());
        logger.error(error.path2().string());
        logger.error(error.what());
        logger.error(error.code().message());
    }
}

HANDLE n3nThread = nullptr;
// launchButton is in scope from here on as we need to access it below...
// Doesn't make the code cleaner but at least we don't block the main thread with waiting this way
QPushButton* launchButton;

DWORD WINAPI manuallyConnectToN3NAndRunEvolve(void* ignored) {
    if (!N3N::connectWithoutElevation((overrideNetwork ? overriddenNetwork : network))) {
        launchButton->setEnabled(true);
        return 0;
    }

    // TODO: Launch Evolve

    launchButton->setEnabled(true);
    return 0;
}

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    readSettings();
    readArgv(argc, argv);

    logger.info("Launcher starting...");

    if (useN3NManager && !overrideN3NManager) {
        if (PIPE::init()) {
            n3nManagerConnected = true;
        } else {
            logger.warn("Failed to connect to N3NManager");
            // Since we can't connect to the manager we just fallback to requesting admin perms ourselves
            n3nManagerConnected = false;
        }
    }

    checkGameFolder();

    QWidget win;
    win.setFixedSize(WIDTH, HEIGHT);

    // Background Video Setup, STEP: MediaPlayer
    auto player = new QMediaPlayer();
    player->setSource(QUrl::fromLocalFile("resources/BackgroundVideo.mp4"));
    player->setLoops(QMediaPlayer::Infinite);
    player->play();

    // Background Video Setup, STEP: Rendering
    auto scene = new QGraphicsScene();
    auto graphicsView = new QGraphicsView(scene);
    graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setStyleSheet("background-color: black;");
    graphicsView->setFixedSize(WIDTH, HEIGHT);

    auto videoItem = new QGraphicsVideoItem;
    videoItem->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    scene->addItem(videoItem);

    player->setVideoOutput(videoItem);

    // Background Video Setup, STEP: Resizing the Video
    auto layer1 = new QVBoxLayout(&win);
    layer1->addWidget(graphicsView);
    layer1->setGeometry(QRect(0, 0, WIDTH, HEIGHT));
    layer1->setContentsMargins(0, 0, 0, 0);

    auto videoSink = videoItem->videoSink();

    // Background Video Setup, STEP: Blurring
    auto *blurLabel = new QLabel(&win);
    blurLabel->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
    blurLabel->setStyleSheet("background: transparent;");

    // Connect the video sink's frameUpdated signal to apply blur
    // This is basically just a lambda that runs every frame, crops out the region we need to blur, applies the effect and renders it back onto the screen
    QObject::connect(videoSink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame &frame) {
        if (!frame.isValid()) return;

        QVideoFrame mappedFrame = frame;
        if (!mappedFrame.map(QVideoFrame::ReadOnly)) return;

        QImage image = mappedFrame.toImage();
        if (image.isNull()) {
            mappedFrame.unmap();
            return;
        }

        // Crop the bottom 1/4
        QRect cropRect;

        if (uiState == DEFAULT) {
            cropRect = QRect(0, image.height() - image.height() / 4, image.width(), image.height() / 4);
        } else if (uiState == SETTINGS) {
            cropRect = QRect(0, 0, image.width(), image.height());
        }

        QImage croppedImage = image.copy(cropRect);

        // Apply a blur effect
        QGraphicsScene blurScene;
        QGraphicsPixmapItem *pixmapItem = blurScene.addPixmap(QPixmap::fromImage(croppedImage));
        auto *blurEffect = new QGraphicsBlurEffect;
        blurEffect->setBlurRadius(15); // Adjust blur radius
        pixmapItem->setGraphicsEffect(blurEffect);

        // Render the blurred result to a pixmap
        QPixmap blurredPixmap(croppedImage.size());
        blurredPixmap.fill(Qt::transparent);
        QPainter painter(&blurredPixmap);
        blurScene.render(&painter);
        painter.end();

        QPixmap scaledBlurredPixmap = blurredPixmap.scaled(
                blurLabel->size(),   // Match the size of the QLabel
                Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation
        );

        // Display the blurred pixmap in the QLabel
        blurLabel->setPixmap(scaledBlurredPixmap);
        blurLabel->setVisible(true);
        mappedFrame.unmap();
    });

    auto bottomOverlay = new QWidget(&win);
    bottomOverlay->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
    bottomOverlay->setAutoFillBackground(false);
    bottomOverlay->setStyleSheet("background-color: rgba(20,20,20,80);");

    // Setup UI
    launchButton = new QPushButton(bottomOverlay);
    launchButton->setIcon(QIcon("resources/LaunchButton.png"));
    launchButton->setIconSize(QSize(280, 81));
    launchButton->setGeometry(WIDTH - WIDTH / 8.0 * 2.2, HEIGHT / 16, 300, 81);
    launchButton->setStyleSheet(R"(
        QPushButton {
            border: 2px solid #b5291d;
            background-color: rgba(40,40,40,90);
        }
        QPushButton:hover {
            background-color: rgba(0,0,0,140);
        }
    )");
    launchButton->setCursor(Qt::PointingHandCursor);
    launchButton->setEnabled(gameFound);

    QObject::connect(launchButton, &QPushButton::released, [&]() {
        if (useN3NManager && n3nManagerConnected) {

        } else {
            launchButton->setEnabled(false);
            // We should never have to wait here, just do it in case we somehow still have to
            if (n3nThread != nullptr) {
                WaitForSingleObject(n3nThread, INFINITE);
            }
            n3nThread = CreateThread(nullptr, 0, manuallyConnectToN3NAndRunEvolve, nullptr, 0, nullptr);
        }
    });

    auto setGameFolderHint = new QLabel(bottomOverlay);
    setGameFolderHint->setText("SET GAME LOCATION IN SETTINGS");
    setGameFolderHint->setGeometry(WIDTH - WIDTH / 8.0 * 2.2, HEIGHT / 16 + 61, 300, 40);
    setGameFolderHint->setStyleSheet(R"(
            QLabel {
                border: 2px solid #b5291d;
                border-top: none;
                background-color: rgba(20,20,20,140);
            }
        )");
    setGameFolderHint->setFont(boldUI);
    setGameFolderHint->setAlignment(Qt::AlignCenter);
    setGameFolderHint->setVisible(!gameFound);

    if (!gameFound) {
        launchButton->move(WIDTH - WIDTH / 8.0 * 2.2, HEIGHT / 16 - 20);
    }

    auto settingsOverlay = new QWidget(&win);
    settingsOverlay->setGeometry(0, 00, WIDTH, HEIGHT);
    settingsOverlay->setAutoFillBackground(false);
    settingsOverlay->setVisible(false);
    settingsOverlay->setObjectName("SettingsOverlay");
    settingsOverlay->setStyleSheet(R"(
        #SettingsOverlay {
            background-color: rgba(20,20,20,80);
        }
    )");

    auto manageN3NManagerLabel = new QLabel(settingsOverlay);
    manageN3NManagerLabel->setText("EvolveN3NManager Service: ");
    manageN3NManagerLabel->setGeometry(40, 80, 400, 40);
    manageN3NManagerLabel->setFont(boldUI);
    auto manageN3NManagerButton = new QPushButton(settingsOverlay);
    manageN3NManagerButton->setGeometry(440, 80, 200, 40);
    manageN3NManagerButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");
    manageN3NManagerButton->setCursor(Qt::PointingHandCursor);
    manageN3NManagerButton->setText((useN3NManager && n3nManagerConnected ? "Remove Service" : "Install Service"));
    manageN3NManagerButton->setEnabled(useN3NManager && !overrideN3NManager);
    manageN3NManagerButton->setFont(boldUI);

    QObject::connect(manageN3NManagerButton, &QPushButton::released, [&]() {
        SHELLEXECUTEINFOA shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = nullptr;
        shExInfo.lpVerb = "runas";
        shExInfo.lpFile = (folder + "EvolveN3NManager.exe").c_str();
        shExInfo.lpDirectory = nullptr;
        shExInfo.nShow = SW_SHOW;
        shExInfo.hInstApp = nullptr;

        if (useN3NManager && n3nManagerConnected) {
            shExInfo.lpParameters = "--remove";
            if (ShellExecuteExA(&shExInfo))
            {
                WaitForSingleObject(shExInfo.hProcess, INFINITE);
                CloseHandle(shExInfo.hProcess);
            } else {
                logger.error("Failed to install service: " + std::to_string(GetLastError()));
                return;
            }
        } else {
            shExInfo.lpParameters = "--install";
            if (ShellExecuteExA(&shExInfo))
            {
                WaitForSingleObject(shExInfo.hProcess, INFINITE);
                CloseHandle(shExInfo.hProcess);
            } else {
                logger.error("Failed to remove service: " + std::to_string(GetLastError()));
                return;
            }
        }

        if (PIPE::init()) {
            n3nManagerConnected = true;
        } else {
            logger.warn("Failed to connect to N3NManager");
            // Since we can't connect to the manager we just fallback to requesting admin perms ourselves
            n3nManagerConnected = false;
        }

        manageN3NManagerButton->setText((useN3NManager && n3nManagerConnected ? "Remove Service" : "Install Service"));
    });

    auto bin64FolderSelectWarningIcon = new QLabel(settingsOverlay);
    bin64FolderSelectWarningIcon->setGeometry(20, 140, 20, 40);
    bin64FolderSelectWarningIcon->setPixmap(QIcon("resources/WarningIcon.svg").pixmap(QSize(20, 20)));
    bin64FolderSelectWarningIcon->setVisible(!gameFound);
    auto bin64FolderSelectLabel = new QLabel(settingsOverlay);
    bin64FolderSelectLabel->setText((std::string("Bin64 Location: (") + getShortenedBin64Path() + ")").c_str());
    bin64FolderSelectLabel->setGeometry(40, 140, 400, 40);
    bin64FolderSelectLabel->setFont(boldUI);
    auto bin64FolderSelectButton = new QPushButton(settingsOverlay);
    bin64FolderSelectButton->setGeometry(440, 140, 200, 40);
    bin64FolderSelectButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");
    bin64FolderSelectButton->setCursor(Qt::PointingHandCursor);
    bin64FolderSelectButton->setText("Select Folder");
    bin64FolderSelectButton->setFont(boldUI);
    bin64FolderSelectButton->setEnabled(!overridePathToBin64Folder);

    QObject::connect(bin64FolderSelectButton, &QPushButton::released, [&]() {
        pathToBin64Folder = QFileDialog::getExistingDirectory(&win, "Select your bin64_SteamRetail folder", pathToBin64Folder.c_str()).toStdString();

        checkGameFolder();

        bin64FolderSelectWarningIcon->setVisible(!gameFound);
        bin64FolderSelectLabel->setText((std::string("Bin64 Location: (") + getShortenedBin64Path() + ")").c_str());

        if (gameFound) {
            setGameFolderHint->setVisible(false);
            launchButton->move(WIDTH - WIDTH / 8.0 * 2.2, HEIGHT / 16);
        } else {
            setGameFolderHint->setVisible(true);
            launchButton->move(WIDTH - WIDTH / 8.0 * 2.2, HEIGHT / 16 - 20);
        }

        launchButton->setEnabled(gameFound);
        bin64FolderSelectWarningIcon->setVisible(!gameFound);

        deployLauncher();
    });

    auto evolveLaunchArgsLabel = new QLabel(settingsOverlay);
    evolveLaunchArgsLabel->setText("Evolve Launch Arguments:");
    evolveLaunchArgsLabel->setGeometry(40, 200, 400, 40);
    evolveLaunchArgsLabel->setFont(boldUI);

    auto evolveLaunchArgsEditor = new QLineEdit(settingsOverlay);
    evolveLaunchArgsEditor->setGeometry(40, 240, 600, 40);
    evolveLaunchArgsEditor->setPlaceholderText("Launch Args");
    evolveLaunchArgsEditor->setFont(ui);
    evolveLaunchArgsEditor->setText(evolveLaunchArgs.c_str());

    QObject::connect(evolveLaunchArgsEditor, &QLineEdit::editingFinished, [&]() {
        evolveLaunchArgs = evolveLaunchArgsEditor->text().toStdString();
    });

    auto creditsText = new QLabel(settingsOverlay);
    creditsText->setGeometry(680, 40, WIDTH - 720, HEIGHT - 80);
    creditsText->setFont(boldUI);
    creditsText->setWordWrap(true);
    creditsText->setTextFormat(Qt::RichText);
    creditsText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    creditsText->setOpenExternalLinks(true);
    // The formatting here is quite bad, but has to be like this to avoid spacing issues
    creditsText->setText(R"(Huge thanks to the creators of the original emulator:<br>
Nemirtingas, schmogmog, Nemerod, kiagam, Pinenut, pikapika<br>
<br>
Without your work this wouldn't have been possible!<br>
<br>
Special thanks to:<br>
Pinenut, for completely rewriting the ricefix and most of the emulator<br>
<a style="color: #db3425;" href="https://www.youtube.com/@macabrevoid">Macabre Void</a>, for the awesome background video<br>
Nemerod, for all the effort he put into this game<br>
The entire Evolve Reunited 2.0 team<br>
All the people in the discord that constantly help with answering questions and other issues<br>
You, for not letting Evolve die<br>
<br>
This launcher was written by DeinAlbtraum<br>
<a style="color: #db3425;" href="https://github.com/PaienNate/EvolveLegacyRebornLauncher">Source</a>, <a style="color: #db3425;" href="http://discord.evolvereunited.org">Discord</a>)");

    auto closeButton = new QPushButton(&win);
    closeButton->setGeometry(WIDTH - 40, 0, 40, 40);
    closeButton->setIcon(QIcon("resources/CloseButton.svg"));
    closeButton->setIconSize(QSize(18, 18));
    closeButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            background-color: transparent;
            width: 32px;
            height: 32px;
        }
        QPushButton:hover {
            background-color: rgba(80,80,80,140);
        }
    )");
    closeButton->setCursor(Qt::PointingHandCursor);

    QObject::connect(closeButton, &QPushButton::released, [&](){
        win.close();
    });

    auto minimizeButton = new QPushButton(&win);
    minimizeButton->setGeometry(WIDTH - 2 * 40, 0, 40, 40);
    minimizeButton->setIcon(QIcon("resources/MinimizeButton.svg"));
    minimizeButton->setIconSize(QSize(18, 18));
    minimizeButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            background-color: transparent;
            width: 32px;
            height: 32px;
        }
        QPushButton:hover {
            background-color: rgba(80,80,80,140);
        }
    )");
    minimizeButton->setCursor(Qt::PointingHandCursor);

    QObject::connect(minimizeButton, &QPushButton::released, [&](){
        win.showMinimized();
    });

    auto settingsButton = new QPushButton(&win);
    settingsButton->setGeometry(WIDTH - 3 * 40, 0, 40, 40);
    settingsButton->setIcon(QIcon("resources/SettingsButton.svg"));
    settingsButton->setIconSize(QSize(18, 18));
    settingsButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            background-color: transparent;
            width: 32px;
            height: 32px;
        }
        QPushButton:hover {
            background-color: rgba(80,80,80,140);
        }
    )");
    settingsButton->setCursor(Qt::PointingHandCursor);

    QObject::connect(settingsButton, &QPushButton::released, [&]() {
        if (uiState == DEFAULT) {
            uiState = SETTINGS;
            blurLabel->setGeometry(0, 0, WIDTH, HEIGHT);
            blurLabel->setVisible(false);
            bottomOverlay->setVisible(false);
            settingsOverlay->setVisible(true);
        } else {
            uiState = DEFAULT;
            blurLabel->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
            blurLabel->setVisible(false);
            bottomOverlay->setVisible(true);
            settingsOverlay->setVisible(false);
        }
    });

    // Setup Dragging
    auto draggingAnchor = new DraggableLabel(&win);
    draggingAnchor->setGeometry(0, 0, WIDTH - 3 * 40, 40);

    // Display the window
    win.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    win.show();

    // Has to be done here, make sure the videoItem gets resized once to fill the screen
    QRectF viewRect = graphicsView->viewport()->rect();
    videoItem->setSize(viewRect.size());

    // UI Loop
    int rc = a.exec();

    // Cleanup
    if (useN3NManager && n3nManagerConnected) {
        unsigned long bytesWritten;
        PIPE::sendMessage("disconnect", &bytesWritten);
    } else {
        N3N::shutdownShellExecute();
    }

    writeSettings();

    PIPE::destroy();

    logger.info("Bye bye");
    return rc;
}