#define _CRT_SECURE_NO_WARNINGS
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

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
int StartServer();
void Log(char const* const format, ...);

#define SERVICE_NAME _T("Fileremover Server Service")


int _tmain(int argc, TCHAR* argv[])
{
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    if (StartServiceCtrlDispatcherA(ServiceTable) == FALSE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            StartServer();
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
        StartServer();
    }
    return ERROR_SUCCESS;
}


int StartServer() {
    WSADATA wsaData = { 0 };
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) {
        Log("WSAStartup function failed with error: %d", res);
        return 1;
    }
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        Log("socket function failed with error = %d", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(34543);
    res = bind(listen_socket, (SOCKADDR*)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
        Log("bind failed with error %u", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    Log("listening...\n");
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        Log("listen failed with error: %ld", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    SOCKET client_socket = accept(listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        Log("accept failed with error: %d", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    Log("accepted client");
    char filename[DEFAULT_BUFLEN];
    int recv_bytes = recv(client_socket, filename, DEFAULT_BUFLEN, 0);
    if (recv_bytes > 0) {
        filename[recv_bytes] = '\0';
        Log("recieve %d bytes: %s", recv_bytes, filename);
    }
    else if (recv_bytes == 0)
        Log("connection closing...");
    else {
        Log("receive failed with error: %d\n", WSAGetLastError());
        closesocket(client_socket);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    Log("deleting file '%s'\n", filename);
    res = remove(filename);
    if (res == -1) {
        Log("remove faliled with error: %s", strerror(errno));
        closesocket(client_socket);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    char response[DEFAULT_BUFLEN];
    sprintf(response, "file '%s' deleted successfully\n", filename);
    res = send(client_socket, response, strlen(response), 0);
    if (res == SOCKET_ERROR) {
        Log("send failed with error: %d", WSAGetLastError());
        closesocket(client_socket);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    res = closesocket(client_socket);
    if (res == SOCKET_ERROR) {
        Log("close client_socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    res = closesocket(listen_socket);
    if (res == SOCKET_ERROR) {
        Log("close listen_socket failed with error: %ld\n", WSAGetLastError());
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
