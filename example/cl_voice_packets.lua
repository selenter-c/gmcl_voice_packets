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