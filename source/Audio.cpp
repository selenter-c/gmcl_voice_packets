#include "VoiceCore.h"

#pragma pack(push, 1)
struct WavHeader {
    char chunkId[4] = { 'R','I','F','F' };
    uint32_t chunkSize = 0;
    char format[4] = { 'W','A','V','E' };
    char subchunk1Id[4] = { 'f','m','t',' ' };
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = BYTE_RATE;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    char subchunk2Id[4] = { 'd','a','t','a' };
    uint32_t subchunk2Size = 0;
};
#pragma pack(pop)

std::vector<char> CreateWavFromPCM(const std::vector<char>& pcmData) {
    WavHeader header;
    header.subchunk2Size = static_cast<uint32_t>(pcmData.size());
    header.chunkSize = 36 + header.subchunk2Size;

    std::vector<char> wav;
    wav.reserve(sizeof(WavHeader) + pcmData.size());

    const char* headerPtr = reinterpret_cast<const char*>(&header);
    wav.insert(wav.end(), headerPtr, headerPtr + sizeof(WavHeader));
    wav.insert(wav.end(), pcmData.begin(), pcmData.end());

    return wav;
}

void ProcessVoiceData(int clientIndex, const unsigned char* data, size_t length) {
    auto& speaker = g_ActiveSpeakers[clientIndex];
    speaker.lastPacketTime = std::chrono::steady_clock::now();

    if (!speaker.isSpeaking) {
        Debug("New speaker detected: client=%d", clientIndex);
        speaker.isSpeaking = true;
        speaker.pcmBuffer.clear();
        TriggerLuaHook("OnVoiceChatStart", clientIndex);
    }

    ISteamUser* steamUser = SteamUser();
    if (!steamUser) return;

    std::vector<int16_t> pcmBuffer(MAX_FRAME_BYTES / 2);
    uint32_t bytesWritten = 0;

    EVoiceResult result = steamUser->DecompressVoice(
        data, static_cast<uint32_t>(length),
        pcmBuffer.data(), static_cast<uint32_t>(pcmBuffer.size() * sizeof(int16_t)),
        &bytesWritten, SAMPLE_RATE
    );

    if (result == k_EVoiceResultOK && bytesWritten > 0) {
        const char* rawPcm = reinterpret_cast<const char*>(pcmBuffer.data());
        speaker.pcmBuffer.insert(speaker.pcmBuffer.end(), rawPcm, rawPcm + bytesWritten);
    } else if (result != k_EVoiceResultNotRecording && result != k_EVoiceResultNoData) {
        Debug("Steam DecompressVoice failed: %d", static_cast<int>(result));
    }
}