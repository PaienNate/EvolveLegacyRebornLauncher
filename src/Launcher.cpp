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
#include <QCheckBox>
#include <QFileSystemWatcher>
#include <random>
#include <fstream>
#include <QProgressBar>
#include "log/Logger.h"
#include "launcher/PipeHelper.h"
#include "qt/DraggableLabel.h"
#include "n3n/N3NHelper.h"
#include "qt/FileDownloader.hpp"
#include "GameFiles.h"
#include "util/SaveFileHelper.h"
#include "qt/Updater.hpp"

#define WIDTH 1152
#define HEIGHT 648

#define FILESERVER_URL "https://evolve.a1btraum.de/base"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// get path we execute from
static TCHAR szUnquotedPath[MAX_PATH];
static const bool foundPath = GetModuleFileName( nullptr, szUnquotedPath, MAX_PATH );
static std::filesystem::path fullpath(szUnquotedPath);
static const auto folder = fullpath.remove_filename().string();

static LOGGER logger(folder);

// SETTINGS
// GROUP: GENERAL
bool firstRun = true;

// GROUP: N3N
bool useN3NManager = true;
bool useCustomNetwork = false;
std::string network = "main";

// GROUP: EVOLVE
std::string evolveLaunchArgs;
std::string pathToBin64Folder;
// end SETTINGS

// CONSOLE ARG OVERRIDES
// GROUP: GENERAL
bool doOnboarding = false;

// GROUP: N3N
bool overrideN3NManager = false;
bool overrideNetwork = false;
std::string overriddenNetwork;

// GROUP: EVOLVE
std::string consoleEvolveLaunchArgs;
bool overridePathToBin64Folder = false;
std::string overriddenPathToBin64Folder;

// CONSOLE ONLY
bool doUpdate = false;
// end CONSOLE ARG OVERRIDES

bool n3nManagerConnected = false;

bool gameFound = false;
std::filesystem::path gameFolder;

enum UIState {
    DEFAULT,
    SETTINGS,
    ONBOARDING_GAME_LOCATION,
    ONBOARDING_USERNAME,
    ONBOARDING_PROGRESS_IMPORT
};

UIState uiState = DEFAULT;

QFont ui("Segoe UI", 13);
QFont boldUI("Segoe UI", 13, QFont::Bold);

bool can_write(const std::filesystem::path& dir) {
    try {
        std::filesystem::path tmp_file = dir / "permtest.tmp";

        std::ofstream ofs(tmp_file);
        if (!ofs) return false; // Unable to open the file for writing

        ofs.close();
        std::filesystem::remove(tmp_file); // Clean up
        return true;
    } catch (std::filesystem::filesystem_error &error) {
        return false;
    }
}

void readSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "EvolveReunited", "EvolveLauncher");
    settings.beginGroup("GENERAL");

    auto const s_firstRun = settings.value("firstRun", true);
    if (s_firstRun.isNull()) {
        firstRun = true;
    } else {
        firstRun = s_firstRun.toBool();
    }

    settings.endGroup();
    settings.beginGroup("N3N");

    auto const s_useN3NManager = settings.value("useN3NManager", true);
    if (s_useN3NManager.isNull()) {
        useN3NManager = true;
    } else {
        useN3NManager = s_useN3NManager.toBool();
    }

    auto const s_useCustomNetwork = settings.value("useCustomNetwork", false);
    if (s_useCustomNetwork.isNull()) {
        useCustomNetwork = false;
    } else {
        useCustomNetwork = s_useCustomNetwork.toBool();
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
        if (std::strcmp(argv[i], "--update") == 0) {
            doUpdate = true;
            logger.info("Updating because of argv");
        } else if (std::strcmp(argv[i], "--do-onboarding") == 0 || std::strcmp(argv[i], "-O") == 0) {
            doOnboarding = true;
            logger.info("(Re-)running onboarding because of argv");
        } else if (std::strcmp(argv[i], "--no-manager") == 0 || std::strcmp(argv[i], "-M") == 0) {
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
    settings.beginGroup("GENERAL");
    settings.setValue("firstRun", firstRun);
    settings.endGroup();
    settings.beginGroup("N3N");
    settings.setValue("useN3NManager", useN3NManager);
    settings.setValue("useCustomNetwork", useCustomNetwork);
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

    if (!can_write(gameFolder)) return;

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
        return _strdup(bin64Path.c_str());
    } else {
        std::string shortString = bin64Path.substr(0, 12) + "{...}" + bin64Path.substr(bin64Path.length() - 12, 12);
        return _strdup(shortString.c_str());
    }
}

