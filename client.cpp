#include <iostream>
#include <string>
#include <winsock2.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

using namespace std;

#define SERVER "127.0.0.1"
#define PORT 8890

// Tangki class to store water tank information
class Tangki {
public:
    string Name;
    float Volume;
    float Level;
    float MinVolume;

    Tangki(string name = "Tank1", float volume = 100.0f, float level = 50.0f, float minVolume = 20.0f) {
        Name = name;
        Volume = volume;
        Level = level;
        MinVolume = minVolume;
    }

    string toString() const {
        stringstream ss;
        ss << Name << "," << Volume << "," << Level << "," << MinVolume;
        return ss.str();
    }
};

class TcpClient { 
public: 
    bool clientConnect(SOCKET client_socket, struct sockaddr_in server) {
        if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
            cerr << "Connection failed. Error code: " << WSAGetLastError() << "\n";
            closesocket(client_socket);
            WSACleanup();
            return false;
        }
        cout << "Connected to server.\n\n";
        return true;
    }

    int clientSend(SOCKET client_socket, const string& message) {
        send(client_socket, message.c_str(), message.length(), 0);
        cout << "Sent to server: " << message << "\n";
        return 0;
    }

    int clientRecv(SOCKET client_socket) {
        char messageRecv[100];
        int recv_size;
        recv_size = recv(client_socket, messageRecv, sizeof(messageRecv) - 1, 0);
        if(recv_size == SOCKET_ERROR){
            cout << "Failed to receive message. Error code: " <<  WSAGetLastError() << "\n";
        }
        else {
            messageRecv[recv_size] = '\0';
        }
        cout << "Message from server: " << messageRecv << "\n";
        return 0;
    }
};

// Function to simulate tank level changes
void simulateLevelChanges(Tangki& tank) {
    // Simulate random changes in level
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<float> dis(-5.0f, 0.0f);
    
    tank.Level += dis(gen);
    
    // Ensure level stays within reasonable bounds
    if (tank.Level < 0) tank.Level = 0;
    if (tank.Level > 120) tank.Level = 120; // Allowing overflow simulation
    
    cout << "Current " << tank.Name << " status: Level = " << tank.Level 
         << "%, Volume = " << tank.Volume << "L, Effective volume = " 
         << (tank.Volume * tank.Level / 100.0f) << "L\n";
}

// Function to get current timestamp
string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto time_t_now = chrono::system_clock::to_time_t(now);
    stringstream ss;
    tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    ss << put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

int main() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server;
    TcpClient clientSide;

    cout << "Initialising Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed. Error code: " << WSAGetLastError() << "\n";
        return 1;
    }
    cout << "Winsock initialised.\n";

    // Setup tank configuration
    string tankName;
    float tankVolume, tankLevel, tankMinVolume;
    
    cout << "Enter tank name: ";
    getline(cin, tankName);
    
    cout << "Enter tank volume (liters): ";
    cin >> tankVolume;
    
    cout << "Enter current tank level (%): ";
    cin >> tankLevel;
    
    cout << "Enter minimum safe volume (liters): ";
    cin >> tankMinVolume;
    
    cin.ignore(); // Clear the newline from the input buffer
    
    Tangki tank(tankName, tankVolume, tankLevel, tankMinVolume);
    
    cout << "\nTank configuration:\n";
    cout << "- Name: " << tank.Name << "\n";
    cout << "- Volume: " << tank.Volume << " liters\n";
    cout << "- Current level: " << tank.Level << "%\n";
    cout << "- Minimum safe volume: " << tank.MinVolume << " liters\n\n";

    while (true) {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == INVALID_SOCKET) {
            cerr << "Could not create socket. Error code: " << WSAGetLastError() << "\n";
            WSACleanup();
            return 1;
        }
        
        server.sin_addr.s_addr = inet_addr(SERVER);
        server.sin_family = AF_INET;
        server.sin_port = htons(PORT);

        // Simulate level changes
        simulateLevelChanges(tank);
        
        // Try to connect to server
        if (clientSide.clientConnect(client_socket, server)) {
            // Format message with timestamp, tank name, volume, level, and min volume
            string message = getCurrentTimestamp() + "," + tank.toString();
            
            // Send tank status to server
            clientSide.clientSend(client_socket, message);
            
            // Receive response from server
            clientSide.clientRecv(client_socket);
            
            closesocket(client_socket);
        } else {
            cout << "Could not connect to server. Will retry in 5 seconds...\n";
        }
        
        // Wait before sending next update
        this_thread::sleep_for(chrono::seconds(5));
    }
    
    WSACleanup();
    return 0;
}
