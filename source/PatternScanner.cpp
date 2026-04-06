#include "PatternScanner.h"
#include <vector>

namespace PatternScanner {

    void* SearchPattern(uint8_t* baseAddress, DWORD size, const std::vector<uint8_t>& pattern) {
        for (DWORD i = 0; i < size - pattern.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < pattern.size(); ++j) {
                if (baseAddress[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return baseAddress + i;
            }
        }
        return nullptr;
    }

    void* FindVoiceDataRead(HMODULE hModule) {
        if (!hModule) return nullptr;

        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)hModule + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return nullptr;

        uint8_t* baseAddress = (uint8_t*)hModule;
        DWORD moduleSize = ntHeaders->OptionalHeader.SizeOfImage;

        std::vector<uint8_t> fullSig = { 0x48, 0x83, 0xEC, 0x38, 0x44, 0x8B, 0x5A, 0x1C, 0x45, 0x33, 0xC9 };
        std::vector<uint8_t> shortSig = { 0x48, 0x83, 0xEC, 0x38, 0x44 };

        void* result = SearchPattern(baseAddress, moduleSize, fullSig);
        if (result) return result;

        return SearchPattern(baseAddress, moduleSize, shortSig);
    }
}