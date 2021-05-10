#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_BUFLEN 512

#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdlib.h>

#pragma comment (lib,"Ws2_32.lib")

SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
char filename[DEFAULT_BUFLEN];

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
int StartClient();
void Log(char const* const format, ...);

#define SERVICE_NAME _T("Fileremover Client Service")


int _tmain(int argc, TCHAR* argv[])
{
    if (argc > 1) {
        int i = 0;
        while (argv[1][i] != '\0') {
            filename[i] = argv[1][i++];
        }
        Log("name of the file to be deletedÆ %s", filename);
    } else {
        Log("you must pass the name of the file you want to delete");
        return 1;
    }
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    if (StartServiceCtrlDispatcherA(ServiceTable) == FALSE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            StartClient();
        }
        return error;
    }
    return 0;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    DWORD Status = E_FAIL;
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (g_StatusHandle == NULL) {
        Log("RegisterServiceCtrlHandler failed with error");
        goto EXIT;
    }
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        Log("SetServiceStatus failed with error");
    }
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        Log("CreateEvent failed with error");
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
            Log("SetServiceStatus failed with error");
        }
        goto EXIT;
    }
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        Log("SetServiceStatus failed with error");
    }
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(g_ServiceStopEvent);
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;
    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        Log("SetServiceStatus failed with error");
    }
EXIT:
    return;
}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;
        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
            Log("SetServiceStatus failed with error");
        }
        SetEvent(g_ServiceStopEvent);
        break;
    default:
        break;
    }
}


DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        StartClient();
    }
    return ERROR_SUCCESS;
}


int StartClient() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) {
        Log("WSAStartup function failed with error: %d", res);
        return 1;
    }
    SOCKET client_socket;
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        Log("socket function failed with error: %ld", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(34543);
    res = connect(client_socket, (SOCKADDR*)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
        Log("connect function failed with error: %ld", WSAGetLastError());
        res = closesocket(client_socket);
        if (res == SOCKET_ERROR) {
            Log("Closesocket function failed with error: %ld", WSAGetLastError());
        }
        WSACleanup();
        return 1;
    }
    Log("Connected to server.");
    res = send(client_socket, filename, strlen(filename), 0);
    if (res == SOCKET_ERROR) {
        Log("send failed: %d", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    else {
        Log("send %d bytes: %s\n", strlen(filename), filename);
    }
    char response[DEFAULT_BUFLEN];
    int bytes_num = recv(client_socket, response, DEFAULT_BUFLEN, 0);
    if (bytes_num > 0) {
        response[bytes_num] = '\0';
        Log("received %d bytes: %s", bytes_num, response);
    }
    else if (res == 0)
        Log("connection closing...");
    else {
        Log("receive failed with error: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    res = closesocket(client_socket);
    if (res == SOCKET_ERROR) {
        Log("closesocket function failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    WSACleanup();
    return 0;
}


void Log(char const* const format, ...)
{
    va_list args;
    int len;
    char* buffer;
    va_start(args, format);
    len = _vscprintf(format, args) + 1;
    buffer = (char*)malloc(len * sizeof(char));
    if (0 != buffer) {
        vsprintf(buffer, format, args);
        OutputDebugStringA(buffer);
        free(buffer);
    }
    va_end(args);
}