void deployLauncher() {
    if (!gameFound) return;

    // We are trying to copy into the folder we already run from? Not needed so we skip
    if (gameFolder.string() == folder) return;

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
        std::filesystem::copy("EvolveN3NManager.exe", std::filesystem::path(gameFolder).append("EvolveN3NManager.exe"), std::filesystem::copy_options::update_existing);

        // Apply the patch
        std::filesystem::copy(std::filesystem::path(fullpath).append("patch/bin64_SteamRetail"), gameFolder, std::filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);
        std::filesystem::copy(std::filesystem::path(fullpath).append("patch/ca-bundle.crt"), gameFolder.parent_path().append("ca-bundle.crt"), std::filesystem::copy_options::update_existing);

        // Copy launcher last so that if something goes wrong we don't have a partially working launcher
        std::filesystem::copy("EvolveLauncher.exe", std::filesystem::path(gameFolder).append("EvolveLauncher.exe"), std::filesystem::copy_options::update_existing);
    } catch (std::filesystem::filesystem_error &error) {
        logger.error("Failed to copy files");
        logger.error(error.path1().string());
        logger.error(error.path2().string());
        logger.error(error.what());
        logger.error(error.code().message());
    }
}

const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

const char steamidCharset[] = "0123456789";

std::default_random_engine rng(std::random_device{}());
//-2 to remove nullterm
std::uniform_int_distribution<> lobbyDist(0, ARRAY_SIZE(charset) - 2);
std::uniform_int_distribution<> steamidDist(0, ARRAY_SIZE(steamidCharset) - 2);

//yes, this actually does always return a string with a length of 10, who would've thought...
std::string generateRandomNetworkName() {
    auto randchar = []() -> char
    {
        return charset[ lobbyDist(rng)];
    };
    std::string str(10,0);
    std::generate_n( str.begin(), 10, randchar );
    return str;
}

std::string generateRandomSteamID64() {
    auto randchar = []() -> char
    {
        return steamidCharset[ steamidDist(rng)];
    };
    std::string str(17,0);
    std::generate_n( str.begin(), 17, randchar );
    return str;
}

HANDLE gameThread = nullptr;
// launchButton is in scope from here on as we need to access it below...
// Doesn't make the code cleaner but at least we don't block the main thread with waiting this way
QPushButton* launchButton;

bool gameRunning = false;

STARTUPINFOA si;
PROCESS_INFORMATION pi;

DWORD WINAPI runEvolve(void* ignored);

