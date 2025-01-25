#include "n3n/N3NHelper.h"
#include "log/Logger.h"
#include <windows.h>
#include <string>
#include <fstream>
#include <filesystem>

enum PIPE_STATUS {
    STOPPED,
    STARTING,
    RUNNING,
    KEEP,
    RESTART,
    ERRORED
};

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
HANDLE hPipe;  // Handle for the named pipe
PIPE_STATUS pipeStatus = STOPPED;
OVERLAPPED ol = { 0 };

void WINAPI ServiceMain(DWORD argc, LPSTR *argv);
void WINAPI ServiceCtrlHandler(DWORD dwCtrl);
void StartPipeServer();
void StopPipeServer();
void HandlePipe();
void Respond(const std::string& message);
void InstallService(const TCHAR* serviceName, const TCHAR* executablePath, bool hasRetry);
void RemoveService(const TCHAR* serviceName);

//get path we execute from
static TCHAR szUnquotedPath[MAX_PATH];
static const bool foundPath = GetModuleFileName( nullptr, szUnquotedPath, MAX_PATH );
static std::filesystem::path fullpath(szUnquotedPath);
static const auto folder = fullpath.remove_filename().string();

static LOGGER logger(folder);

void InstallService(const TCHAR* serviceName, const TCHAR* executablePath, bool hasRetry = true) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        logger.error("Failed to open service control manager. Error: " + std::to_string(GetLastError()));
        return;
    }

    SC_HANDLE hService = CreateService(
            hSCManager,               // SCM database handle
            serviceName,              // Service name
            serviceName,              // Display name
            SERVICE_ALL_ACCESS,       // Desired access
            SERVICE_WIN32_OWN_PROCESS,// Service type
            SERVICE_AUTO_START,       // Start type
            SERVICE_ERROR_NORMAL,     // Error control type
            executablePath,           // Path to service executable
            nullptr,                  // No load ordering group
            nullptr,                  // No tag identifier
            nullptr,                  // No dependencies
            nullptr,                  // LocalSystem account
            nullptr                   // No password
    );

    if (!hService) {
        if (GetLastError() != 1073 || !hasRetry) {
            logger.error("Failed to create service. Error: " + std::to_string(GetLastError()));
        } else {
            RemoveService(serviceName);
            InstallService(serviceName, executablePath, false);
        }
    } else {
        logger.info("Service installed successfully.");

        // Start the service after installation
        if (StartService(hService, 0, nullptr)) {
            logger.info("Service started successfully.");
        } else {
            logger.error("Failed to start service. Error: " + std::to_string(GetLastError()));
        }

        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCManager);
}

void RemoveService(const TCHAR* serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        logger.error("Failed to open service control manager. Error: " + std::to_string(GetLastError()));
        return;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_STOP | DELETE);
    if (!hService) {
        logger.error("Failed to open service. Error: " + std::to_string(GetLastError()));
        CloseServiceHandle(hSCManager);
        return;
    }

    SERVICE_STATUS status;
    if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        logger.info("Stopping service...");
        Sleep(1000); // Give the service some time to stop
    }

    if (!DeleteService(hService)) {
        logger.error("Failed to delete service. Error: " + std::to_string(GetLastError()));
    } else {
        logger.info("Service removed successfully.");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}

int main(int argc, char* argv[]) {
    const TCHAR* serviceName = "EvolveN3NManager";

    if (argc > 1) {
        if (strcmp(argv[1], "--install") == 0) {
            InstallService(serviceName, szUnquotedPath);
            return 0;
        } else if (strcmp(argv[1], "--remove") == 0) {
            RemoveService(serviceName);
            return 0;
        } else {
            logger.error(std::string("Invalid argument. Usage: ") + argv[0] + " --install | --remove");
            return 1;
        }
    }

    SERVICE_TABLE_ENTRY ServiceTable[] = {
            { (LPSTR)"EvolveN3NManager", (LPSERVICE_MAIN_FUNCTION)ServiceMain },
            { nullptr, nullptr }
    };

    // Start the service control dispatcher
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        logger.error("Error starting service control dispatcher: " + std::to_string(GetLastError()));
        return 1;
    }

    return 0;
}

