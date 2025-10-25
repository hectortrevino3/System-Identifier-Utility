#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <random>
#include <locale>
#include <codecvt>
#include "skStr.h"
#include "auth.hpp"
#include "utils.hpp"

#define IDR_VOLUMEID64 101
#define IDR_AMIDEWINX64 102
#define IDR_AMIFLDRV64 103
#define IDR_AMIGENDRV64 104

using namespace KeyAuth;

std::string name = skCrypt("System-Identifier-Utility").decrypt(); // Application Name
std::string ownerid = skCrypt("LVGF1OzrWM").decrypt(); // Owner ID
std::string secret = skCrypt("9d4070b7b4dcba3f3ae65d326b60a14ec45fa9ac015447fd390feb952360cc4c").decrypt(); // Application Secret
std::string version = skCrypt("1.0").decrypt(); // Application Version
std::string url = skCrypt("https://keyauth.win/api/1.2/").decrypt(); // change if you're self-hosting
std::string path = skCrypt("").decrypt();

api KeyAuthApp(name, ownerid, secret, version, url, path);

std::wstring StringToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string WStringToString(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void SetTitle() {
    SetConsoleTitle(L"System-Identifier-Utility");
}

void ClearConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD written;

    GetConsoleScreenBufferInfo(hConsole, &csbi);

    COORD coord = { 0, 0 };
    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', size, coord, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, size, coord, &written);

    SetConsoleCursorPosition(hConsole, coord);
}

std::string RunCommand(const std::string& command, bool& errorDetected) {
    std::string result;
    char buffer[128];
    std::string fullCommand = command + " 2>&1";

    FILE* pipe = _popen(fullCommand.c_str(), "r");
    if (!pipe) {
        errorDetected = true;
        return "popen() failed!";
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int returnCode = _pclose(pipe);
    errorDetected = (returnCode != 0);

    return result;
}

std::string RunCommand(const std::string& command) {
    bool errorDetected = false;
    return RunCommand(command, errorDetected);
}

std::string GenerateRandomSerial() {
    const std::string characters = "C1856";
    const int length = 8;
    std::string randomSerial;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    for (int i = 0; i < length; ++i) {
        randomSerial += characters[distribution(generator)];
    }

    randomSerial.insert(4, "-");

    return randomSerial;
}

std::string ExtractVolumeSerialNumber(const std::string& volOutput) {
    std::string serialNumber;
    std::istringstream iss(volOutput);
    std::string line;
    bool foundSerialNumber = false;

    while (std::getline(iss, line)) {
        if (line.find("Volume Serial Number is") != std::string::npos) {
            foundSerialNumber = true;
            size_t pos = line.find("Volume Serial Number is") + std::string("Volume Serial Number is").length();
            serialNumber = line.substr(pos);
            serialNumber.erase(0, serialNumber.find_first_not_of(" \t"));
            serialNumber.erase(serialNumber.find_last_not_of(" \t") + 1);
            break;
        }
    }

    if (!foundSerialNumber) {
        serialNumber = "Serial number not found or access denied. (will not affect changing)";
    }

    return serialNumber;
}

// *** New helper function to de-duplicate code ***
std::vector<std::string> GetDriveLetters() {
    std::string driveListOutput = RunCommand("wmic logicaldisk get deviceid");
    std::istringstream iss(driveListOutput);
    std::vector<std::string> drives;
    std::string line;

    std::getline(iss, line); // Skip header

    while (std::getline(iss, line)) {
        line.erase(line.find_last_not_of(" \n\r\t:") + 1);
        if (!line.empty()) {
            drives.push_back(line);
        }
    }
    return drives;
}

bool ExtractResource(int resourceId, const std::string& filename) {
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) {
        std::cerr << "FindResource failed for resource ID: " << resourceId << std::endl;
        return false;
    }

    HGLOBAL hResData = LoadResource(nullptr, hRes);
    if (!hResData) {
        std::cerr << "LoadResource failed for resource ID: " << resourceId << std::endl;
        return false;
    }

    void* pResData = LockResource(hResData);
    if (!pResData) {
        std::cerr << "LockResource failed for resource ID: " << resourceId << std::endl;
        return false;
    }

    DWORD resSize = SizeofResource(nullptr, hRes);
    if (resSize == 0) {
        std::cerr << "SizeofResource failed for resource ID: " << resourceId << std::endl;
        return false;
    }

    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    outFile.write(static_cast<const char*>(pResData), resSize);
    if (!outFile.good()) {
        std::cerr << "Failed to write data to file: " << filename << std::endl;
        return false;
    }

    return true;
}

void CheckSerials() {
    ClearConsole();
    std::cout << "Checking current serials...\n" << std::endl;

    std::vector<std::string> drives = GetDriveLetters();

    std::string allSerials;
    for (const std::string& driveLetter : drives) {
        std::string command = "vol " + driveLetter + ":";
        std::string volOutput = RunCommand(command);
        std::string serialNumber = ExtractVolumeSerialNumber(volOutput);
        allSerials += "Drive " + driveLetter + ": " + serialNumber + "\n";
    }

    std::cout << "-Volume Serial Numbers-\n" << std::endl;
    std::cout << allSerials << std::endl;

    std::cout << "-SMBIOS Serial Numbers-\n" << std::endl;
    std::cout << "Baseboard " + RunCommand("wmic baseboard get serialnumber") << std::endl;
    std::cout << "Bios " + RunCommand("wmic bios get serialnumber") << std::endl;
    std::cout << RunCommand("wmic path win32_computersystemproduct get uuid") << std::endl;

    std::cout << "Press enter to continue...";
    std::cin.get();
    ClearConsole();
}