DWORD WINAPI manuallyConnectToN3NAndRunEvolve(void* ignored) {
    if (!N3N::connectWithoutElevation((overrideNetwork ? overriddenNetwork : network))) {
        launchButton->setEnabled(true);
        return 0;
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    runEvolve(ignored);
    return 0;
}

DWORD WINAPI runEvolve(void* ignored) {
    logger.info("Starting Evolve");

    CreateProcessA(
            nullptr,
            (LPSTR) (std::filesystem::path(gameFolder).append("Evolve.exe").string() + " --no-launcher " + evolveLaunchArgs + " " + consoleEvolveLaunchArgs).c_str(),
            nullptr,
            nullptr,
            FALSE,
            DETACHED_PROCESS,
            nullptr,
            nullptr,
            &si,
            &pi
    );

    gameRunning = true;

    //wait for game to close
    WaitForSingleObject(pi.hProcess, INFINITE);

    gameRunning = false;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    launchButton->setEnabled(true);
    return 0;
}

inline std::string joinArgvWithoutUpdate(char* arr[], int size) {
    std::ostringstream oss;
    for (int i = 0; i < size; ++i) {
        if (strcmp(arr[i], "--update") == 0) continue;

        if (i > 0) oss << " ";
        oss << arr[i];
    }
    return oss.str();
}

FileDownloader* downloader;

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    readSettings();
    readArgv(argc, argv);

    if (doUpdate) {
        checkGameFolder();
        deployLauncher();
        CreateProcessA(
                nullptr,
                (LPSTR) (std::filesystem::path(gameFolder).append("EvolveLauncher.exe").string() + " " +
                        joinArgvWithoutUpdate(argv, argc) + " " + evolveLaunchArgs + " " + consoleEvolveLaunchArgs).c_str(),
                nullptr,
                nullptr,
                FALSE,
                DETACHED_PROCESS,
                nullptr,
                nullptr,
                &si,
                &pi
        );

        exit(0);
    }

    logger.info("Launcher starting...");

    Updater updater;

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

    // File Downloader Setup
    downloader = new FileDownloader(2);

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
    auto blurLabel = new QLabel(&win);
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
        } else if (uiState == SETTINGS || uiState == ONBOARDING_GAME_LOCATION || uiState == ONBOARDING_USERNAME || uiState == ONBOARDING_PROGRESS_IMPORT) {
            cropRect = QRect(0, 0, image.width(), image.height());
        }

        QImage croppedImage = image.copy(cropRect);

        // Apply a blur effect
        QGraphicsScene blurScene;
        QGraphicsPixmapItem *pixmapItem = blurScene.addPixmap(QPixmap::fromImage(croppedImage));
        auto blurEffect = new QGraphicsBlurEffect();
        blurEffect->setBlurRadius(10); // Adjust blur radius
        blurEffect->setBlurHints(QGraphicsBlurEffect::PerformanceHint);
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
                Qt::FastTransformation
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

    auto customNetworkInput = new QLineEdit(bottomOverlay);
    if (overrideNetwork) {
        customNetworkInput->setText(overriddenNetwork.c_str());
    } else if (useCustomNetwork) {
        customNetworkInput->setText(network.c_str());
    } else {
        customNetworkInput->setText("main");
    }
    customNetworkInput->setGeometry(60, HEIGHT / 16, 200, 40);
    customNetworkInput->setStyleSheet("background-color: transparent;");
    customNetworkInput->setFont(boldUI);
    customNetworkInput->setMaxLength(10);
    customNetworkInput->setEnabled(useCustomNetwork);

    QObject::connect(customNetworkInput, &QLineEdit::textEdited, [&](const QString& text) {
        network = text.toStdString();
    });

    auto customNetworkRefreshButton = new QPushButton(bottomOverlay);
    customNetworkRefreshButton->setGeometry(260, HEIGHT / 16, 40, 40);
    customNetworkRefreshButton->setIcon(QIcon("resources/RefreshIcon.svg"));
    customNetworkRefreshButton->setIconSize(QSize(20, 20));
    customNetworkRefreshButton->setCursor(Qt::PointingHandCursor);
    customNetworkRefreshButton->setStyleSheet(R"(
        QPushButton {
                border-top-right-radius: 4px;
                border-bottom-right-radius: 4px;
                background-color: #db3027;
        }
        QPushButton:hover {
            background-color: #b5291d;
        }
    )");
    customNetworkRefreshButton->setEnabled(useCustomNetwork);

    QObject::connect(customNetworkRefreshButton, &QPushButton::released, [&]() {
        customNetworkInput->setText(generateRandomNetworkName().c_str());
    });

    auto customNetworkCheckbox = new QCheckBox(bottomOverlay);
    customNetworkCheckbox->setGeometry(60, HEIGHT / 16 + 56, 20, 20);
    customNetworkCheckbox->setStyleSheet("background-color: transparent;");
    customNetworkCheckbox->setEnabled(!overrideNetwork);
    customNetworkCheckbox->setChecked(useCustomNetwork);

    QObject::connect(customNetworkCheckbox, &QCheckBox::clicked, [&](bool checked) {
        useCustomNetwork = checked;

        customNetworkRefreshButton->setEnabled(useCustomNetwork);
        customNetworkInput->setEnabled(useCustomNetwork);

        if (!useCustomNetwork) {
            network = "main";
        } else {
            if (strcmp(customNetworkInput->text().toStdString().c_str(), "main") == 0) {
                customNetworkInput->setText(generateRandomNetworkName().c_str());
            }
            network = customNetworkInput->text().toStdString();
        }
    });

    auto customNetworkCheckboxLabel = new QLabel(bottomOverlay);
    customNetworkCheckboxLabel->setFont(boldUI);
    customNetworkCheckboxLabel->setText("Use Private Lobby");
    customNetworkCheckboxLabel->setGeometry(85, HEIGHT / 16 + 51, 200, 30);
    customNetworkCheckboxLabel->setStyleSheet("background-color: transparent;");

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
        launchButton->setEnabled(false);
        if (useN3NManager && n3nManagerConnected) {
            // We should never have to wait here, just do it in case we somehow still have to
            if (gameThread != nullptr) {
                WaitForSingleObject(gameThread, INFINITE);
            }

            // Connect to N3N, this doesn't take long, so we do it in this Thread
            if (!N3N::connect(network)) {
                launchButton->setEnabled(true);
                return;
            }

            gameThread = CreateThread(nullptr, 0, runEvolve, nullptr, 0, nullptr);
        } else {
            // We should never have to wait here, just do it in case we somehow still have to
            if (gameThread != nullptr) {
                WaitForSingleObject(gameThread, INFINITE);
            }
            gameThread = CreateThread(nullptr, 0, manuallyConnectToN3NAndRunEvolve, nullptr, 0, nullptr);
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
    settingsOverlay->setGeometry(0, 0, WIDTH, HEIGHT);
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

    auto onboardingGameLocationOverlay = new QWidget(&win);
    onboardingGameLocationOverlay->setGeometry(0, 0, WIDTH, HEIGHT);
    onboardingGameLocationOverlay->setAutoFillBackground(false);
    onboardingGameLocationOverlay->setVisible(false);
    onboardingGameLocationOverlay->setObjectName("OnboardingGameLocationOverlay");
    onboardingGameLocationOverlay->setStyleSheet(R"(
        #OnboardingGameLocationOverlay {
            background-color: rgba(20,20,20,80);
        }
    )");

    // this is placed below the folder select, but needs to be initialized first...
    auto obGameLocationInstructionLabel = new QLabel(onboardingGameLocationOverlay);
    obGameLocationInstructionLabel->setGeometry(40, 160, WIDTH - 80, 200);
    obGameLocationInstructionLabel->setFont(boldUI);
    obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like this is your first time here, so i'll walk you through a short setup.
