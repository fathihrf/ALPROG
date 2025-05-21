#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <limits>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>
#include <future>
#include <winsock2.h>

using namespace std;

#define PORT 8890

class TcpServer {
    public: 
        bool bindListen(SOCKET listen_socket, struct sockaddr_in &server) {
            if (bind(listen_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
                cerr << "Bind failed. Error code: " << WSAGetLastError() << "\n";
                closesocket(listen_socket);
                WSACleanup();
                return false;
            }
            cout << "Bind done.\n";
        
            listen(listen_socket, 3);
            cout << "Waiting for incoming connections...\n";
            return true;
        };
        int acceptClient(SOCKET listen_socket, SOCKET &client_socket, sockaddr_in client) {
            int c = sizeof(struct sockaddr_in);
            client_socket = accept(listen_socket, (struct sockaddr*)&client, &c);
            if (client_socket == INVALID_SOCKET) {
                cerr << "Accept failed. Error code: " << WSAGetLastError() << "\n";
                closesocket(listen_socket);
                WSACleanup();
                return 1;
            }
            cout << "Connection to client accepted.\n\n";
            return 0;
        };
        int serverSend(SOCKET client_socket) {
            string messageSend;
            cout << "Message to client: ";
            getline(cin, messageSend);

            send(client_socket, messageSend.c_str(), messageSend.length(), 0);
            return 0;
        };
        int serverRecv(SOCKET client_socket) {
            char messageRecv[100];
            int recv_size = recv(client_socket, messageRecv, sizeof(messageRecv) - 1, 0);
            if(recv_size == SOCKET_ERROR){
                cout << "Failed to receive message. Error code: " <<  WSAGetLastError() << "\n";
            }
            else {
                messageRecv[recv_size] = '\0';
            }
            cout << "Message from client: " << messageRecv << "\n";
            return 0;
        };
};

class Tangki {
    public:
        string Name;
        float Volume;
        float Level;
}; // nanti di list-in`

/*string statusTangki(Tangki tangki)

*/

/*int 
*/

/*void sortingAngka(angkaWeighted angka[], int nAngka) {
    int k, m, localHighest;
    angkaWeighted temp;
    for (k = 0; k < nAngka; ++k) {
        temp = angka[k];
        localHighest = k;
        for (m = k + 1; m < nAngka; ++m) {
            if (angka[m].jmlh0 < temp.jmlh0) {
                temp = angka[m];
                localHighest = m;
            }
            else if (angka[m].jmlh0 == temp.jmlh0) {
                if (angka[m].angka > temp.angka) {
                    temp = angka[m];
                    localHighest = m;
                }
            }
        }
        angka[localHighest] = angka[k];
        angka[k] = temp;
    }
}*/

int main() {
    WSADATA wsa;
    SOCKET listen_socket, client_socket;
    struct sockaddr_in server, client;
    int c, nClient, i;
    string messageSend;
    TcpServer serverSide;

    cout << "Initialising Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed. Error code: " << WSAGetLastError() << "\n";
        return 1;
    }
    cout << "Winsock initialised.\n";

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == INVALID_SOCKET) {
        cerr << "Could not create socket. Error code: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
    cout << "Socket created.\n";

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    serverSide.bindListen(listen_socket, server);

    cout << "Number of clients: ";
    cin >> nClient;
    cin.clear();
    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    for (i = 0; i < nClient; ++i) {
        serverSide.acceptClient(listen_socket, client_socket, client);
        serverSide.serverSend(client_socket);
        serverSide.serverRecv(client_socket);
        closesocket(client_socket);
    }

    closesocket(listen_socket);
    WSACleanup();

    return 0;
}

/*
Volume boleh include atau ga (mungkin liat dari minimum water volume untuk pompa ga dry run di google)
kyknya butuh max level dari setiap client deh (tergantung sensor levelnya ngerecord apaan)
output warning terserah mau gmn (cout di server paling simple sih)
semua data dan aksi observable di terminal client & server (client di client, server di server)
siapkan scenario di mana ada multiple client
*/