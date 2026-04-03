#include "VoiceCore.h"

#pragma comment(linker, "/NODEFAULTLIB:libcmt.lib")

using namespace GarrysMod::Lua;

std::map<int, VoiceData> g_ActiveSpeakers;
ILuaBase* g_Lua = nullptr;
float g_VoiceTimeout = 1.0f;
bool g_IsDebug = false;

void SafeSetTop(int target) {
    int current = g_Lua->Top();
    if (current > target) {
        g_Lua->Pop(current - target);
    }
}

void PushColor(int r, int g, int b) {
    g_Lua->CreateTable();
    g_Lua->PushNumber(r); g_Lua->SetField(-2, "r");
    g_Lua->PushNumber(g); g_Lua->SetField(-2, "g");
    g_Lua->PushNumber(b); g_Lua->SetField(-2, "b");
    g_Lua->PushNumber(255); g_Lua->SetField(-2, "a");
}

void LogMessage(const char* prefix, int r, int g, int b, const char* fmt, va_list args) {
    int top = g_Lua->Top();
    char buf[512];
    vsprintf_s(buf, fmt, args);

    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->GetField(-1, "MsgC");
    if (g_Lua->IsType(-1, Type::Function)) {
        PushColor(r, g, b);
        g_Lua->PushString(prefix);
        PushColor(255, 255, 255);
        g_Lua->PushString(buf);
        g_Lua->PushString("\n");
        g_Lua->PCall(5, 0, 0);
    }
    SafeSetTop(top);
}

void GameLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessage("[Voice-Module] ", 0, 255, 100, fmt, args);
    va_end(args);
}

void Debug(const char* fmt, ...) {
    if (g_IsDebug == false) return;

    va_list args;
    va_start(args, fmt);
    LogMessage("[Voice-Module] [DEBUG] ", 0, 255, 100, fmt, args);
    va_end(args);
}

void TriggerLuaHook(const char* hookName, int clientIndex, const std::vector<char>* data) {
    int top = g_Lua->Top();
    
    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->GetField(-1, "hook");
    g_Lua->GetField(-1, "Run");
    g_Lua->PushString(hookName);
    g_Lua->PushNumber(clientIndex);

    int argsCount = 2;
    if (data) {
        g_Lua->PushString(data->data(), static_cast<unsigned int>(data->size()));
        argsCount++;
    }

    if (g_Lua->PCall(argsCount, 0, 0) != 0) {
        Debug("Error in %s: %s", hookName, g_Lua->GetString(-1));
    }
    
    SafeSetTop(top);
}

LUA_FUNCTION(CreateWavFromPCM_Lua) {
    g_Lua->CheckType(1, Type::String);

    unsigned int dataLen = 0;
    const char* pcmData = g_Lua->GetString(1, &dataLen);

    if (!pcmData || dataLen == 0) {
        g_Lua->PushString("");
        return 1;
    }

    std::vector<char> pcmVector(pcmData, pcmData + dataLen);
    std::vector<char> wavData = CreateWavFromPCM(pcmVector);

    g_Lua->PushString(wavData.data(), static_cast<unsigned int>(wavData.size()));
    return 1;
}

LUA_FUNCTION(CheckVoiceTimeouts) {
    int top = g_Lua->Top();
    auto now = std::chrono::steady_clock::now();
    int timeoutMs = static_cast<int>(g_VoiceTimeout * 1000);

    for (auto it = g_ActiveSpeakers.begin(); it != g_ActiveSpeakers.end();) {
        if (!it->second.isSpeaking) {
            ++it;
            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastPacketTime).count();
        if (elapsed > timeoutMs) {
            int clientId = it->first;
            it->second.isSpeaking = false;

            if (!it->second.pcmBuffer.empty()) {
                TriggerLuaHook("OnVoiceChatEnd", clientId, &it->second.pcmBuffer);
            }

            it = g_ActiveSpeakers.erase(it);
        } else {
            ++it;
        }
    }

    SafeSetTop(top);
    return 0;
}

LUA_FUNCTION(SetVoiceTimeout) {
    g_Lua->CheckType(1, Type::Number);
    
    float timeout = static_cast<float>(g_Lua->GetNumber(1));
    if (timeout > 0.0f) {
        g_VoiceTimeout = timeout;
        GameLog("Voice timeout set to %.2f seconds", timeout);
    }
    return 0;
}

LUA_FUNCTION(GetActiveSpeakers) {
    g_Lua->CreateTable();
    int tableIndex = 1;

    for (const auto& [clientId, voiceData] : g_ActiveSpeakers) {
        if (voiceData.isSpeaking) {
            g_Lua->PushNumber(tableIndex++);
            g_Lua->PushNumber(clientId);
            g_Lua->RawSet(-3);
        }
    }

    return 1;
}

void RegisterLuaFunctions() {
    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->CreateTable();
    
    g_Lua->PushCFunction(CheckVoiceTimeouts);
    g_Lua->SetField(-2, "CheckVoiceTimeouts");
    
    g_Lua->PushCFunction(SetVoiceTimeout);
    g_Lua->SetField(-2, "SetTimeout");
    
    g_Lua->PushCFunction(GetActiveSpeakers);
    g_Lua->SetField(-2, "GetActiveSpeakers");

    g_Lua->PushCFunction(CreateWavFromPCM_Lua);
    g_Lua->SetField(-2, "CreateWavFromPCM");
    
    g_Lua->SetField(-2, "voice_packets");
    g_Lua->Pop();
}

void CreateLuaTimer() {
    g_Lua->PushSpecial(SPECIAL_GLOB);
    g_Lua->GetField(-1, "timer");
    g_Lua->GetField(-1, "Create");
    g_Lua->PushString("VoiceTimeoutTimer");
    g_Lua->PushNumber(0.1);
    g_Lua->PushNumber(0);
    g_Lua->PushCFunction(CheckVoiceTimeouts);

    if (g_Lua->PCall(4, 0, 0) != 0) {
        GameLog("Failed to create timer: %s", g_Lua->GetString(-1));
        g_Lua->Pop();
    }
    g_Lua->Pop();
}

GMOD_MODULE_OPEN() {
    g_Lua = LUA;
    Debug("Module loading...");

    if (InitializeHook()) {
        Debug("Hook enabled successfully");
    }

    RegisterLuaFunctions();
    CreateLuaTimer();

    GameLog("gm_voice_packets module loaded successfully.");
    return 0;
}

GMOD_MODULE_CLOSE() {
    g_ActiveSpeakers.clear();
    RemoveHook();
    return 0;
}