If you have Evolve Legacy installed already, please select the EvolveGame folder.
If you don't have it installed, please select where you want the game to be installed instead. Make sure the folder is empty.
)");

    // click handler for this button can be found after the onboarding overlay definitions as we need access to those in the handler
    auto obGameLocationContinueButton = new QPushButton(onboardingGameLocationOverlay);
    obGameLocationContinueButton->setFont(boldUI);
    obGameLocationContinueButton->setText("Continue");
    obGameLocationContinueButton->setEnabled(false);
    obGameLocationContinueButton->setGeometry(WIDTH - 240, HEIGHT - 80, 200, 40);
    obGameLocationContinueButton->setCursor(Qt::PointingHandCursor);
    obGameLocationContinueButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obBin64FolderSelectWarningIcon = new QLabel(onboardingGameLocationOverlay);
    obBin64FolderSelectWarningIcon->setGeometry(20, 140, 20, 40);
    obBin64FolderSelectWarningIcon->setPixmap(QIcon("resources/WarningIcon.svg").pixmap(QSize(20, 20)));
    obBin64FolderSelectWarningIcon->setVisible(!gameFound);
    auto obBin64FolderSelectLabel = new QLabel(onboardingGameLocationOverlay);
    obBin64FolderSelectLabel->setText((std::string("Bin64 Location: (") + getShortenedBin64Path() + ")").c_str());
    obBin64FolderSelectLabel->setGeometry(40, 140, 400, 40);
    obBin64FolderSelectLabel->setFont(boldUI);
    auto obBin64FolderSelectButton = new QPushButton(onboardingGameLocationOverlay);
    obBin64FolderSelectButton->setGeometry(440, 140, 200, 40);
    obBin64FolderSelectButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");
    obBin64FolderSelectButton->setCursor(Qt::PointingHandCursor);
    obBin64FolderSelectButton->setText("Select Folder");
    obBin64FolderSelectButton->setFont(boldUI);
    obBin64FolderSelectButton->setEnabled(!overridePathToBin64Folder);

    QObject::connect(obBin64FolderSelectButton, &QPushButton::released, [&]() {
        pathToBin64Folder = QFileDialog::getExistingDirectory(&win, "Select your bin64_SteamRetail folder", pathToBin64Folder.c_str()).toStdString();

        checkGameFolder();

        obBin64FolderSelectWarningIcon->setVisible(!gameFound);
        obBin64FolderSelectLabel->setText((std::string("Bin64 Location: (") + getShortenedBin64Path() + ")").c_str());

        if (!can_write(std::filesystem::path(pathToBin64Folder))) {
            obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like i can't write to the folder you have selected.
Please use a different folder.
)");
            obGameLocationContinueButton->setEnabled(false);
            return;
        }

        if (gameFound) {
            obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like you have selected a valid Evolve Legacy install.
Please double-check if that is the correct install.
If you are sure just click on the button below to install the launcher into the game files.
The launcher might appear to hang during the file verification. This is normal, just let it run.
)");
        } else {
            if (is_empty(std::filesystem::path(pathToBin64Folder))) {
                obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like you have selected an empty folder.
Please double-check if that is where you want to have Evolve installed to.
If you are sure just click on the button below and i will download the game and install the launcher into the files.
If the download gets interrupted just start the launcher again and point it to the same folder.
This might take a bit as the game is 40gb in size.
The launcher might appear to hang during the file verification. This is normal, just let it run.
)");
            } else {
                obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like you have selected a non-empty folder.
If you started the download previously and it got interrupted this is probably fine.
Please double-check if that is where you want to have Evolve installed to.
If you are sure just click on the button below and i will try to continue to download the game and install the launcher into the files.
This might take a bit as the game is 40gb in size.
The launcher might appear to hang during the file verification. This is normal, just let it run.
)");
            }
        }

        obGameLocationContinueButton->setEnabled(true);
    });

    QProgressBar* obDownloadProgressBars[] = {
            new QProgressBar(onboardingGameLocationOverlay),
            new QProgressBar(onboardingGameLocationOverlay),
            new QProgressBar(onboardingGameLocationOverlay)
    };

    QLabel* obDownloadStateLabels[] = {
            new QLabel(onboardingGameLocationOverlay),
            new QLabel(onboardingGameLocationOverlay)
    };

    for (int i = 0; i < 3; ++i) {
        auto bar = obDownloadProgressBars[i];

        bar->setGeometry(40, 420 + i * (30 + (i < 2 ? 20 : 10)), WIDTH - 80, 20);
        bar->setMinimum(0);
        bar->setMaximum(100);
        bar->setVisible(false);
    }

    for (int i = 0; i < 2; ++i) {
        auto label = obDownloadStateLabels[i];
        label->setText("Preparing...");
        label->setGeometry(40, 400 + i * (30 + 20), WIDTH - 80, 20);
        label->setFont(boldUI);
        label->setVisible(false);
    }

    QObject::connect(downloader, &FileDownloader::downloadProgressUpdated, [&](int index, int percent, std::string* filename, DownloadState state) {
        obDownloadProgressBars[index]->setValue(percent);
        obDownloadStateLabels[index]->setText(((state == VERIFYING ? "Verifying: ": "Downloading: ") + *filename).c_str());
    });

    QObject::connect(downloader, &FileDownloader::totalProgressUpdated, [&](int progress) {
        obDownloadProgressBars[2]->setValue(progress);
    });

    auto onboardingUsernameOverlay = new QWidget(&win);
    onboardingUsernameOverlay->setGeometry(0, 0, WIDTH, HEIGHT);
    onboardingUsernameOverlay->setAutoFillBackground(false);
    onboardingUsernameOverlay->setVisible(false);
    onboardingUsernameOverlay->setObjectName("OnboardingUsernameOverlay");
    onboardingUsernameOverlay->setStyleSheet(R"(
        #OnboardingUsernameOverlay {
            background-color: rgba(20,20,20,80);
        }
    )");

    auto obUsernameInstructionLabel = new QLabel(onboardingUsernameOverlay);
    obUsernameInstructionLabel->setGeometry(40, 140, WIDTH - 80, 200);
    obUsernameInstructionLabel->setFont(boldUI);
    obUsernameInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