void ChangeSerials() {
    ClearConsole();
    std::cout << "Changing serials...\n\n" << std::endl;

    const std::string volumeIdPath = "Volumeid64.exe";
    const std::string amidewinPath = "AMIDEWINx64.exe";
    const std::string amifldrvPath = "AMIFLDRV64.sys";
    const std::string amigendrvPath = "AMIGENDRV64.sys";

    bool anyError = false;

    if (!ExtractResource(IDR_VOLUMEID64, volumeIdPath)) {
        std::cerr << "Failed to extract resource (Error Code: 1)" << std::endl;
        anyError = true;
        return;
    }

    if (!ExtractResource(IDR_AMIDEWINX64, amidewinPath)) {
        std::cerr << "Failed to extract resource (Error Code: 2)" << std::endl;
        anyError = true;
        return;
    }

    if (!ExtractResource(IDR_AMIFLDRV64, amifldrvPath)) {
        std::cerr << "Failed to extract driver (Error Code: 3)" << std::endl;
        anyError = true;
        return;
    }

    if (!ExtractResource(IDR_AMIGENDRV64, amigendrvPath)) {
        std::cerr << "Failed to extract driver (Error Code: 4)" << std::endl;
        anyError = true;
        return;
    }

    bool errorDetected = false;

    RunCommand("AMIDEWINx64.exe /SU auto", errorDetected);
    if (errorDetected) {
        std::cerr << "Failed to change SMBIOS UUID (Error Code: 5)" << std::endl;
        anyError = true;
    }

    RunCommand("AMIDEWINx64.exe /BS \"                        \"", errorDetected);
    if (errorDetected) {
        std::cerr << "Failed to change SMBIOS baseboard serial number (Error Code: 6)" << std::endl;
        anyError = true;
    }

    RunCommand("AMIDEWINx64.exe /SS \"Default string\"", errorDetected);
    if (errorDetected) {
        std::cerr << "Failed to change BIOS serial number (Error Code: 7)" << std::endl;
        anyError = true;
    }

    RunCommand("Volumeidx64.exe /accepteula");

    std::vector<std::string> drives = GetDriveLetters();

    for (const std::string& driveLetter : drives) {
        std::string newSerial = GenerateRandomSerial();
        std::string volumeIdCommand = "Volumeid64.exe " + driveLetter + ": " + newSerial;
        RunCommand(volumeIdCommand, errorDetected);
        if (errorDetected) {
            std::cerr << "Failed to change drive " << driveLetter << ": (Error Code: 8)" << std::endl;
            anyError = true;
        }
    }

    DeleteFile(StringToWString(volumeIdPath).c_str());
    DeleteFile(StringToWString(amidewinPath).c_str());
    DeleteFile(StringToWString(amifldrvPath).c_str());
    DeleteFile(StringToWString(amigendrvPath).c_str());

    if (!anyError) {
        std::cout << "\n\nSuccessfully changed serials\n" << std::endl;
    }
    else {
        std::cout << "\n\nSome errors occurred during changing. Ensure you are running this software as an Administrator. \nIf the issue persists, open a ticket.\n\n" << std::endl;
    }
    std::cout << "Press enter to continue...";
    std::cin.get();
    ClearConsole();
}

int main() {
    name.clear(); ownerid.clear(); secret.clear(); version.clear(); url.clear();
    std::string consoleTitle = skCrypt("System-Identifier-Utility").decrypt();
    SetConsoleTitleA(consoleTitle.c_str());
    std::cout << skCrypt("\n\n Connecting..");
    KeyAuthApp.init();
    if (!KeyAuthApp.response.success) {
        std::cout << skCrypt("\n Status: ") << KeyAuthApp.response.message;
        Sleep(1500);
        return 1;
    }

    std::string key;
    std::cout << skCrypt("\n Enter license: ");
    std::cin >> key;

    KeyAuthApp.license(key);

    if (!KeyAuthApp.response.success) {
        std::cout << skCrypt("\n Status: ") << KeyAuthApp.response.message;
        Sleep(1500);
        return 1;
    }
    std::cin.ignore();
    {
        SetTitle();
        while (true) {
            ClearConsole();
            std::cout << "System-Identifier-Utility\n" << std::endl;
            std::cout << "1. Check Serials" << std::endl;
            std::cout << "2. Change Serials" << std::endl;
            std::cout << "3. Exit\n" << std::endl;
            std::cout << "Choose an option: ";

            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1") {
                CheckSerials();
            }
            else if (choice == "2") {
                ChangeSerials();
            }
            else if (choice == "3") {
                break;
            }
            else {
                ClearConsole();
                std::cout << "Invalid option. Please try again.\n" << std::endl;
                std::cout << "Press enter to continue...";
                std::cin.get();
            }
        }

        return 0;
    }
}