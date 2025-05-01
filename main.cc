#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <array>
#include <cstdlib>

#include "Vec3.hh"
#include "Utils.hh"

uint32_t gameAddr;

namespace LocalPlayer {
    const std::vector<uint32_t> basePtrOffset = {0x1097438, 0x0}; 

    const std::vector<uint32_t> camXOffset = { 0x20, 0x70 };
    const std::vector<uint32_t> camYOffset = { 0x20, 0x74 };
    const std::vector<uint32_t> camZOffset = { 0x20, 0x78 };

    const std::vector<uint32_t> camForwardXOffset = { 0x20, 0x60 };
    const std::vector<uint32_t> camForwardYOffset = { 0x20, 0x64 };
    const std::vector<uint32_t> camForwardZOffset = { 0x20, 0x68 };    

    const std::vector<uint32_t> CoordOffsets = {0x0, 0x28, 0xE8, 0x4, 0x0};
    const std::vector<uint32_t> xOffsets = {0x80};
    const std::vector<uint32_t> yOffsets = {0x84};
    const std::vector<uint32_t> zOffsets = {0x88};
}

namespace World {
    const std::vector<uint32_t> worldOffset = {0x1097484, 0x0};

    const std::vector<uint32_t> NodeInfoOffsets = {0x7C};
    const std::vector<uint32_t> baseAddressOffsets = {0x0};
    const std::vector<uint32_t> stepOffsets = {0x4};
    const std::vector<uint32_t> sizeOffsets = {0x8};

    const std::vector<uint32_t> EntityOffsets = {0x10, 0xE8, 0x4, 0x0};
    const std::vector<uint32_t> levelOffsets = {0x58, 0xE8, 0x54, 0x120};
    const std::vector<uint32_t> nameOffsets = {0x58, 0x64, 0x0};
    const std::vector<uint32_t> isDeathOffsets = {0x58, 0x0};
    const std::vector<uint32_t> healthOffsets = {0x58, 0xE8, 0x84, 0x80};
    const std::vector<uint32_t> xOffsets = {0x58, 0xE8, 0x4, 0x80};
    const std::vector<uint32_t> yOffsets = {0x58, 0xE8, 0x4, 0x84};
    const std::vector<uint32_t> zOffsets = {0x58, 0xE8, 0x4, 0x88};
}

struct Entity {
    std::string name;
    vec3 position;
    float range;

    Entity(const std::string& name2 = "", const vec3& position2 = vec3(), const float& range2 = -1.f) {
        name = name2;
        position = position2;
        range = range2;
    }
};

// hardcoded
const std::array<uintptr_t, 3> movssOffsets = {
    0x2d7133, // Replace with actual offset 1
    0x2d7169, // Replace with actual offset 2
    0x2d719f  // Replace with actual offset 3
};

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

const Entity NearestEntity(const uint32_t &world, const vec3 &ourPos, const float &range) {
    std::vector<uint32_t> nodes;
    Entity ret;

    float nearestRange = range;

    const auto nodeInfo = GetAddress(world, World::NodeInfoOffsets);

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
            const auto name2 = ReadStr(nameAddr, 96);

            if (name2.length() > 0) {
                auto entityX = Read<float>(GetAddress(entity, World::xOffsets));
                auto entityY = Read<float>(GetAddress(entity, World::yOffsets));
                auto entityZ = Read<float>(GetAddress(entity, World::zOffsets));

                vec3 entityPos (entityX, entityY, entityZ);

                auto delta = entityPos - ourPos;
				auto dist = delta.length();

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
                    ret.position = entityPos;
                    ret.name = name2;
                    ret.range = dist;
                }
            }
        }
    }

    return ret;
}

void Aimbot(const std::array<LPVOID, 3>& addresses) {
    auto lpAddr = GetAddress(gameAddr, LocalPlayer::basePtrOffset);
    auto world = GetAddress(gameAddr, World::worldOffset);

    auto x = Read<float>(GetAddress(lpAddr, LocalPlayer::camXOffset));
    auto y = Read<float>(GetAddress(lpAddr, LocalPlayer::camYOffset));
    auto z = Read<float>(GetAddress(lpAddr, LocalPlayer::camZOffset));

    // our camera forward vector
    auto forx = Read<float>(GetAddress(lpAddr, LocalPlayer::camForwardXOffset));
    auto fory = Read<float>(GetAddress(lpAddr, LocalPlayer::camForwardYOffset));
    auto forz = Read<float>(GetAddress(lpAddr, LocalPlayer::camForwardZOffset));

    auto coordAddr = GetAddress(lpAddr, LocalPlayer::CoordOffsets);

    auto px = Read<float>(GetAddress(coordAddr, LocalPlayer::xOffsets));
    auto py = Read<float>(GetAddress(coordAddr, LocalPlayer::yOffsets));
    auto pz = Read<float>(GetAddress(coordAddr, LocalPlayer::zOffsets));

    vec3 playerPos(px, py, pz);

    auto entity = NearestEntity(world, playerPos, 45.0f);

    printf("target dist: %.0f target name: %s\r", entity.range, entity.name.c_str());
    //printf("player pos: %.0f %.0f %.0f\n", playerPos.x, playerPos.y, playerPos.z);
    vec3 camPos(x, y, z);

    std::array<float, 3> forwardArray;

    if (entity.range == -1.f) { // no entity found
        forwardArray = { forx, fory, forz };
    } else {
        vec3 forward = GetForwardVector(camPos, entity.position);
        forwardArray = {forward.x, forward.y, forward.z};
    }


    for (int i = 0; i < 3; ++i) {   
        float value = -forwardArray[i];
        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(hProcess, addresses[i], &value, sizeof(float), &bytesWritten) || bytesWritten != sizeof(float)) {
            std::cerr << "Failed to write float to remote process.\n";
        }
    }
}

// For patching old bytes when exit window
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT:
        cleanup();
        return TRUE;

    case CTRL_CLOSE_EVENT:
        cleanup();
        return TRUE;

    case CTRL_BREAK_EVENT:
        cleanup();
        return FALSE;

    case CTRL_LOGOFF_EVENT:
        cleanup();
        return FALSE;

    case CTRL_SHUTDOWN_EVENT:
        cleanup();
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
    gameAddr = baseAddress;

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

    while (true) {
        Aimbot(addresses);
        Sleep(10);
    }

    return 0;
}