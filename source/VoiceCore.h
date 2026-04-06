#pragma once
#include "GarrysMod/Lua/Interface.h"
#include <map>
#include <vector>
#include <chrono>
#include <cstdint>
#include <Windows.h>
#include <steam_api.h>

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint32_t BYTE_RATE = 96000;
constexpr uint32_t MAX_FRAME_BYTES = 22528;
constexpr int MAX_CLIENTS = 128;

struct VoiceData {
    std::vector<char> pcmBuffer;
    std::chrono::time_point<std::chrono::steady_clock> lastPacketTime;
    bool isSpeaking = false;
};

extern std::map<int, VoiceData> g_ActiveSpeakers;
extern GarrysMod::Lua::ILuaBase* g_Lua;
extern float g_VoiceTimeout;
extern bool g_IsDebug;

void GameLog(const char* fmt, ...);
void Debug(const char* fmt, ...);
void TriggerLuaHook(const char* hookName, int clientIndex, const std::vector<char>* data = nullptr);

std::vector<char> CreateWavFromPCM(const std::vector<char>& pcmData);
void ProcessVoiceData(int clientIndex, const unsigned char* data, size_t length);
bool InitializeHook();
void RemoveHook();