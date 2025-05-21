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
#include <mutex>
#include <sstream>
#include <winsock2.h>
#include <map>
#include <queue>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

#define PORT 8890
#define BACKUP_INTERVAL 60 // Seconds between backups
#define JSON_EXPORT_INTERVAL 300 // Seconds between JSON exports

// Define tank actions
enum TangkiAction {
    NO_ACTION,
    REFILL,
    DRAIN
};

// Tank class definition
class Tangki {
public:
    string Name;
    float Volume;
    float Level;
    float MinVolume;
    chrono::system_clock::time_point LastUpdated;
    vector<pair<chrono::system_clock::time_point, TangkiAction>> ActionHistory;

    Tangki(const string& name = "", float volume = 0, float level = 0, float minVolume = 0) {
        Name = name;
        Volume = volume;
        Level = level;
        MinVolume = minVolume;
        LastUpdated = chrono::system_clock::now();
    }
    
    // Calculate effective volume
    float getEffectiveVolume() const {
        return Volume * (Level / 100.0f);
    }
    
    // Calculate if tank is in critical state
    TangkiAction checkCriticalLevel() const {
        if (getEffectiveVolume() < MinVolume) {
            return REFILL;
        } else if (Level > 100.0f) {
            return DRAIN;
        }
        return NO_ACTION;
    }
    
    // Update tank data with received information
    void update(float newVolume, float newLevel, float newMinVolume) {
        Volume = newVolume;
        Level = newLevel;
        MinVolume = newMinVolume;
        LastUpdated = chrono::system_clock::now();
        
        // Check if action needed and record it
        TangkiAction action = checkCriticalLevel();
        if (action != NO_ACTION) {
            ActionHistory.push_back(make_pair(LastUpdated, action));
        }
    }
    
    // Convert to JSON format
    json toJson() const {
        json j;
        j["name"] = Name;
        j["volume"] = Volume;
        j["level"] = Level;
        j["minVolume"] = MinVolume;
        j["effectiveVolume"] = getEffectiveVolume();
        j["lastUpdated"] = chrono::system_clock::to_time_t(LastUpdated);
        
        json actions = json::array();
        for (const auto& action : ActionHistory) {
            json actionJson;
            actionJson["timestamp"] = chrono::system_clock::to_time_t(action.first);
            actionJson["action"] = (action.second == REFILL) ? "REFILL" : "DRAIN";
            actions.push_back(actionJson);
        }
        j["actions"] = actions;
        
        return j;
    }
};

// Function to convert TangkiAction to string
string tangkiActionToString(TangkiAction action) {
    switch (action) {
        case REFILL: return "REFILL";
        case DRAIN: return "DRAIN";
        default: return "NO_ACTION";
    }
}

// TCP Server implementation
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
    
        listen(listen_socket, SOMAXCONN);
        cout << "Waiting for incoming connections...\n";
        return true;
    }
    
    int acceptClient(SOCKET listen_socket, SOCKET &client_socket, sockaddr_in &client) {
        int c = sizeof(struct sockaddr_in);
        client_socket = accept(listen_socket, (struct sockaddr*)&client, &c);
        if (client_socket == INVALID_SOCKET) {
            cerr << "Accept failed. Error code: " << WSAGetLastError() << "\n";
            return 1;
        }
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client.sin_addr), clientIP, INET_ADDRSTRLEN);
        cout << "New connection from " << clientIP << ":" << ntohs(client.sin_port) << "\n";
        
        return 0;
    }
    
    int serverSend(SOCKET client_socket, const string& message) {
        send(client_socket, message.c_str(), message.length(), 0);
        return 0;
    }
    
    string serverRecv(SOCKET client_socket) {
        char messageRecv[1024];
        int recv_size = recv(client_socket, messageRecv, sizeof(messageRecv) - 1, 0);
        if(recv_size == SOCKET_ERROR) {
            cout << "Failed to receive message. Error code: " <<  WSAGetLastError() << "\n";
            return "";
        }
        else if (recv_size == 0) {
            return "";
        }
        else {
            messageRecv[recv_size] = '\0';
            cout << "Received: " << messageRecv << "\n";
            return string(messageRecv);
        }
    }
};

// Global data structures
map<string, Tangki> tanks;
mutex tanksMutex;
bool shutdownServer = false;

// Function to process received data and update tank information
void processClientData(const string& data) {
    stringstream ss(data);
    string timestamp, tankName;
    float volume, level, minVolume;
    
    getline(ss, timestamp, ',');
    getline(ss, tankName, ',');
    ss >> volume;
    ss.ignore(); // Skip comma
    ss >> level;
    ss.ignore(); // Skip comma
    ss >> minVolume;
    
    lock_guard<mutex> lock(tanksMutex);
    
    // Update or create tank entry
    if (tanks.find(tankName) == tanks.end()) {
        tanks[tankName] = Tangki(tankName, volume, level, minVolume);
        cout << "New tank registered: " << tankName << "\n";
    } else {
        tanks[tankName].update(volume, level, minVolume);
    }
    
    // Check for critical status
    TangkiAction action = tanks[tankName].checkCriticalLevel();
    if (action != NO_ACTION) {
        cout << "ALERT: Tank " << tankName << " requires " << tangkiActionToString(action) 
             << "! Current effective volume: " << tanks[tankName].getEffectiveVolume() 
             << "L, Minimum safe volume: " << minVolume << "L\n";
    }
}

