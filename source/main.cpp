#include "GarrysMod/Lua/Interface.h"
#include <map>
#include <vector>
#include <chrono>
#include <Windows.h>
#include <cstdint>
#include <MinHook.h>
#include <steam/steam_api.h>

#pragma comment(linker, "/NODEFAULTLIB:libcmt.lib")

using namespace GarrysMod::Lua;

#pragma pack(push, 1)
struct WavHeader
{
    char     chunkId[4] = { 'R','I','F','F' };
    uint32_t chunkSize = 0;
    char     format[4] = { 'W','A','V','E' };
    char     subchunk1Id[4] = { 'f','m','t',' ' };
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 96000;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    char     subchunk2Id[4] = { 'd','a','t','a' };
    uint32_t subchunk2Size = 0;
};
#pragma pack(pop)

struct VoiceData
{
    std::vector<char> pcmBuffer;
    std::chrono::time_point<std::chrono::steady_clock> lastPacketTime;
    bool isSpeaking = false;
};

std::map<int, VoiceData> activeSpeakers;
ILuaBase* g_Lua = nullptr;
float g_flVoiceTimeout = 1.0f;

typedef unsigned int(__thiscall* OriginalReadFn)(void*, int*);
OriginalReadFn Original_Read = nullptr;

void SafeSetTop(int target)
{
    int current = g_Lua->Top();
    if (current > target)
        g_Lua->Pop(current - target);
}

void PushColor(int r, int g, int b)
{
    g_Lua->CreateTable();
    g_Lua->PushNumber(r); g_Lua->SetField(-2, "r");
    g_Lua->PushNumber(g); g_Lua->SetField(-2, "g");
    g_Lua->PushNumber(b); g_Lua->SetField(-2, "b");
    g_Lua->PushNumber(255); g_Lua->SetField(-2, "a");
}

void GameLog(const char* fmt, ...)
{
    int top = g_Lua->Top();
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, fmt, args);
    va_end(args);

    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->GetField(-1, "MsgC");
    if (g_Lua->IsType(-1, Type::Function))
    {
        PushColor(0, 255, 100);
        g_Lua->PushString("[Voice-Module] ");
        PushColor(255, 255, 255);
        g_Lua->PushString(buf);
        g_Lua->PushString("\n");
        g_Lua->PCall(5, 0, 0);
    }
    SafeSetTop(top);
}

std::vector<char> CreateWavFromPCM(const std::vector<char>& pcmData)
{
    WavHeader header;
    header.subchunk2Size = (uint32_t)pcmData.size();
    header.chunkSize = 36 + header.subchunk2Size;

    std::vector<char> wav;
    wav.reserve(sizeof(WavHeader) + pcmData.size());

    const char* hPtr = reinterpret_cast<const char*>(&header);
    wav.insert(wav.end(), hPtr, hPtr + sizeof(WavHeader));
    wav.insert(wav.end(), pcmData.begin(), pcmData.end());

    return wav;
}

void DecodeAndStore(int clientIndex, const unsigned char* data, size_t length)
{
    int top = g_Lua->Top();
    auto& speaker = activeSpeakers[clientIndex];
    speaker.lastPacketTime = std::chrono::steady_clock::now();

    if (!speaker.isSpeaking)
    {
        speaker.isSpeaking = true;
        speaker.pcmBuffer.clear();

        g_Lua->PushSpecial(SPECIAL_GLOB);
        g_Lua->GetField(-1, "hook");
        g_Lua->GetField(-1, "Run");
        g_Lua->PushString("OnVoiceChatStart");
        g_Lua->PushNumber(clientIndex);
        g_Lua->PCall(2, 0, 0);
        SafeSetTop(top);
    }

    const uint32_t MAX_FRAME_BYTES = 22528;
    std::vector<int16_t> pcm(MAX_FRAME_BYTES / 2);
    uint32_t bytesWritten = 0;

    EVoiceResult res = SteamUser()->DecompressVoice(
        data,
        (uint32_t)length,
        pcm.data(),
        (uint32_t)pcm.size() * sizeof(int16_t),
        &bytesWritten,
        48000
    );

    if (res == k_EVoiceResultOK && bytesWritten > 0)
    {
        const char* rawPcm = reinterpret_cast<const char*>(pcm.data());
        speaker.pcmBuffer.insert(speaker.pcmBuffer.end(), rawPcm, rawPcm + bytesWritten);
    }
    else
    {
        GameLog("Steam DecompressVoice failed with result: %d", (int)res);
    }

    SafeSetTop(top);
}