The game should now be installed.
Please put the username you want to use into the field below!
Once you are happy just hit continue below.
Make sure it is at least 6 characters long please.
)");

    auto obUsernameLabel = new QLabel(onboardingUsernameOverlay);
    obUsernameLabel->setText("Username:");
    obUsernameLabel->setGeometry(40, 360, 400, 40);
    obUsernameLabel->setFont(boldUI);

    auto obUsernameInput = new QLineEdit(onboardingUsernameOverlay);
    obUsernameInput->setGeometry(40, 400, 600, 40);
    obUsernameInput->setPlaceholderText("Username");
    obUsernameInput->setFont(ui);
    obUsernameInput->setMaxLength(20);

    auto obUsernameContinueButton = new QPushButton(onboardingUsernameOverlay);
    obUsernameContinueButton->setFont(boldUI);
    obUsernameContinueButton->setText("Continue");
    obUsernameContinueButton->setEnabled(false);
    obUsernameContinueButton->setGeometry(WIDTH - 240, HEIGHT - 80, 200, 40);
    obUsernameContinueButton->setCursor(Qt::PointingHandCursor);
    obUsernameContinueButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    QObject::connect(obUsernameInput, &QLineEdit::textEdited, [&](const QString& string) {
        if (string.length() > 6) {
            obUsernameContinueButton->setEnabled(true);
        } else {
            obUsernameContinueButton->setEnabled(false);
        }
    });

    auto onboardingProgressImportOverlay = new QWidget(&win);
    onboardingProgressImportOverlay->setGeometry(0, 0, WIDTH, HEIGHT);
    onboardingProgressImportOverlay->setAutoFillBackground(false);
    onboardingProgressImportOverlay->setVisible(false);
    onboardingProgressImportOverlay->setObjectName("OnboardingProgressImportOverlay");
    onboardingProgressImportOverlay->setStyleSheet(R"(
        #OnboardingProgressImportOverlay {
            background-color: rgba(20,20,20,80);
        }
    )");

    auto obProgressImportInstructionLabel = new QLabel(onboardingProgressImportOverlay);
    obProgressImportInstructionLabel->setGeometry(40, 140, WIDTH - 80, 200);
    obProgressImportInstructionLabel->setFont(boldUI);
    obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