// Function to generate response based on tank status
string generateResponse(const string& tankName) {
    lock_guard<mutex> lock(tanksMutex);
    
    if (tanks.find(tankName) == tanks.end()) {
        return "ERROR: Tank not found";
    }
    
    const Tangki& tank = tanks[tankName];
    TangkiAction action = tank.checkCriticalLevel();
    
    stringstream response;
    response << "STATUS:" << tangkiActionToString(action);
    
    return response.str();
}

// Function to handle client connection in a separate thread
void handleClient(SOCKET client_socket) {
    TcpServer server;
    
    // Receive data from client
    string received = server.serverRecv(client_socket);
    if (!received.empty()) {
        // Extract tank name for response
        stringstream ss(received);
        string timestamp, tankName;
        
        getline(ss, timestamp, ',');
        getline(ss, tankName, ',');
        
        // Process the data
        processClientData(received);
        
        // Generate and send response
        string response = generateResponse(tankName);
        server.serverSend(client_socket, response);
    }
    
    closesocket(client_socket);
}

// Function to save data to binary file periodically
void backupData() {
    while (!shutdownServer) {
        this_thread::sleep_for(chrono::seconds(BACKUP_INTERVAL));
        
        lock_guard<mutex> lock(tanksMutex);
        if (tanks.empty()) {
            continue;
        }
        
        ofstream file("tank_data.bin", ios::binary);
        if (!file) {
            cerr << "Error opening backup file\n";
            continue;
        }
        
        // Write number of tanks
        size_t numTanks = tanks.size();
        file.write(reinterpret_cast<char*>(&numTanks), sizeof(numTanks));
        
        // Write tank data
        for (const auto& pair : tanks) {
            const Tangki& tank = pair.second;
            
            // Write name length and name
            size_t nameLen = tank.Name.length();
            file.write(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            file.write(tank.Name.c_str(), nameLen);
            
            // Write tank properties
            file.write(reinterpret_cast<const char*>(&tank.Volume), sizeof(tank.Volume));
            file.write(reinterpret_cast<const char*>(&tank.Level), sizeof(tank.Level));
            file.write(reinterpret_cast<const char*>(&tank.MinVolume), sizeof(tank.MinVolume));
            
            // Write timestamp
            auto timepoint = chrono::system_clock::to_time_t(tank.LastUpdated);
            file.write(reinterpret_cast<char*>(&timepoint), sizeof(timepoint));
            
            // Write action history size
            size_t actionSize = tank.ActionHistory.size();
            file.write(reinterpret_cast<char*>(&actionSize), sizeof(actionSize));
            
            // Write action history
            for (const auto& action : tank.ActionHistory) {
                auto actionTime = chrono::system_clock::to_time_t(action.first);
                file.write(reinterpret_cast<char*>(&actionTime), sizeof(actionTime));
                file.write(reinterpret_cast<const char*>(&action.second), sizeof(action.second));
            }
        }
        
        file.close();
        cout << "Data backup completed\n";
    }
}

// Function to export critical events to JSON
void exportJsonData() {
    while (!shutdownServer) {
        this_thread::sleep_for(chrono::seconds(JSON_EXPORT_INTERVAL));
        
        lock_guard<mutex> lock(tanksMutex);
        if (tanks.empty()) {
            continue;
        }
        
        // Create JSON array for tanks
        json tanksJson = json::array();
        
        for (const auto& pair : tanks) {
            tanksJson.push_back(pair.second.toJson());
        }
        
        // Create filename with timestamp
        auto now = chrono::system_clock::now();
        auto time_t_now = chrono::system_clock::to_time_t(now);
        stringstream ss;
        tm tm_now;
        localtime_s(&tm_now, &time_t_now);
        ss << "tank_status_" << put_time(&tm_now, "%Y%m%d_%H%M%S") << ".json";
        
        // Write to file
        ofstream file(ss.str());
        if (file) {
            file << tanksJson.dump(4); // Pretty print with 4 space indent
            file.close();
            cout << "JSON export completed: " << ss.str() << "\n";
        } else {
            cerr << "Error exporting JSON data\n";
        }
    }
}

int main() {
    WSADATA wsa;
    SOCKET listen_socket;
    struct sockaddr_in server;
    TcpServer serverSide;
    vector<thread> clientThreads;

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

    if (!serverSide.bindListen(listen_socket, server)) {
        cerr << "Failed to bind and listen\n";
        return 1;
    }
    
    // Start backup thread
    thread backupThread(backupData);
    backupThread.detach();
    
    // Start JSON export thread
    thread jsonExportThread(exportJsonData);
    jsonExportThread.detach();
    
    cout << "Tank Monitoring Server is running on port " << PORT << "\n";
    cout << "Press Ctrl+C to stop the server\n";
    
    // Main server loop to accept connections
    while (!shutdownServer) {
        SOCKET client_socket;
        sockaddr_in client;
        
        if (serverSide.acceptClient(listen_socket, client_socket, client) == 0) {
            thread clientThread(handleClient, client_socket);
            clientThread.detach();
        }
    }

    closesocket(listen_socket);
    WSACleanup();
    
    return 0;
}
