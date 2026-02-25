# gmcl_voice_packets

A Garry's Mod client-side module that captures voice chat data, decompresses it, and saves it as WAV files. It also provides a Lua API to monitor and handle voice activity.

## Compatibility

This module was compiled exclusively for the **32-bit (x86)** version of Garry's Mod, based on reverse engineering of the Windows client. It will **not work** on the 64-bit branch due to binary differences and the target function offset being specific to the x86 build.

**Tested with:**  
Protocol version 24  
Network version 2025.03.26 (garrysmod)  
Exe build: 15:41:51 Dec 10 2025 (9860) (4000)  
GMod version 2025.12.10, branch: unknown, multicore: 0  
Windows 32bit

## Build Instructions

### Prerequisites
- [Premake5](https://premake.github.io/)
- [garrysmod_common](https://github.com/danielga/garrysmod_common)

### Steps

1. Clone this repository and navigate to its root.

2. Clone the garrysmod_common repository and its submodules:
   ```bash
   git clone https://github.com/danielga/garrysmod_common.git
   cd garrysmod_common
   git submodule update --init --recursive
   ```

3. Generate the project files using Premake5:
   ```bash
   premake5.exe --os=windows --gmcommon=./garrysmod_common vs2026
   ```
   (Adjust the path to garrysmod_common if needed)

4. Open the generated solution in `projects/windows/vs2026/` and build the project. The compiled binary will be placed in the appropriate output directory.

## Dependencies

The module links against:
- [minhook](https://github.com/TsudaKageyu/minhook) – for hooking the target function.
- [opus](https://opus-codec.org/) – though Steam's `DecompressVoice` is used, the opus library is required for linking.
- [Steam SDK](https://partner.steamgames.com/downloads/list) – provides `steam_api.lib` and `sdkencryptedappticket.lib`.

All dependencies are expected to be placed in the `deps/` folder as shown in the Premake script.

## How It Works

The module hooks the `SVC_VoiceData::Read` method inside `engine.dll`. The function offset (`0x1ced30`) was identified through disassembly (Ghidra dump provided). This method is called when a `svc_VoiceData` message is received from a client.

### Hooked Function (`Hooked_SVC_VoiceData_Read`)
- Extracts the client index, data length (in bits), and a pointer to the raw voice data buffer.
- Because the data may be unaligned (due to bitstream reading), it manually aligns the bytes before passing them to Steam's decompressor.
- Calls `SteamUser()->DecompressVoice` to convert Opus‑encoded voice into 16‑bit PCM.

### Voice Data Storage
- For each client that starts speaking, a `VoiceData` structure is created containing:
  - A growing PCM buffer.
  - A timestamp of the last received packet.
  - A flag indicating whether the client is currently speaking.
- PCM data is appended until the client stops speaking (timeout is reached).

### Timeout Handling
- A Lua timer (every 0.1 seconds) calls `CheckVoiceTimeouts`.
- If no voice packet is received for `g_flVoiceTimeout` seconds (default 1.0), the speaking session ends.
- The accumulated PCM data is then wrapped in a valid WAV header (mono, 48kHz, 16‑bit) and passed to the Lua hook `OnVoiceChatEnd`.

### Lua Integration
The module exposes a table `voice_packets` with the following functions:

- `voice_packets.CheckVoiceTimeouts()` – called automatically by a timer, checks for finished voice sessions and fires the `OnVoiceChatEnd` hook.
- `voice_packets.SetTimeout(seconds)` – changes the inactivity timeout.
- `voice_packets.GetActiveSpeakers()` – returns a table mapping client indices to an array index of currently speaking players.

Two Lua hooks are also available:

- `hook.Add("OnVoiceChatStart", "id", function(clientIndex) end)` – triggered when a client starts sending voice.
- `hook.Add("OnVoiceChatEnd", "id", function(clientIndex, wavData) end)` – triggered when a client stops speaking, with `wavData` being a string containing a complete WAV file.

## Example Lua Usage

```lua
local status = pcall(require, "voice_packets")
if !status or !voice_packets or !voice_packets.CheckVoiceTimeouts then return print("[Voice-Lua] ERROR: C++ library not loaded!") end

local pathDir = "voice_logs"
file.CreateDir(pathDir)

voice_packets.SetTimeout(1.0)

timer.Create("CheckActiveSpeakers", 5, 0, function()
    local active = voice_packets.GetActiveSpeakers()

    if #active > 0 then
        local names = {}
        for _, idx in ipairs(active) do
            local ply = Entity(idx)
            table.insert(names, IsValid(ply) and ply:Nick() or "player " .. idx)
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
    local ply = Entity(clientIndex)
    local rawName = IsValid(ply) and ply:Nick() or "unknown_" .. clientIndex

    local safeName = string.gsub(rawName, "[%c%/%\\%:%*%?%\"%<%>%|]", "_")

    local fileName = "voice_" .. safeName .. "_" .. os.time() .. ".wav"
    file.Write(pathDir .. "/" .. fileName, data)

    print("[Voice] Recording saved: " .. fileName .. " | Size: " .. string.format("%.2f", #data / 1024) .. " KB")
end)
```

## Notes

- The module is **client‑side only**; it hooks into the `engine.dll`.
- The WAV files are saved in the standard format for mono 48 kHz 16‑bit PCM.
- If the module fails to load, check that the `engine.dll` offset is correct for your GMod version.
