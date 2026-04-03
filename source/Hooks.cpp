#include "VoiceCore.h"
#include <MinHook.h>

typedef bool(__fastcall* OriginalReadFn)(void*, void*);
OriginalReadFn Original_Read = nullptr;

std::vector<unsigned char> AlignVoiceData(const unsigned char* rawBuffer, int lengthInBytes, int bitsAvail) {
    std::vector<unsigned char> alignedData(lengthInBytes, 0);
    int bitPosition = 32 - bitsAvail;

    for (int i = 0; i < lengthInBytes; ++i) {
        int currentBit = bitPosition + (i * 8);
        int byteIndex = currentBit / 8;
        int bitOffset = currentBit % 8;

        if (bitOffset == 0) {
            alignedData[i] = rawBuffer[byteIndex];
        } else {
            unsigned char b1 = rawBuffer[byteIndex];
            unsigned char b2 = rawBuffer[byteIndex + 1];
            alignedData[i] = (b1 >> bitOffset) | (b2 << (8 - bitOffset));
        }
    }
    return alignedData;
}

bool __fastcall Hooked_SVC_VoiceData_Read(void* thisPtr, void* bfRead) {
    bool originalResult = Original_Read(thisPtr, bfRead);

    auto baseAddr = reinterpret_cast<uintptr_t>(thisPtr);
    int clientIndex = *reinterpret_cast<int*>(baseAddr + 0x20);
    int lengthInBits = *reinterpret_cast<int*>(baseAddr + 0x28);
    int bitsAvail = *reinterpret_cast<int*>(baseAddr + 0x54);
    uint32_t* pDataIn = *reinterpret_cast<uint32_t**>(baseAddr + 0x58);

    if (clientIndex >= 0 && clientIndex < MAX_CLIENTS && lengthInBits > 0 && pDataIn) {
        int lengthInBytes = (lengthInBits + 7) / 8;
        const unsigned char* rawBuffer = reinterpret_cast<const unsigned char*>(pDataIn - 1);
        std::vector<unsigned char> alignedData = AlignVoiceData(rawBuffer, lengthInBytes, bitsAvail);
        ProcessVoiceData(clientIndex + 1, alignedData.data(), lengthInBytes);
    }

    return originalResult;
}

bool InitializeHook() {
    HMODULE hEngine = GetModuleHandleA("engine.dll");
    if (!hEngine) {
        GameLog("Failed to get handle of engine.dll");
        return false;
    }

    void* targetFunc = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(hEngine) + ENGINE_HOOK_OFFSET);

    if (MH_Initialize() != MH_OK) {
        GameLog("MinHook init failed");
        return false;
    }

    if (MH_CreateHook(targetFunc, Hooked_SVC_VoiceData_Read, reinterpret_cast<void**>(&Original_Read)) != MH_OK) {
        GameLog("Failed to create hook");
        return false;
    }

    if (MH_EnableHook(targetFunc) != MH_OK) {
        GameLog("Failed to enable hook");
        return false;
    }

    return true;
}

void RemoveHook() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}