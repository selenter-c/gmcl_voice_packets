timer.Simple(2, function()
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
end)
