#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <array>
#include <thread>
#include <atomic>
#include <cstdlib>

namespace localPlayer {
    std::vector<uint32_t> basePtrOffset = {0x1097438, 0x0}; 

    std::vector<uint32_t> camXOffset = { 0x10, 0x120, 0x70 };
    std::vector<uint32_t> camYOffset = { 0x10, 0x120, 0x74 };
    std::vector<uint32_t> camZOffset = { 0x10, 0x120, 0x78 };

    std::vector<uint32_t> CoordOffsets = {0x0, 0x28, 0xE8, 0x4, 0x0};

    std::vector<uint32_t> xOffsets = {0x80};
    std::vector<uint32_t> yOffsets = {0x84};
    std::vector<uint32_t> zOffsets = {0x88};
}

namespace World {
    std::vector<uint32_t> worldOffset = {0x1097484, 0x0};

    std::vector<uint32_t> NodeInfoOffsets = {0x7C};

    std::vector<uint32_t> baseAddressOffsets = {0x0};
    std::vector<uint32_t> stepOffsets = {0x4};
    std::vector<uint32_t> sizeOffsets = {0x8};

    std::vector<uint32_t> EntityOffsets = {0x10, 0xE8, 0x4, 0x0};
    std::vector<uint32_t> levelOffsets = {0x58, 0xE8, 0x54, 0x120};
    std::vector<uint32_t> nameOffsets = {0x58, 0x64, 0x0};
    std::vector<uint32_t> isDeathOffsets = {0x58, 0x0};
    std::vector<uint32_t> healthOffsets = {0x58, 0xE8, 0x84, 0x80};
    std::vector<uint32_t> xOffsets = {0x58, 0xE8, 0x4, 0x80};
    std::vector<uint32_t> yOffsets = {0x58, 0xE8, 0x4, 0x84};
    std::vector<uint32_t> zOffsets = {0x58, 0xE8, 0x4, 0x88};
}

// Store original instructions for restoration
std::array<std::vector<BYTE>, 3> originalInstructions;
std::array<uintptr_t, 3> movssOffsets = {
    0x2d7133, // Replace with actual offset 1
    0x2d7169, // Replace with actual offset 2
    0x2d719f  // Replace with actual offset 3
};
std::array<uintptr_t, 3> instructionAddresses;
std::array<LPVOID, 3> addresses;
uint32_t gameAddress;
HANDLE hProcess = nullptr;

std::atomic<bool> keepRunning{true};

struct vec3 {
    float x, y, z;

    vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

	vec3 operator-(vec3 b) {
		return { x - b.x, y - b.y, z - b.z };
	}

	vec3 operator+(vec3 b) {
		return { x + b.x, y + b.y, z + b.z };
	}

	bool isNotZero() {
		return 
			floor(x) != 0.0 &&
			floor(y) != 0.0 &&
			floor(z) != 0.0;
	}

    float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	vec3 normalized() const {
		float len = length();
		if (len == 0.f) return vec3();
		return vec3(x / len, y / len, z / len);
	}
};

vec3 GetForwardVector(vec3 &pos1, vec3 &pos2) {
	auto direction = pos2 - pos1;
	return direction.normalized();
}

void cleanup()
{
    if (hProcess) {
        // Restore original instructions
        for (int i = 0; i < 3; ++i) {
            if (!originalInstructions[i].empty()) {
                SIZE_T bytesWritten = 0;
                WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(instructionAddresses[i]),
                                   originalInstructions[i].data(), originalInstructions[i].size(), &bytesWritten);
            }
        }
        std::cout << originalInstructions.size() << std::endl;
        // Free allocated floats
        for (int i = 0; i < 3; ++i) {
            if (addresses[i]) {
                VirtualFreeEx(hProcess, addresses[i], 0, MEM_RELEASE);
            }
        }
        CloseHandle(hProcess);
        std::cout << "Cleanup done.\n";
    }
}

uint32_t GetAddress(const uint32_t &base, const std::vector<uint32_t> &offsets)
{
    if (offsets.empty())
        return base;
    uint32_t address = base + offsets[0];
    for (size_t i = 1; i < offsets.size(); ++i)
    {
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address),
                               &address, sizeof(address), nullptr))
            return 0;
        address += offsets[i];
    }
    return address;
}

template <typename T>
T Read(const uint32_t &address)
{
    T value = T();
    ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
    return value;
}

std::string ReadStr(const uint32_t &address, const size_t &maxLen)
{
    std::string str;
    char c;
    while (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address + str.size()), &c, 1, nullptr) &&
           c != '\0' && str.size() < maxLen)
        str += c;
    return str;
}

