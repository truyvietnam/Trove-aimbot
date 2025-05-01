#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <array>
#include <iostream>
#include <vector>

// Store original instructions for restoration
std::array<std::vector<BYTE>, 3> originalInstructions;
std::array<uintptr_t, 3> instructionAddresses;
std::array<LPVOID, 3> addresses;
HANDLE hProcess = nullptr;

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


#endif