void WINAPI ServiceMain(DWORD argc, LPSTR *argv)
{
    // Register the service control handler
    hStatus = RegisterServiceCtrlHandler("EvolveN3NManager", ServiceCtrlHandler);

    if (hStatus == nullptr)
    {
        logger.error("RegisterServiceCtrlHandler failed!");
        return;
    }

    // Service is starting
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

    if (SetServiceStatus(hStatus, &ServiceStatus) == FALSE)
    {
        logger.error("SetServiceStatus failed!");
        return;
    }

    // Service is now running
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    if (SetServiceStatus(hStatus, &ServiceStatus) == FALSE)
    {
        logger.error("SetServiceStatus failed!");
        return;
    }

    // Start the named pipe server to listen for client requests
    StartPipeServer();

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        if (pipeStatus == RESTART) {
            StartPipeServer();
        }

        DWORD dwWaitResult = WaitForSingleObject(ol.hEvent, 0);
        DWORD bytesTransferred;

        if (dwWaitResult == WAIT_OBJECT_0) {
            // The connection was established
            if (GetOverlappedResult(hPipe, &ol, &bytesTransferred, FALSE)) {
                logger.info("Client connected async, processing...");
                HandlePipe();
                break;
            } else {
                logger.error("Failed to get result from overlapped operation.");
                pipeStatus = ERRORED;
                break;
            }
        }
    }

    logger.info("Stopping pipe server");
    StopPipeServer();

    logger.info("Goodbye");
}

void HandlePipe() {
    char buffer[512];
    DWORD bytesRead;

    pipeStatus = KEEP;

    while (pipeStatus == KEEP && ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        // Read the client's request from the pipe
        if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, nullptr) == FALSE) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                // Client has disconnected
                logger.info("Client disconnected.");
                pipeStatus = RESTART;
                return;
            } else {
                logger.error("ReadFile failed, error: " + std::to_string(GetLastError()));
                pipeStatus = ERRORED;
                return;
            }
        }

        std::string message = std::string(buffer, bytesRead);

        logger.info("Received message: " + message);

        if (message.starts_with("connect")) {
            std::string network = message.replace(0, sizeof("connect "), "");
            if (network.empty()) {
                if (N3N::connect()) {
                    Respond("Connected to main");
                    logger.info("Connected to main");
                } else {
                    Respond("Connection to main failed");
                    logger.error("Connection to main failed: " + std::to_string(GetLastError()));
                }
            } else {
                if (N3N::connect(network)) {
                    Respond("Connected to " + network);
                    logger.info("Connected to " + network);
                } else {
                    Respond("Connection to " + network + " failed");
                    logger.error("Connection to " + network + " failed: " + std::to_string(GetLastError()));
                }
            }
        } else if (message.starts_with("disconnect")) {
            N3N::shutdown();
            Respond("Disconnected");
            logger.info("Disconnected");
        } else {
            Respond("Unknown Command!");
            logger.warn("Unknown Command!");
        }
    }
}

void Respond(const std::string& message) {
    DWORD bytesWritten;
    if (!WriteFile(hPipe, message.c_str(), message.length(), &bytesWritten, nullptr))
    {
        logger.error("WriteFile failed, error: " + std::to_string(GetLastError()));
        pipeStatus = ERRORED;
    }
}

void WINAPI ServiceCtrlHandler(DWORD dwCtrl)
{
    switch (dwCtrl)
    {
        case SERVICE_CONTROL_STOP:
            ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(hStatus, &ServiceStatus);
            // Stop the service and named pipe server
            CancelIoEx(hPipe, nullptr);
            pipeStatus = STOPPED;
            StopPipeServer();
            N3N::shutdown();
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(hStatus, &ServiceStatus);
            break;

        default:
            break;
    }
}

void StartPipeServer() {
    pipeStatus = STARTING;

    logger.info("Starting pipe server");

    //Make sure the pipe is accessible by non-elevated processes as the launcher won't have admin rights!
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
    SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE};

    //Create blocking pipe for IO, Buffer size is 512
    hPipe = CreateNamedPipe(
            R"(\\.\pipe\EvolveN3NManager)",
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            512,
            512,
            0,
            &sa
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        logger.error("Failed to create pipe server: Invalid Handle");
        pipeStatus = ERRORED;
        return;
    }

    //Create overlapped structure, needed for Async IO
    ol.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (ol.hEvent == nullptr) {
        logger.error("Failed to create event. Error: " + std::to_string(GetLastError()));
        CloseHandle(hPipe);
        pipeStatus = ERRORED;
        return;
    }

    logger.info("Waiting for client connection");

    pipeStatus = RUNNING;

    BOOL connected = ConnectNamedPipe(hPipe, &ol);

    if (connected) {
        logger.info("Client connected, processing");
        HandlePipe();
        StopPipeServer();
        if (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
            pipeStatus = RESTART;
        }
        return;
    } else if (GetLastError() != ERROR_IO_PENDING) {
        logger.error("Failed to connect to pipe. Error: " + std::to_string(GetLastError()));
        StopPipeServer();
        pipeStatus = ERRORED;
        return;
    }
}

void StopPipeServer()
{
    if (hPipe != INVALID_HANDLE_VALUE)
    {
        if (pipeStatus != ERRORED) {
            pipeStatus = STOPPED;
        }
        CancelIo(hPipe);
        //Disconnect the client and close the pipe handle
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        if (ol.hEvent && ol.hEvent != INVALID_HANDLE_VALUE) {
            CloseHandle(ol.hEvent);
        }
        logger.info("Named pipe server stopped.");
    }
}