unsigned int __fastcall Hooked_SVC_VoiceData_Read(void* thisPtr, void* edx, int* bfRead)
{
    unsigned int result = Original_Read(thisPtr, bfRead);

    int clientIndex = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x10);
    int lengthInBits = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x18);
    void* dataBuffer = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(thisPtr) + 0x28);
    int bitPosition = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x34);

    if (clientIndex >= 1 && clientIndex <= 128 && lengthInBits > 0 && dataBuffer)
    {
        int lengthInBytes = (lengthInBits + 7) / 8;
        const unsigned char* rawBuffer = reinterpret_cast<const unsigned char*>(dataBuffer);

        std::vector<unsigned char> alignedVoiceData(lengthInBytes, 0);
        for (int i = 0; i < lengthInBytes; ++i)
        {
            int curBit = bitPosition + (i * 8);
            int byteIdx = curBit / 8;
            int bitOffset = curBit % 8;

            if (bitOffset == 0)
            {
                alignedVoiceData[i] = rawBuffer[byteIdx];
            }
            else
            {
                unsigned char b1 = rawBuffer[byteIdx];
                unsigned char b2 = rawBuffer[byteIdx + 1];
                alignedVoiceData[i] = (b1 >> bitOffset) | (b2 << (8 - bitOffset));
            }
        }

        DecodeAndStore(clientIndex, alignedVoiceData.data(), lengthInBytes);
    }

    return result;
}

LUA_FUNCTION(CheckVoiceTimeouts)
{
    int top = g_Lua->Top();
    auto now = std::chrono::steady_clock::now();
    int timeoutMs = static_cast<int>(g_flVoiceTimeout * 1000);

    for (auto it = activeSpeakers.begin(); it != activeSpeakers.end();)
    {
        if (it->second.isSpeaking)
        {
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastPacketTime).count();

            if (diff > timeoutMs)
            {
                int id = it->first;
                it->second.isSpeaking = false;
                size_t pcmSize = it->second.pcmBuffer.size();

                if (pcmSize > 0)
                {
                    std::vector<char> wav = CreateWavFromPCM(it->second.pcmBuffer);

                    g_Lua->PushSpecial(SPECIAL_GLOB);
                    g_Lua->GetField(-1, "hook");
                    g_Lua->GetField(-1, "Run");
                    g_Lua->PushString("OnVoiceChatEnd");
                    g_Lua->PushNumber(id);
                    g_Lua->PushString(wav.data(), (unsigned int)wav.size());

                    if (g_Lua->PCall(3, 0, 0) != 0)
                    {
                        GameLog("Error in OnVoiceChatEnd: %s", g_Lua->GetString(-1));
                        g_Lua->Pop();
                    }
                }

                it = activeSpeakers.erase(it);
                SafeSetTop(top);
                continue;
            }
        }
        ++it;
    }

    SafeSetTop(top);
    return 0;
}

LUA_FUNCTION(SetVoiceTimeout)
{
    g_Lua->CheckType(1, Type::Number);
    float timeout = (float)g_Lua->GetNumber(1);
    if (timeout > 0.0f)
    {
        g_flVoiceTimeout = timeout;
        GameLog("Voice timeout set to %.2f seconds", timeout);
    }
    return 0;
}

LUA_FUNCTION(GetActiveSpeakers)
{
    g_Lua->CreateTable();
    int index = 1;
    for (const auto& pair : activeSpeakers)
    {
        if (pair.second.isSpeaking)
        {
            g_Lua->PushNumber(pair.first);
            g_Lua->PushNumber(index++);
            g_Lua->RawSet(-3);
        }
    }
    return 1;
}

GMOD_MODULE_OPEN()
{
    g_Lua = LUA;

    HMODULE hEngine = GetModuleHandleA("engine.dll");
    if (!hEngine)
    {
        GameLog("Failed to get handle of engine.dll");
        return 0;
    }

    void* targetFunc = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(hEngine) + 0x1ced30);

    if (MH_Initialize() != MH_OK)
    {
        GameLog("MinHook init failed");
        return 0;
    }

    if (MH_CreateHook(targetFunc, Hooked_SVC_VoiceData_Read, reinterpret_cast<void**>(&Original_Read)) != MH_OK)
    {
        GameLog("Failed to create hook");
        return 0;
    }

    if (MH_EnableHook(targetFunc) != MH_OK)
    {
        GameLog("Failed to enable hook");
        return 0;
    }

    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->CreateTable();
    g_Lua->PushCFunction(CheckVoiceTimeouts);
    g_Lua->SetField(-2, "CheckVoiceTimeouts");
    g_Lua->PushCFunction(SetVoiceTimeout);
    g_Lua->SetField(-2, "SetTimeout");
    g_Lua->PushCFunction(GetActiveSpeakers);
    g_Lua->SetField(-2, "GetActiveSpeakers");
    g_Lua->SetField(-2, "voice_packets");
    g_Lua->Pop();

    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->GetField(-1, "timer");
    g_Lua->GetField(-1, "Create");
    g_Lua->PushString("VoiceTimeoutTimer");
    g_Lua->PushNumber(0.1);
    g_Lua->PushNumber(0);
    g_Lua->PushCFunction(CheckVoiceTimeouts);

    if (g_Lua->PCall(4, 0, 0) != 0)
    {
        GameLog("Failed to create timer: %s", g_Lua->GetString(-1));
        g_Lua->Pop();
    }
    g_Lua->Pop();

    GameLog("gm_voice_packets module loaded successfully.");

    return 0;
}

GMOD_MODULE_CLOSE()
{
    activeSpeakers.clear();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    return 0;
}