void Aimbot(HANDLE hProcess, const uint32_t &gameAddr, const std::array<LPVOID, 3>& addresses) {
    //printf("Running aimbot\n");
    auto lpAddr = GetAddress(gameAddr, localPlayer::basePtrOffset);
    auto world = GetAddress(gameAddr, World::worldOffset);

    auto x = Read<float>(GetAddress(lpAddr, localPlayer::camXOffset));
    auto y = Read<float>(GetAddress(lpAddr, localPlayer::camYOffset));
    auto z = Read<float>(GetAddress(lpAddr, localPlayer::camZOffset));

    auto coordAddr = GetAddress(lpAddr, localPlayer::CoordOffsets);

    auto px = Read<float>(GetAddress(coordAddr, localPlayer::xOffsets));
    auto py = Read<float>(GetAddress(coordAddr, localPlayer::yOffsets));
    auto pz = Read<float>(GetAddress(coordAddr, localPlayer::zOffsets));

    vec3 playerPos(px, py, pz);

    std::vector<uint32_t> nodes;

    float nearestRange = 45.f;
    vec3 target;
    std::string targetName = "none";

    const auto nodeInfo = GetAddress(world, World::NodeInfoOffsets);

    //printf("nodeInfo address: %08X\n", nodeInfo);
    const auto baseAddr = Read<uint32_t>(GetAddress(nodeInfo, World::baseAddressOffsets));
    const auto size = Read<uint32_t>(GetAddress(nodeInfo, World::sizeOffsets));
    const auto step = Read<uint32_t>(GetAddress(nodeInfo, World::stepOffsets));

    for (uint32_t i = 0; i < size; i++)
    {
        uint32_t address = baseAddr + i * step;
        uint32_t addressNext;
        do
        {
            addressNext = Read<uint32_t>(address);
            
            if (addressNext != 1) {
                //printf("nodes NODE UPDATE: %08X ADDRESS NEXT: %08X STEP: %d\n", address, addressNext, step);
                nodes.emplace_back(address);
            }
            address = addressNext & 0xFFFFFFFE;
        } while ((addressNext & 0xFFFFFFFE) != 0);
    }

    for (const auto& node : nodes) {
        const auto entity = GetAddress(node, World::EntityOffsets);
        const auto nameAddr = GetAddress(entity, World::nameOffsets);
        //printf("%08X Name address: %08X\n", entity, nameAddr);

        if (nameAddr != 0) {
            const auto name2 = ReadStr(nameAddr, 128);

            if (name2.length() > 0) {
                auto entityX = Read<float>(GetAddress(entity, World::xOffsets));
                auto entityY = Read<float>(GetAddress(entity, World::yOffsets));
                auto entityZ = Read<float>(GetAddress(entity, World::zOffsets));

                vec3 entityPos (entityX, entityY, entityZ);

                auto delta = playerPos - entityPos;
				auto dist = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

                if (
                    dist < nearestRange &&
                    name2.find("pet") == std::string::npos &&
                    name2.find("portal") == std::string::npos &&
                    name2.find("abilities") == std::string::npos &&
                    name2.find("placeable") == std::string::npos &&
                    name2.find("cornerstone") == std::string::npos &&
                    name2.find("services") == std::string::npos &&
                    name2.find("client") == std::string::npos &&
                    name2.find("mana") == std::string::npos &&
                    name2.find("karma") == std::string::npos
                    ) {
                    nearestRange = dist;
                    target = entityPos;
                    targetName = name2;
                }
            }
        }
    }

    printf("target dist: %.0f target name: %s\r", nearestRange, targetName.c_str());
    //printf("player pos: %.0f %.0f %.0f\n", playerPos.x, playerPos.y, playerPos.z);
    vec3 camPos(x, y, z);

    vec3 forward = GetForwardVector(camPos, target);
    std::array<float, 3> forwardArray = {forward.x, forward.y, forward.z};

    for (int i = 0; i < 3; ++i) {   
        float value = -forwardArray[i];
        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(hProcess, addresses[i], &value, sizeof(float), &bytesWritten) || bytesWritten != sizeof(float)) {
            std::cerr << "Failed to write float to remote process.\n";
        } else {
            //std::cout << "Wrote " << value << " to address " << addresses[i] << "\n";
        }
    }

    //printf("end aimbot\n");
}

/**
 * @brief Find the process ID by process name.
 * @param processName The name of the process (e.g., "target.exe").
 * @return The process ID, or 0 if not found.
 */