The game should now be setup and you are almost ready to play!
If you have played with the old rice-fix or emulator you might want to import your progress!
This is a bit more complex, so please select the game version you were using previously!
You can also just hit skip if you want to do this manually or just started playing.
)");

    auto obProgressImportWarningIcon = new QLabel(onboardingProgressImportOverlay);
    obProgressImportWarningIcon->setGeometry(20, 380, 20, 40);
    obProgressImportWarningIcon->setPixmap(QIcon("resources/WarningIcon.svg").pixmap(QSize(20, 20)));
    obProgressImportWarningIcon->setVisible(false);
    auto obProgressImportAttributesButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportAttributesButton->setGeometry(40, 380, 200, 40);
    obProgressImportAttributesButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");
    obProgressImportAttributesButton->setCursor(Qt::PointingHandCursor);
    obProgressImportAttributesButton->setText("Select Attributes.xml");
    obProgressImportAttributesButton->setFont(boldUI);
    obProgressImportAttributesButton->setVisible(false);

    auto obProgressImportRiceButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportRiceButton->setFont(boldUI);
    obProgressImportRiceButton->setText("Rice-Fix");
    obProgressImportRiceButton->setGeometry(WIDTH - 720, HEIGHT - 80, 200, 40);
    obProgressImportRiceButton->setCursor(Qt::PointingHandCursor);
    obProgressImportRiceButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obProgressImportEmuButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportEmuButton->setFont(boldUI);
    obProgressImportEmuButton->setText("Emulator");
    obProgressImportEmuButton->setGeometry(WIDTH - 480, HEIGHT - 80, 200, 40);
    obProgressImportEmuButton->setCursor(Qt::PointingHandCursor);
    obProgressImportEmuButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obProgressImportBackButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportBackButton->setFont(boldUI);
    obProgressImportBackButton->setText("Back");
    obProgressImportBackButton->setGeometry( 40, HEIGHT - 80, 200, 40);
    obProgressImportBackButton->setCursor(Qt::PointingHandCursor);
    obProgressImportBackButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obProgressImportSkipButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportSkipButton->setFont(boldUI);
    obProgressImportSkipButton->setText("Skip");
    obProgressImportSkipButton->setGeometry(WIDTH - 240, HEIGHT - 80, 200, 40);
    obProgressImportSkipButton->setCursor(Qt::PointingHandCursor);
    obProgressImportSkipButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obProgressImportGameLaunchButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportGameLaunchButton->setFont(boldUI);
    obProgressImportGameLaunchButton->setText("Continue");
    obProgressImportGameLaunchButton->setVisible(false);
    obProgressImportGameLaunchButton->setGeometry(WIDTH - 240, HEIGHT - 80, 200, 40);
    obProgressImportGameLaunchButton->setCursor(Qt::PointingHandCursor);
    obProgressImportGameLaunchButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    auto obProgressImportFinishButton = new QPushButton(onboardingProgressImportOverlay);
    obProgressImportFinishButton->setFont(boldUI);
    obProgressImportFinishButton->setText("Continue");
    obProgressImportFinishButton->setVisible(false);
    obProgressImportFinishButton->setGeometry(WIDTH - 240, HEIGHT - 80, 200, 40);
    obProgressImportFinishButton->setCursor(Qt::PointingHandCursor);
    obProgressImportFinishButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 20, 90);
            border: 1px solid rgba(80, 80, 80, 90);
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: rgba(40, 40, 40, 140);
        }
    )");

    // we need access to the other overlays here
    QObject::connect(obGameLocationContinueButton, &QPushButton::released, [&]() {
        obGameLocationContinueButton->setEnabled(false);

        for (auto bar : obDownloadProgressBars) {
            bar->setVisible(true);
        }

        for (auto label : obDownloadStateLabels) {
            label->setVisible(true);
        }

        // download game files
        QList<QPair<QUrl, QString>> filesToDownload;
        for (const std::string& entry : EVOLVE_GAME_FILES) {
            filesToDownload.append(QPair<QUrl, QString>(QUrl((FILESERVER_URL + entry).c_str()), QString((pathToBin64Folder + entry).c_str())));
        }

        downloader->downloadFiles(filesToDownload);
    });

    QObject::connect(downloader, &FileDownloader::allDownloadsFinished, [&](bool succeeded) {
        if (succeeded) {
            // Advance to progress import stage
            checkGameFolder();

            if (gameFound) {
                deployLauncher();

                uiState = ONBOARDING_USERNAME;
                onboardingGameLocationOverlay->setVisible(false);
                onboardingUsernameOverlay->setVisible(true);

                return;
            }
        }

        for (auto bar : obDownloadProgressBars) {
            bar->setVisible(false);
        }

        for (auto label : obDownloadStateLabels) {
            label->setVisible(false);
        }

        obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like something went wrong downloading the game.
Please re-start the launcher and try again, if the problem persists contact DeinAlbtraum on Discord.
)");
    });

    QObject::connect(obUsernameContinueButton, &QPushButton::released, [&]() {
        // Generate steamid and put username and steamid into the EvolveCrack/273350/settings/configs.user.ini
        std::string userIniPart1 = "[user::general]\n"
                                   "\n"
                                   "account_name=";
        std::string userIniPart2 = "\n"
                                   "# Steam64 format\n"
                                   "account_steamid=";
        std::string userIniPart3 = "\n"
                                   "# the language reported to the game, default is 'english', check 'API language code' in https://partner.steamgames.com/doc/store/localization/languages\n"
                                   "language=english\n"
                                   "\n"
                                   "# ISO 3166-1-alpha-2 format, use this link to get the 'Alpha-2' country code: https://www.iban.com/country-codes\n"
                                   "ip_country=US";

        // combine part1 + username + part2 + steamid + part3
        std::string userIniFull = userIniPart1 + obUsernameInput->text().toStdString() + userIniPart2 + generateRandomSteamID64() + userIniPart3;

        auto settingsFilePath = std::filesystem::path(gameFolder).append("EvolveCrack/273350/settings/configs.user.ini");

        QFileInfo fi(settingsFilePath);
        if (!fi.absoluteDir().mkpath(fi.absolutePath())) {
            obUsernameInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like something went wrong setting your username and steamid.
Please re-start the launcher and try again, if the problem persists contact DeinAlbtraum on Discord.
)");
            obUsernameContinueButton->setEnabled(false);
        }

        auto file = new QFile(settingsFilePath);
        if (!file->open(QIODevice::ReadWrite)) {
            obUsernameInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like something went wrong setting your username and steamid.
Please re-start the launcher and try again, if the problem persists contact DeinAlbtraum on Discord.
)");
            obUsernameContinueButton->setEnabled(false);
        }

        file->write(userIniFull.c_str());
        file->close();

        uiState = ONBOARDING_PROGRESS_IMPORT;
        onboardingUsernameOverlay->setVisible(false);
        onboardingProgressImportOverlay->setVisible(true);
    });

    enum ProgressImportMode {
        STEAM,
        EMULATOR
    };

    ProgressImportMode importMode = STEAM;
    std::string attributeXmlLocation = "";

    bool importWaitingForGame = false;

    auto watcher = new QFileSystemWatcher();

    QObject::connect(obProgressImportRiceButton, &QPushButton::released, [&]() {
        obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
You want to import progress from a steam install? Great!
Evolve saves your progress in an attributes.xml file.
This file is usually stored in "C:\Program Files (x86)\Steam\userdata\STEAM_ID32\273350\remote\USER0\Profiles\default".
I can't guess your steam-id, but there should only be one folder in the userdata folder.
Please select the attributes.xml you want to import.
For assistance please ask in the discord.
)");
        obProgressImportAttributesButton->setVisible(true);
        obProgressImportRiceButton->setVisible(false);
        obProgressImportEmuButton->setVisible(false);
        obProgressImportSkipButton->setVisible(false);
        importMode = STEAM;
    });

    QObject::connect(obProgressImportEmuButton, &QPushButton::released, [&]() {
        obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
You want to import progress from an emu install? Great!
Evolve saves your progress in an attributes.xml file.
This file is usually stored in "YOUR_EMU_INSTALL\bin64_SteamRetail\EvolveCrack\273350\remote\USER0\Profiles\default".
Please select the attributes.xml you want to import.
For assistance please ask in the discord.
)");

        obProgressImportAttributesButton->setVisible(true);
        obProgressImportRiceButton->setVisible(false);
        obProgressImportEmuButton->setVisible(false);
        obProgressImportSkipButton->setVisible(false);
        importMode = EMULATOR;
    });

    QObject::connect(obProgressImportAttributesButton, &QPushButton::released, [&]() {
        if (importMode == STEAM) {
            std::string steamDefaultLocation = R"(C:\Program Files (x86)\Steam\userdata\)";
            if (exists(std::filesystem::path(steamDefaultLocation))) {
                attributeXmlLocation = QFileDialog::getOpenFileName(&win, R"(Select STEAMID_32\273350\remote\USER0\Profiles\default\attributes.xml)", steamDefaultLocation.c_str(), "XML files (*.xml)").toStdString();
            } else {
                attributeXmlLocation = QFileDialog::getOpenFileName(&win, "Select attributes.xml", "C:", "XML files (*.xml)").toStdString();
            }
        } else {
            attributeXmlLocation = QFileDialog::getOpenFileName(&win, "Select Attributes.xml", gameFolder.string().c_str(), "XML files (*.xml)").toStdString();
        }

        if (!attributeXmlLocation.ends_with("attributes.xml")) {
            return;
        }

        QByteArray attributesPrefix = "TRSEVLVPC";

        QFile file(attributeXmlLocation.c_str());

        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        QByteArray fileStart = file.read(attributesPrefix.size());
        file.close();

        if (fileStart != attributesPrefix) {
            return;
        }

        watcher->removePaths(watcher->files());
        watcher->addPath(std::filesystem::path(gameFolder).append(R"(EvolveCrack\273350\273350\remote\USER0\Profiles\default)").string().c_str());

        obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
Okay, you have selected an attributes.xml file.
Now the game needs to generate it's own attribute.xml file so we can move your save over.
Once you click on continue below the game will start. Just let it run, it should close itself after a few seconds.
)");

        importWaitingForGame = true;

        obProgressImportGameLaunchButton->setVisible(true);
    });

    QObject::connect(obProgressImportGameLaunchButton, &QPushButton::released, [&]() {
        gameThread = CreateThread(nullptr, 0, runEvolve, nullptr, 0, nullptr);
    });

    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path) {
        QDir dir(path);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

        if (importWaitingForGame) {
            for (const auto& file : files) {
                if (file.endsWith("attributes.xml")) {
                    TerminateProcess(pi.hProcess, 1);

                    QString magicString = "47d9579f3197";
                    QString originalSave = SaveFileHelper::readOriginal2Hex(attributeXmlLocation.c_str());
                    QString newSave = SaveFileHelper::readOriginal2Hex(std::filesystem::path(gameFolder).append(R"(EvolveCrack\273350\273350\remote\USER0\Profiles\default)").append(file.toStdString()).string().c_str());

                    if (originalSave == nullptr || newSave == nullptr) {
                        obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like something went wrong importing your save.
Please re-start the launcher and try again, if the problem persists contact DeinAlbtraum on Discord.
)");

                        obProgressImportGameLaunchButton->setEnabled(false);
                        obProgressImportBackButton->setEnabled(false);
                        return;
                    }

                    importWaitingForGame = false;

                    QString newSteamid = newSave.split(magicString)[0];
                    QString originalContent = originalSave.split(magicString)[1];

                    SaveFileHelper::writeNew2Binary(newSteamid + magicString + originalContent, std::filesystem::path(gameFolder).append(R"(EvolveCrack\273350\273350\remote\USER0\Profiles\default)").append(file.toStdString()).string().c_str());

                    obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
Alright, that should be it.
Your progress should now be imported, have fun playing!
Just hit continue below to finish the onboarding process.
)");

                    obProgressImportGameLaunchButton->setVisible(false);
                    obProgressImportFinishButton->setVisible(true);
                }
            }
        }
    });

    auto updateOverlay = new QWidget(&win);
    updateOverlay->setGeometry(0, 0, WIDTH, HEIGHT);
    updateOverlay->setAutoFillBackground(false);
    updateOverlay->setVisible(false);
    updateOverlay->setObjectName("updateOverlay");
    updateOverlay->setStyleSheet(R"(
        #updateOverlay {
            background-color: rgba(20,20,20,80);
        }
    )");

    auto updateLabel = new QLabel(updateOverlay);
    updateLabel->setGeometry(40, 140, WIDTH - 80, 200);
    updateLabel->setFont(boldUI);
    updateLabel->setText(R"(Update detected!
Hang tight, we are downloading an update.
The launcher should restart in a bit.
)");

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

    // Onboarding starting logic
    if (firstRun || doOnboarding) {
        // If we run the onboarding process make sure to hide the settings button
        uiState = ONBOARDING_GAME_LOCATION;
        settingsButton->setVisible(false);
        blurLabel->setGeometry(0, 0, WIDTH, HEIGHT);
        blurLabel->setVisible(false);
        bottomOverlay->setVisible(false);
        onboardingGameLocationOverlay->setVisible(true);

        checkGameFolder();

        if (gameFound) {
            obGameLocationInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
It looks like you already have a valid Evolve Legacy install configured.
Please double-check if that is the correct install.
If you are sure just click on the button below to install the launcher into the game files.
The launcher might appear to hang during the file verification. This is normal, just let it run.
)");

            obGameLocationContinueButton->setEnabled(true);
        }
    }

    QObject::connect(obProgressImportBackButton, &QPushButton::released, [&]() {
        uiState = ONBOARDING_USERNAME;
        onboardingUsernameOverlay->setVisible(true);
        onboardingProgressImportOverlay->setVisible(false);

        obProgressImportAttributesButton->setVisible(false);
        obProgressImportRiceButton->setVisible(true);
        obProgressImportEmuButton->setVisible(true);
        obProgressImportSkipButton->setVisible(true);

        obProgressImportInstructionLabel->setText(R"(Welcome to the Evolve Launcher!
The game should now be setup and you are almost ready to play!
If you have played with the old rice-fix or emulator you might want to import your progress!
This is a bit more complex, so please select the game version you were using previously!
You can also just hit skip if you want to do this manually or just started playing.
)");
    });

    QObject::connect(obProgressImportSkipButton, &QPushButton::released, [&]() {
        firstRun = false;
        uiState = DEFAULT;
        blurLabel->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
        blurLabel->setVisible(false);
        bottomOverlay->setVisible(true);
        onboardingProgressImportOverlay->setVisible(false);
    });

    QObject::connect(obProgressImportFinishButton, &QPushButton::released, [&]() {
        firstRun = false;
        uiState = DEFAULT;
        blurLabel->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
        blurLabel->setVisible(false);
        bottomOverlay->setVisible(true);
        onboardingProgressImportOverlay->setVisible(false);
    });

    QObject::connect(&updater, &Updater::newVersionDetected, [&](const QString& version) {
        blurLabel->setGeometry(0, HEIGHT - HEIGHT / 4, WIDTH, HEIGHT / 4);
        blurLabel->setVisible(false);
        bottomOverlay->setVisible(false);
        settingsOverlay->setVisible(false);
        onboardingGameLocationOverlay->setVisible(false);
        onboardingUsernameOverlay->setVisible(false);
        onboardingProgressImportOverlay->setVisible(false);
        settingsButton->setVisible(false);

        updateOverlay->setVisible(true);
    });

    QObject::connect(&updater, &Updater::updateExtracted, [&](const QString& path) {
        CreateProcessA(
                nullptr,
                (LPSTR) (std::filesystem::path(gameFolder).append("EvolveLauncher.exe").string() + " --update " +
                        joinArgvWithoutUpdate(argv, argc) + " " + evolveLaunchArgs + " " + consoleEvolveLaunchArgs).c_str(),
                nullptr,
                nullptr,
                FALSE,
                DETACHED_PROCESS,
                nullptr,
                nullptr,
                &si,
                &pi
        );

        win.close();
    });

    // Display the window
    win.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    win.show();

    // Has to be done here, make sure the videoItem gets resized once to fill the screen
    QRectF viewRect = graphicsView->viewport()->rect();
    videoItem->setSize(viewRect.size());

    // UI Loop
    int rc = a.exec();

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

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