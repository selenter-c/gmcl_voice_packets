# gmcl_voice_packets

A Garry's Mod client-side module that captures voice chat data, decompresses it, and saves it as WAV files. It also provides a Lua API to monitor and handle voice activity.

## Compatibility

> [!IMPORTANT]  
> This module is now optimized for the **64-bit (x86-64)** version of Garry's Mod. While 32-bit build configurations exist, they are considered **legacy/non-functional** because the engine offsets are maintained exclusively for the x64 architecture.

**Tested with:**
* **Protocol version:** 24
* **Network version:** 2025.03.26 (garrysmod)
* **Exe build:** 14:35:20 Apr 1 2026 (10007) (4000)
* **GMod version:** 2026.04.01, branch: x86-64, multicore: 1
* **Windows 64-bit**

## Build Instructions

### Prerequisites
- [Premake5](https://premake.github.io/) (Included in root)
- [garrysmod_common](https://github.com/danielga/garrysmod_common)

### Step-by-Step Setup
1.  **Initialize Environment**: Run `generate_garrysmod_common.bat`. This ensures all necessary headers from `garrysmod_common` are initialized.
2.  **Generate Projects**: Run `generate_x64.bat`. This uses `premake5_x64.lua` to create the Visual Studio solution.
3.  **Open Solution**: Navigate to `projects\windows\vs2026\x64` and open **`voice_packets.slnx`**.
4.  **Compilation**:
    * Set configuration to **Release** and platform to **x64**.
    * Press **`F7`** to build the solution.
5.  **Output**: The binary `gmcl_voice_packets_win64.dll` will be created in the output directory.

## Maintenance & Offsets

> [!TIP]
> This module no longer requires manual offset updates after game patches.

The module implements an internal **Pattern Scanner** that searches the `engine.dll` memory space for the specific instruction signature of `SVC_VoiceData::Read`. This ensures that the module remains functional even if the function's memory address changes in future Garry's Mod updates.

## Lua API Documentation

### Methods (`voice_packets` table)

#### `voice_packets.SetTimeout(number: seconds)`
Sets the duration of silence required before a voice session is considered "finished."
* **Default**: `1.0`
* **Example**: `voice_packets.SetTimeout(0.5)`

#### `voice_packets.GetActiveSpeakers()`
Returns a sequentially indexed table of player indices (UserIDs) currently transmitting voice.
* **Returns**: `table` (e.g., `{1, 4, 12}`)

#### `voice_packets.CreateWavFromPCM(string: pcmData)`
Converts raw 16-bit PCM data into a valid, playable `.wav` file string (48000Hz, Mono, 16-bit).
* **Returns**: `string` (Binary WAV data)

#### `voice_packets.CheckVoiceTimeouts()`
Internal logic handler. This is called automatically by a module-created timer every 0.1s to trigger `OnVoiceChatEnd`.

---

### Hooks

#### `OnVoiceChatStart(number: clientIndex)`
Called the moment the module detects the first voice packet from a specific player.
* **clientIndex**: The Entity index of the player.

#### `OnVoiceChatEnd(number: clientIndex, string: pcmData)`
Called when a player stops talking (after the timeout duration).
* **clientIndex**: The Entity index of the player.
* **pcmData**: The raw, decompressed 16-bit PCM binary string. Use `CreateWavFromPCM` to save this as a file.

## Example Usage

```lua
local status = pcall(require, "voice_packets")

if not status or not voice_packets or not voice_packets.CheckVoiceTimeouts then
    print("[Voice-Lua] ERROR: C++ library not loaded!")
    return
end

local pathDir = "voice_logs"
if not file.Exists(pathDir, "DATA") then
    file.CreateDir(pathDir)
end

voice_packets.SetTimeout(1.0)

timer.Create("CheckActiveSpeakers", 5, 0, function()
    local active = voice_packets.GetActiveSpeakers()

    if #active > 0 then
        local names = {}
        for _, idx in ipairs(active) do
            local ply = Entity(idx)
            table.insert(names, IsValid(ply) and ply:Nick() or "Player " .. idx)
        end
        print("[Voice] Currently speaking: " .. table.concat(names, ", "))
    end
end)

hook.Add("OnVoiceChatStart", "MyVoiceStart", function(clientIndex)
    local ply = Entity(clientIndex)
    local name = IsValid(ply) and ply:Nick() or "Player " .. clientIndex
    print("[Voice] " .. name .. " started speaking...")
end)

hook.Add("OnVoiceChatEnd", "MyVoiceEnd", function(clientIndex, data)
    if not data or #data == 0 then return end

    local wav = voice_packets.CreateWavFromPCM(data)
    local ply = Entity(clientIndex)
    
    local rawName = IsValid(ply) and ply:Nick() or "unknown_" .. clientIndex
    local safeName = string.gsub(rawName, "[%c%/%\\%:%*%?%\"%<%>%|]", "_")
    
    local fileName = "voice_" .. safeName .. "_" .. os.time() .. ".wav"
    local filePath = pathDir .. "/" .. fileName

    file.Write(filePath, wav)

    local sizeKB = #wav / 1024
    print(string.format("[Voice] Recording saved: %s | Size: %.2f KB", fileName, sizeKB))
end)
```

## How It Works

The module hooks the `SVC_VoiceData::Read` method inside `engine.dll` using **MinHook**.

1.  **Extraction**: It intercepts the raw bitstream from the engine.
2.  **Alignment**: Manually aligns the bit-level data into byte-aligned buffers.
3.  **Decompression**: Uses the Steamworks SDK (`ISteamUser::DecompressVoice`) to decode Opus/SILK data into standard 16-bit PCM.
4.  **WAV Construction**: Adds the standard RIFF/WAVE headers to the PCM data upon request.

## Notes
- The module is **client-side only**.
- It requires `engine.dll` offsets to be accurate.
- 64-bit GMod is mandatory for current build stability.