DWORD findProcessId(const std::string& processName)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    DWORD pid = 0;
    if (Process32First(snapshot, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

/**
 * @brief Find the base address of a module in a process.
 * @param pid The process ID.
 * @param moduleName The name of the module (e.g., "target.exe").
 * @return The base address, or 0 if not found.
 */
uintptr_t findModuleBaseAddress(DWORD pid, const std::string& moduleName)
{
    MODULEENTRY32 entry;
    entry.dwSize = sizeof(MODULEENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    uintptr_t baseAddress = 0;
    if (Module32First(snapshot, &entry)) {
        do {
            if (moduleName == entry.szModule) {
                baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return baseAddress;
}

/**
 * @brief Allocate memory for 3 floats in the target process.
 * @param hProcess Handle to the target process.
 * @return Array of 3 addresses.
 */
std::array<LPVOID, 3> allocateRemoteFloats(HANDLE hProcess)
{
    std::array<LPVOID, 3> addresses{};
    for (int i = 0; i < 3; ++i) {
        addresses[i] = VirtualAllocEx(hProcess, nullptr, sizeof(float), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!addresses[i]) {
            std::cerr << "Failed to allocate memory in target process.\n";
            exit(1);
        } else {
            std::cout << "address: " << addresses[i] << std::endl;
        }
    }
    return addresses;
}

/**
 * @brief Patch the instruction "movss xmm0, [address]" at a given address.
 * @param hProcess Handle to the target process.
 * @param instructionAddress Address of the instruction to patch.
 * @param newAddress The new address to use in the instruction.
 */
void patchMovssInstruction(HANDLE hProcess, uintptr_t instructionAddress, LPVOID newAddress)
{
    // movss xmm0, [address] = F3 0F 10 05 xx xx xx xx (RIP-relative)
    // We'll assume the instruction is at instructionAddress and is 7 bytes.
    // We'll patch the address part (last 4 bytes).
    // For simplicity, we assume absolute addressing: movss xmm0, [abs address] = F3 0F 10 05 xx xx xx xx
    // If the instruction uses RIP-relative, more work is needed.

    BYTE patch[8];
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(instructionAddress), patch, 8, &bytesRead) || bytesRead != 8) {
        std::cerr << "Failed to read instruction at " << std::hex << instructionAddress << "\n";
        exit(1);
    }

    // Patch the address (bytes 3-6)
    uintptr_t addr = reinterpret_cast<uintptr_t>(newAddress);
    patch[3] = static_cast<BYTE>(0x05);

    patch[4] = static_cast<BYTE>(addr & 0xFF);
    patch[5] = static_cast<BYTE>((addr >> 8) & 0xFF);
    patch[6] = static_cast<BYTE>((addr >> 16) & 0xFF);
    patch[7] = static_cast<BYTE>((addr >> 24) & 0xFF);

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(instructionAddress), patch, 8, &bytesWritten) || bytesWritten != 8) {
        std::cerr << "Failed to patch instruction at " << std::hex << instructionAddress << "\n";
        exit(1);
    }
}

void AimbotThreadFunc()
{
    while (keepRunning.load()) {
        Aimbot(hProcess, gameAddress, addresses);
        // Optionally add a small sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cleanup();
}

void onExit() {
    cleanup();
    keepRunning.store(false); 
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        printf("Ctrl-C event\n\n");
        Beep(750, 300);
        return TRUE;

        // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
        //Beep(600, 200);
        onExit();
        printf("Ctrl-Close event\n\n");
        return TRUE;

        // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
        Beep(900, 200);
        printf("Ctrl-Break event\n\n");
        return FALSE;

    case CTRL_LOGOFF_EVENT:
        Beep(1000, 200);
        printf("Ctrl-Logoff event\n\n");
        return FALSE;

    case CTRL_SHUTDOWN_EVENT:
        Beep(750, 500);
        printf("Ctrl-Shutdown event\n\n");
        return FALSE;

    default:
        return FALSE;
    }
}

int main()
{
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    std::string processName = "Trove.exe"; // Change to your target process
    std::string moduleName = "Trove.exe";  // Change to your target module

    DWORD pid = findProcessId(processName);
    if (!pid) {
        std::cerr << "Process not found.\n";
        return 1;
    }
    std::cout << "Found PID: " << pid << "\n";

    uintptr_t baseAddress = findModuleBaseAddress(pid, moduleName);
    if (!baseAddress) {
        std::cerr << "Module not found.\n";
        return 1;
    }
    std::cout << "Module base address: 0x" << std::hex << baseAddress << "\n";
    gameAddress = baseAddress;

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "Failed to open process.\n";
        return 1;
    }

    addresses = allocateRemoteFloats(hProcess);

    // Save original instructions and patch
    for (int i = 0; i < 3; ++i) {
        instructionAddresses[i] = baseAddress + movssOffsets[i];
        // Read original instruction (8 bytes)
        std::vector<BYTE> orig(8);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(instructionAddresses[i]), orig.data(), 8, &bytesRead) || bytesRead != 8) {
            std::cerr << "Failed to read original instruction at " << std::hex << instructionAddresses[i] << "\n";
            cleanup();
            return 1;
        }
        originalInstructions[i] = orig;
        patchMovssInstruction(hProcess, instructionAddresses[i], addresses[i]);
    }

        // Start the Aimbot in a new thread
    new std::thread (AimbotThreadFunc);

    //Aimbot(hProcess, gameAddress, addresses);

    std::atexit(onExit);
    getchar();

    return 0;
}