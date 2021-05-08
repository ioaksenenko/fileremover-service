#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_BUFLEN 512

#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdio.h>

#pragma comment (lib,"Ws2_32.lib")


int main() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) {
        wprintf(L"WSAStartup function failed with error: %d\n", res);
        return 1;
    }
    SOCKET client_socket;
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        wprintf(L"socket function failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(34543);
    res = connect(client_socket, (SOCKADDR*)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
        wprintf(L"connect function failed with error: %ld\n", WSAGetLastError());
        res = closesocket(client_socket);
        if (res == SOCKET_ERROR) {
            wprintf(L"closesocket function failed with error: %ld\n", WSAGetLastError());
        }
        WSACleanup();
        return 1;
    }
    wprintf(L"Connected to server.\n");
    char filename[DEFAULT_BUFLEN];
    printf("Enter the name of the file you want to delete: ");
    scanf("%s", filename);
    res = send(client_socket, filename, strlen(filename), 0);
    if (res == SOCKET_ERROR) {
        printf("Send failed: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    else {
        printf("Send %d bytes: %s\n", strlen(filename), filename);
    }
    char response[DEFAULT_BUFLEN];
    int bytes_num = recv(client_socket, response, DEFAULT_BUFLEN, 0);
    if (bytes_num > 0) {
        response[bytes_num] = '\0';
        printf("Received %d bytes: %s\n", bytes_num, response);
    }
    else if (res == 0)
        printf("Connection closing...\n");
    else {
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    res = closesocket(client_socket);
    if (res == SOCKET_ERROR) {
        wprintf(L"closesocket function failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    WSACleanup();
    return 0;
}