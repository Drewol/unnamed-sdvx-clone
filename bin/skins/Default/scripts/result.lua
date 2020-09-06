-- TODO: move util functions to common.lua
local jacketImg = nil
local chartTitle = ""

local desw = 770
local desh = 800

local moveX = 0
local moveY = 0

local currResX = 0
local currResY = 0

local scale = 1
local hitGraphHoverScale = 10

local gradeImg = nil;
local badgeImg = nil;
local gradeAR = 1 --grade aspect ratio

local chartDuration = 0
local chartDurationText = "0m 00s"

local badgeImages = {
    gfx.CreateSkinImage("badges/played.png", 0),
    gfx.CreateSkinImage("badges/clear.png", 0),
    gfx.CreateSkinImage("badges/hard-clear.png", 0),
    gfx.CreateSkinImage("badges/full-combo.png", 0),
    gfx.CreateSkinImage("badges/perfect.png", 0)
}

local laneNames = {"A", "B", "C", "D", "L", "R"}
local diffNames = {"NOV", "ADV", "EXH", "INF"}
local backgroundImage = gfx.CreateSkinImage("bg.png", 1);
game.LoadSkinSample("applause")
local played = false
local shotTimer = 0;
local shotPath = "";
game.LoadSkinSample("shutter")
local highScores = nil

local highestScore = 0

local hasHitStat = false
local hitHistogram = {}
local hitMinDelta = 0
local hitMaxDelta = 0

local hitWindowPerfect = 46
local hitWindowGood = 92

local clearTextBase = "" -- Used to determind the type of clear
local clearText = ""

local currTime = 0

local critText = "CRIT"
local nearText = "NEAR"

local speedMod = ""
local speedModValue = ""

local prevFXLeft = false
local prevFXRight = false

local hitDeltaScale = game.GetSkinSetting("hit_graph_delta_scale")

local showGuide = game.GetSkinSetting("show_result_guide")
local showIcons = game.GetSkinSetting("show_result_icons")
local showStatsHit = game.GetSkinSetting("show_detailed_results")
local showHiScore = game.GetSkinSetting("show_result_hiscore")
local prevShowHiScore = not showHiScore

function waveParam(period, offset)
    local t = currTime
    if offset then t = t+offset end
    
    t = t / period
    
    return 0.5 + 0.5*math.cos(t * math.pi * 2)
end

function getTextScale(txt, max_width)
    local x1, y1, x2, y2 = gfx.TextBounds(0, 0, txt)
    if x2 < max_width then
        return 1
    else
        return max_width / x2
    end
end

function drawScaledText(txt, x, y, max_width)
    local text_scale = getTextScale(txt, max_width)
    
    if text_scale == 1 then
        gfx.BeginPath()
        gfx.Text(txt, x, y)
        return
    end
    
    gfx.Save()
    
    gfx.Translate(x, y)
    gfx.Scale(text_scale, 1)
    
    gfx.BeginPath()
    gfx.Text(txt, 0, 0)
    
    gfx.Restore()
end

function drawLine(x1,y1,x2,y2,w,r,g,b)
    gfx.BeginPath()
    gfx.MoveTo(x1,y1)
    gfx.LineTo(x2,y2)
    gfx.StrokeColor(r,g,b)
    gfx.StrokeWidth(w)
    gfx.Stroke()
end

function getScoreBadgeDesc(s)
    if s.badge == 1 then
        if s.flags & 1 ~= 0 then return "crash"
        else return string.format("%.1f%%", s.gauge * 100)
        end
    elseif 2 <= s.badge and s.badge <= 4 and s.misses < 10 then
        return string.format("%d-%d", s.goods, s.misses)
    end
    return ""
end

result_set = function()
    highScores = { }
    currentAdded = false
    
    chartTitle = result.title
    if result.realTitle ~= nil and result.playerName ~= nil then chartTitle = result.realTitle end
    
    if result.duration ~= nil then
        chartDuration = result.duration
        chartDurationText = string.format("%dm %02d.%01ds", chartDuration // 60000, (chartDuration // 1000) % 60, (chartDuration // 100) % 10)
        hitGraphHoverScale = math.max(chartDuration / 10000, 5)
    else
        chartDuration = 0
        chartDurationText = ""
        hitGraphHoverScale = 10
    end
    
    if result.uid == nil then --local scores
        for i,s in ipairs(result.highScores) do
            newScore = { }
            if currentAdded == false and result.score > s.score then
                newScore.score = string.format("%08d", result.score)
                newScore.badge = result.badge
                newScore.badgeDesc = getScoreBadgeDesc(result)
                newScore.color = {255, 127, 0}
                newScore.subtext = "Now"
                newScore.xoff = 0
                table.insert(highScores, newScore)
                newScore = { }
                currentAdded = true
            end
            newScore.score = string.format("%08d", s.score)
            newScore.badge = s.badge
            newScore.badgeDesc = getScoreBadgeDesc(s)
            newScore.color = {0, 127, 255}
            newScore.xoff = 0
            if s.timestamp > 0 then
                newScore.subtext = os.date("%Y-%m-%d %H:%M:%S", s.timestamp)
            else 
                newScore.subtext = ""
            end
            
            if highestScore < s.score then
                highestScore = s.score
            end
            
            table.insert(highScores, newScore)
        end

        if currentAdded == false then
            newScore = { }
            newScore.score = string.format("%08d", result.score)
            newScore.badge = result.badge
            newScore.badgeDesc = getScoreBadgeDesc(result)
            newScore.color = {255, 127, 0}
            newScore.subtext = "Now"
            newScore.xoff = 0
            table.insert(highScores, newScore)
            newScore = { }
            currentAdded = true
        end
    else --multi scores
        showHiScore = true
        for i,s in ipairs(result.highScores) do
            newScore = { }
            if s.uid == result.uid then 
                newScore.color = {255, 127, 0}
            else
                newScore.color = {0, 127, 255}
            end

            if result.displayIndex + 1 == i then
                newScore.xoff = -20
            else
                newScore.xoff = 0
            end

            newScore.score = string.format("%08d", s.score)
            newScore.badge = s.badge
            newScore.badgeDesc = getScoreBadgeDesc(s)
            newScore.subtext = s.name
            
            if highestScore < s.score then
                highestScore = s.score
            end
            
            table.insert(highScores, newScore)
        end
    end
    
    if result.jacketPath ~= nil and result.jacketPath ~= "" then
        jacketImg = gfx.CreateImage(result.jacketPath, 0)
    end
    
    gradeImg = gfx.CreateSkinImage(string.format("score/%s.png", result.grade), 0)
    if gradeImg ~= nil then
        local gradew, gradeh = gfx.ImageSize(gradeImg)
        gradeAR = gradew / gradeh
    end
    
    if 1 <= result.badge and result.badge <= 5 then
        badgeImg = badgeImages[result.badge]
    end
    
    if result.autoplay then clearTextBase = "AUTOPLAY"
    elseif result.hitWindow ~= nil and result.hitWindow.type == 0 then clearTextBase = "EXPAND JUDGE"
    elseif result.badge == 0 then clearTextBase = "NOT SAVED"
    elseif result.badge == 1 then clearTextBase = "PLAYED"
    elseif result.badge == 2 then clearTextBase = "CLEAR"
    elseif result.badge == 3 then clearTextBase = "HARD CLEAR"
    elseif result.badge == 4 then clearTextBase = "FULL COMBO"
    elseif result.badge == 5 then clearTextBase = "PERFECT"
    else clearTextBase = ""
    end
    
    if result.playbackSpeed ~= nil and result.playbackSpeed ~= 1.00 then
        if clearTextBase == "" then clearText = string.format("x%.2f play", result.playbackSpeed)
        else clearText = string.format("%s (x%.2f play)", clearTextBase, result.playbackSpeed)
        end
    else
        clearText = clearTextBase
    end
    
    if result.uid ~= nil and result.playerName ~= nil and result.isSelf ~= true then
        clearText = string.format("By %s", result.playerName)
    end
    
    if result.speedModType ~= nil then
        if result.speedModType == 0 then
            speedMod = "XMOD"
            speedModValue = string.format("%.2f", result.speedModValue)
        elseif result.speedModType == 1 then
            speedMod = "MMOD"
            speedModValue = string.format("%.1f", result.speedModValue)
        elseif result.speedModType == 2 then
            speedMod = "CMOD"
            speedModValue = string.format("%.1f", result.speedModValue)
        else
            speedMod = ""
            speedModValue = ""
        end
    else
        speedMod = ""
        speedModValue = ""
    end

    hasHitStat = result.noteHitStats ~= nil and #result.noteHitStats > 0
    
    hitWindowPerfect = 46
    hitWindowGood = 92
    critText = "CRIT"
    nearText = "NEAR"
        
    if result.hitWindow ~= nil then
        hitWindowPerfect = result.hitWindow.perfect
        hitWindowGood = result.hitWindow.good
        
        if hitWindowPerfect ~= 46 or hitWindowGood ~= 92 then
            critText = string.format("%02dms CRIT", hitWindowPerfect)
            nearText = string.format("%02dms NEAR", hitWindowGood)
        end
    end
    
    hitHistogram = {}
    
    if hasHitStat then
        for i = 1, #result.noteHitStats do
            local hitStat = result.noteHitStats[i]
            if hitStat.rating == 1 or hitStat.rating == 2 then
                if hitHistogram[hitStat.delta] == nil then hitHistogram[hitStat.delta] = 0 end
                hitHistogram[hitStat.delta] = hitHistogram[hitStat.delta] + 1
                
                if hitStat.delta < hitMinDelta then hitMinDelta = hitStat.delta end
                if hitStat.delta > hitMaxDelta then hitMaxDelta = hitStat.delta end
            end
        end
    end
end

draw_shotnotif = function(x,y)
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    gfx.Save()
    gfx.Translate(x,y)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    gfx.BeginPath()
    gfx.Rect(0,0,200,40)
    gfx.FillColor(30,30,30)
    gfx.StrokeColor(255,128,0)
    gfx.Fill()
    gfx.Stroke()
    gfx.FillColor(255,255,255)
    gfx.FontSize(15)
    gfx.Text("Screenshot saved to:", 3,5)
    gfx.Text(shotPath, 3,20)
    gfx.Restore()
end

---------------------
-- Subcomponents --
---------------------

draw_stat = function(x,y,w,h, name, value, format,r,g,b)
    gfx.Save()
    gfx.Translate(x,y)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    gfx.FontSize(h)
    gfx.Text(name .. ":",0, 0)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_TOP)
    gfx.Text(string.format(format, value),w, 0)
    gfx.BeginPath()
    gfx.MoveTo(0,h)
    gfx.LineTo(w,h)
    if r then gfx.StrokeColor(r,g,b) 
    else gfx.StrokeColor(200,200,200) end
    gfx.StrokeWidth(1)
    gfx.Stroke()
    gfx.Restore()
    return y + h + 5
end

draw_score = function(score, x, y, w, h, pre)
    local center = x + w * 0.54
    local prefix = ""
    if pre ~= nil then prefix = pre end

    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT)
    gfx.FontSize(h)
    gfx.Text(string.format("%s%04d", prefix, score // 10000), center-h/70, y)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
    gfx.FontSize(h*0.75)
    gfx.Text(string.format("%04d", score % 10000), center+h/70, y)
end

draw_highscores = function(full)
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    gfx.FontSize(30)
    gfx.Text("High Scores",510,30)
    gfx.StrokeWidth(1)
    for i,s in ipairs(highScores) do
        gfx.Save()
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
        gfx.BeginPath()
        local ypos =  45 + (i - 1) * 80
        if ypos > desh then
            break
        end
        gfx.Translate(510 + s.xoff, ypos)
        gfx.RoundedRectVarying(0, 0, 260, 70,0,0,25,0)
        gfx.FillColor(15,15,15)
        gfx.StrokeColor(s.color[1], s.color[2], s.color[3])
        gfx.Fill()
        gfx.Stroke()
        gfx.BeginPath()
        gfx.FillColor(255,255,255)
        gfx.FontSize(25)
        gfx.Text(string.format("#%d",i), 5, 5)
        
        if s.badge ~= nil and 1 <= s.badge and s.badge <= 5 then
            gfx.BeginPath()
            gfx.ImageRect(37, 7, 36, 36, badgeImages[s.badge], 1, 0)
            
            if full then
                gfx.BeginPath()
                gfx.FontSize(15)
                gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_BOTTOM)
                gfx.Text(s.badgeDesc, 55, 52)
            end
        end
        
        draw_score(s.score, 55, 42, 215, 60)
        
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_TOP)
        gfx.LoadSkinFont("NotoSans-Regular.ttf")
        gfx.FontSize(20)
        gfx.Text(s.subtext, 135, 45)
        gfx.Restore()
    end
end

draw_gauge_graph = function(x, y, w, h, alpha, xfocus, xscale)
    if alpha == nil then alpha = 160 end
    if xfocus == nil then
        xfocus = 0
        xscale = 1
    end
    
    local leftIndex = math.floor(#result.gaugeSamples/w * (-xfocus / xscale + xfocus))
    leftIndex = math.max(1, math.min(#result.gaugeSamples, leftIndex))
    
    gfx.BeginPath()
    gfx.MoveTo(x, y + h - h * result.gaugeSamples[leftIndex])
    
    for i = leftIndex+1, #result.gaugeSamples do
        local gaugeX = i * w / #result.gaugeSamples
        gaugeX = (gaugeX - xfocus) * xscale + xfocus
        gfx.LineTo(x + gaugeX,y + h - h * result.gaugeSamples[i])
        
        if gaugeX > w then break end
    end
    
    gfx.StrokeWidth(2.0)
    if result.flags & 1 ~= 0 then
        gfx.StrokeColor(255,80,0,alpha)
        gfx.Stroke()
    else
        gfx.StrokeColor(0,180,255,alpha)
        gfx.Scissor(x, y + h * 0.3, w, h * 0.7)
        gfx.Stroke()
        gfx.ResetScissor()
        gfx.Scissor(x,y-10,w,10+h*0.3)
        gfx.StrokeColor(255,0,255,alpha)
        gfx.Stroke()
        gfx.ResetScissor()
    end
end

draw_hit_graph_lines = function(x, y, w, h)
    local maxDispDelta = h/2 / hitDeltaScale
    
    gfx.StrokeWidth(1)
    
    gfx.BeginPath()
    gfx.StrokeColor(128, 255, 128, 128)
    gfx.MoveTo(x, y+h/2)
    gfx.LineTo(x+w, y+h/2)
    gfx.Stroke()
    
    gfx.BeginPath()
    gfx.StrokeColor(64, 128, 64, 64)
    
    for i = -math.floor(maxDispDelta / 10), math.floor(maxDispDelta / 10) do
        local lineY = y + h/2 + i*10*hitDeltaScale
        
        if i ~= 0 then
            gfx.MoveTo(x, lineY)
            gfx.LineTo(x+w, lineY)
        end
    end
    
    gfx.Stroke()
end

draw_hit_graph = function(x, y, w, h, xfocus, xscale)
    if not hasHitStat or hitDeltaScale == 0.0 then
        return
    end
    
    if xfocus == nil then xfocus = 0 end
    if xscale == nil then xscale = 1 end
    
    draw_hit_graph_lines(x, y, w, h)
    
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    gfx.FontSize(12)
    
    for i = 1, #result.noteHitStats do
        local hitStat = result.noteHitStats[i]
        local hitStatX = (hitStat.timeFrac*w - xfocus)*xscale + xfocus
        
        if 0 <= hitStatX then
            if hitStatX > w then break end
            
            local hitStatY = h/2 + hitStat.delta * hitDeltaScale
            if hitStatY < 0 then hitStatY = 0
            elseif hitStatY > h then hitStatY = h
            end
            
            local hitStatSize = 1
            
            if hitStat.rating == 2 then
                hitStatSize = 1.25
                gfx.FillColor(255, 150, 0, 160)
            elseif hitStat.rating == 1 then
                hitStatSize = 1.75
                gfx.FillColor(255, 0, 200, 128)
            elseif hitStat.rating == 0 then
                hitStatSize = 2
                gfx.FillColor(255, 0, 0, 128)
            end
            
            gfx.BeginPath()
            if xscale > 1 then
                gfx.Text(laneNames[hitStat.lane + 1], x+hitStatX, y+hitStatY)
            else
                gfx.Rect(x+hitStatX-hitStatSize/2, y+hitStatY-hitStatSize/2, hitStatSize, hitStatSize)
                gfx.Fill()
            end
        end
    end
end

draw_left_graph = function(x, y, w, h)
    local mx, my = game.GetMousePos()
    mx = mx / scale - moveX
    my = my / scale - moveY
    
    local mhit = x <= mx and mx <= x+w and y <= my and my <= y+h
    local hit_xfocus = 0
    local hit_xscale = 1
    
    gfx.BeginPath()
    gfx.Rect(x, y, w, h)
    gfx.FillColor(255, 255, 255, 32)
    gfx.Fill()
    
    local chartDurationDisp = string.format("Duration: %s", chartDurationText)
    
    if mhit then
        hit_xfocus = mx - x
        hit_xscale = hitGraphHoverScale
        
        local currPos = chartDuration * ((mx - x) / w)
        chartDurationDisp = string.format("%dm %02d.%01ds / %s" , currPos // 60000, (currPos // 1000) % 60, (currPos // 100) % 10, chartDurationText)
        
        drawLine(mx, y, mx, y+h, 1, 64, 96, 64)
    end
    
    gfx.FontSize(17)
    gfx.FillColor(64, 128, 64, 96)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    
    gfx.BeginPath()
    gfx.Text(chartDurationDisp, x+5, y)
    
    if result.bpm ~= nil then
        gfx.BeginPath()
        gfx.Text(string.format("BPM: %s", result.bpm), x+5, y+15)
    end
    
    draw_hit_graph(x, y, w, h, hit_xfocus, hit_xscale)
    if hit_xscale == 1 then
        draw_gauge_graph(x, y, w, h)
    else
        draw_gauge_graph(x, y, w, h, 64, hit_xfocus, hit_xscale)
        draw_gauge_graph(x, y, w, h)
        
        local gaugeInd = math.floor(1 + #result.gaugeSamples/w * ((mx-x - hit_xfocus) / hit_xscale + hit_xfocus))
        gaugeInd = math.max(1, math.min(#result.gaugeSamples, gaugeInd))
        
        local gaugeY = h - h * result.gaugeSamples[gaugeInd]
        
        gfx.StrokeColor(255, 0, 0, 196)
        gfx.FillColor(255, 255, 255, 196)
        gfx.FontSize(16)
        
        gfx.BeginPath()
        gfx.Circle(mx, y + gaugeY, 2)
        gfx.Stroke()
        
        gfx.BeginPath()
        gfx.Text(string.format("%.1f%%", result.gaugeSamples[gaugeInd]*100), mx, y + gaugeY - 10)
    end
    
    -- hitDeltaAbs is unavailable for multiplayers
    if result.uid ~= nil then
        gfx.FontSize(16)
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
        gfx.BeginPath()
        gfx.FillColor(255, 255, 255, 128)
        gfx.Text(string.format("Mean: %.1f ms, Median: %d ms", result.meanHitDelta, result.medianHitDelta), x+4, y+h)
    elseif hasHitStat then
        gfx.FontSize(16)
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
        gfx.BeginPath()
        gfx.FillColor(255, 255, 255, 128)
        gfx.Text(string.format("Mean deviation: %.1fms", result.meanHitDeltaAbs), x+4, y+h)
    end
    
    -- End gauge display
    local endGauge = result.gauge
    local endGaugeY = y + h - h * endGauge
    
    if endGaugeY > y+h - 10 then endGaugeY = y+h - 10
    elseif endGaugeY < y + 10 then endGaugeY = y + 10
    end
    
    local gaugeText = string.format("%.1f%%", endGauge*100)
    
    gfx.FontSize(20)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE)
    local x1, y1, x2, y2 = gfx.TextBounds(x+w-6, endGaugeY, gaugeText)
    
    gfx.BeginPath()
    gfx.FillColor(80, 80, 80, 128)
    gfx.RoundedRect(x1-3, y1, x2-x1+6, y2-y1, 4)
    gfx.Fill()
    
    gfx.BeginPath()
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.FillColor(255, 255, 255)
    gfx.Text(gaugeText, x+w-6, endGaugeY)
end

draw_hit_histogram = function(x, y, w, h)
    if not hasHitStat or hitDeltaScale == 0.0 then
        return
    end
    
    local maxDispDelta = math.floor(h/2 / hitDeltaScale)
    
    local mode = 0
    local modeCount = 0
    
    for i = -maxDispDelta-1, maxDispDelta+1 do
        if hitHistogram[i] == nil then hitHistogram[i] = 0 end
    end

    for i = -maxDispDelta, maxDispDelta do
        local count = hitHistogram[i-1] + hitHistogram[i]*2 + hitHistogram[i+1]
        
        if count > modeCount then
            mode = i
            modeCount = count
        end
    end
    
    gfx.StrokeWidth(1.5)
    gfx.BeginPath()
    gfx.StrokeColor(255, 255, 128, 96)
    gfx.MoveTo(x, y)
    for i = -maxDispDelta, maxDispDelta do
        local count = hitHistogram[i-1] + hitHistogram[i]*2 + hitHistogram[i+1]
        
        gfx.LineTo(x + 0.9 * w * count / modeCount, y+h/2 + i*hitDeltaScale)
    end
    gfx.LineTo(x, y+h)
    gfx.Stroke()
end

draw_right_graph = function(x, y, w, h)
    if not hasHitStat or hitDeltaScale == 0.0 then
        return
    end
    
    gfx.BeginPath()
    gfx.Rect(x, y, w, h)
    gfx.FillColor(64, 64, 64, 32)
    gfx.Fill()
    
    draw_hit_graph_lines(x, y, w, h)
    draw_hit_histogram(x, y, w, h)
    
    local meanY = h/2 + hitDeltaScale * result.meanHitDelta
    local medianY = h/2 + hitDeltaScale * result.medianHitDelta
    
    drawLine(x, y+meanY, x+w, y+meanY, 1.25, 255, 0, 0, 192)
    drawLine(x, y+medianY, x+w, y+medianY, 1.25, 64, 64, 255, 192)
    
    gfx.LoadSkinFont("NovaMono.ttf")
    
    gfx.BeginPath()
    if meanY < medianY then
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
    else
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    end
    gfx.FillColor(255, 128, 128)
    gfx.FontSize(16)
    gfx.Text(string.format("Mean: %.1f ms", result.meanHitDelta), x+2, y+meanY)
    
    gfx.BeginPath()
    if medianY <= meanY then
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
    else
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    end
    gfx.FillColor(196, 196, 255)
    gfx.FontSize(16)
    gfx.Text(string.format("Median: %d ms", result.medianHitDelta), x+2, y+medianY)
    
    gfx.FillColor(255, 255, 255)
    gfx.FontSize(15)
    
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    gfx.Text(string.format("Earliest: %d ms", hitMinDelta), x+5, y)
    
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
    gfx.Text(string.format("Latest: %d ms", hitMaxDelta), x+5, y+h)
end

draw_laser_icon = function(x, y, s)
    gfx.Save()
    gfx.Translate(x, y)
    
    local r, g, b = game.GetLaserColor(0)
    gfx.BeginPath()
    gfx.FillColor(r, g, b, 96)
    gfx.MoveTo(s*0.1, s*0.1)
    gfx.LineTo(s*0.4, s*0.5)
    gfx.LineTo(s*0.1, s*0.9)
    gfx.LineTo(s*0.3, s*0.9)
    gfx.LineTo(s*0.6, s*0.5)
    gfx.LineTo(s*0.3, s*0.1)
    gfx.LineTo(s*0.1, s*0.1)
    gfx.Fill()
    
    local r, g, b = game.GetLaserColor(1)
    gfx.BeginPath()
    gfx.FillColor(r, g, b, 96)
    gfx.MoveTo(s*0.7, s*0.1)
    gfx.LineTo(s*0.4, s*0.5)
    gfx.LineTo(s*0.7, s*0.9)
    gfx.LineTo(s*0.9, s*0.9)
    gfx.LineTo(s*0.6, s*0.5)
    gfx.LineTo(s*0.9, s*0.1)
    gfx.LineTo(s*0.7, s*0.1)
    gfx.Fill()
    
    gfx.Restore()
    
    return x - s
end

draw_speed_icon = function(x, y, s)
    if speedMod == "" then return x end
    
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    gfx.FillColor(255, 255, 255)
    
    gfx.BeginPath()
    gfx.FontSize(15)
    gfx.Text(speedMod, x + s/2, y + s*0.3)
    
    gfx.BeginPath()
    gfx.FontSize(20)
    gfx.Text(speedModValue, x + s/2, y + s*0.65)
    
    return x - s
end

draw_hidsud_icon = function(x, y, s)
    if result.hidsud == nil then
        return x
    end
    
    gfx.FontSize(15)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    gfx.FillColor(255, 255, 255)
    
    gfx.BeginPath()
    gfx.Text("SUDDEN", x + s/2, y + s*0.13)
    gfx.Text("HIDDEN", x + s/2, y + s*0.62)
    
    gfx.BeginPath()
    gfx.FontSize(13)
    gfx.Text(string.format("%.2f fd %.1f", result.hidsud.suddenCutoff, result.hidsud.suddenFade), x + s/2, y + s*0.35)
    gfx.Text(string.format("%.2f fd %.1f", result.hidsud.hiddenCutoff, result.hidsud.hiddenFade), x + s/2, y + s*0.84)
    
    return x - s
end

draw_mir_ran_icon = function(x, y, s)
    if result.flags & 6 == 0 then return x end
    
    gfx.FontSize(20)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    gfx.FillColor(255, 255, 255)
    
    if result.flags & 2 ~= 0 then
        gfx.BeginPath()
        gfx.Text("MIR", x + s/2, y + s*0.3)
    end
    
    if result.flags & 4 ~= 0 then
        gfx.BeginPath()
        gfx.Text("RAN", x + s/2, y + s*0.7)
    end
    
    return x - s
end

---------------------
-- Main components --
---------------------

draw_title = function(x, y, w, h)
    local centerLineY = y+h*0.6
    
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    
    gfx.BeginPath()
    gfx.FillColor(255, 255, 255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
    
    gfx.FontSize(48)
    drawScaledText(chartTitle, x+w/2, centerLineY-18, w/2-5)
    
    drawLine(x+30, centerLineY, x+w-30,centerLineY, 1, 64, 64, 64)
    
    gfx.FontSize(27)
    drawScaledText(result.artist, x+w/2, centerLineY+28, w/2-5)
end

draw_chart_info = function(x, y, w, h, full)
    local jacket_size = 250
    
    local jacket_y = y+40
    
    if not full then
        jacket_y = y
        jacket_size = 300
    end
    
    local jacket_x = x+(w-jacket_size)/2
    
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    
    gfx.BeginPath()
    if jacketImg ~= nil then
        gfx.ImageRect(jacket_x, jacket_y, jacket_size, jacket_size, jacketImg, 1, 0)
    else
        gfx.BeginPath()
        gfx.FillColor(0, 0, 0, 128)
        gfx.Rect(jacket_x, jacket_y, jacket_size, jacket_size)
        gfx.Fill()
        
        gfx.BeginPath()
        gfx.FillColor(255, 255, 255, math.floor(40+80*waveParam(4)))
        gfx.FontSize(30)
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
        gfx.Text("No Image", x+w/2, jacket_y + jacket_size/2)
    end
    
    if full then
        gfx.BeginPath()
        gfx.FillColor(255, 255, 255)
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
        gfx.FontSize(30)
        gfx.Text(string.format("%s %02d", diffNames[result.difficulty + 1], result.level), x+w/2, y+30)
    else
        do
            gfx.FontSize(20)
            gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
            
            local level_text = string.format("%s %02d", diffNames[result.difficulty + 1], result.level)
            local _a, _b, level_text_width, _c = gfx.TextBounds(0, 0, level_text)
            local box_width = level_text_width
            
            local effector_text = ""
            
            if result.effector ~= nil and result.effector ~= "" then
                effector_text = string.format("  by %s", result.effector)
                
                gfx.FontSize(16)
                local _d, _e, effector_text_width, _f = gfx.TextBounds(0, 0, effector_text)
                box_width = box_width + effector_text_width
            end
            
            box_width = box_width + 10
            if box_width > jacket_size then box_width = jacket_size end
            
            gfx.BeginPath()
            gfx.FillColor(0, 0, 0, 200)
            gfx.RoundedRectVarying(jacket_x, jacket_y, box_width, 25, 0, 0, 5, 0)
            gfx.Fill()
            
            gfx.FillColor(255, 255, 255)
            gfx.BeginPath()
            gfx.FontSize(20)
            gfx.Text(level_text, jacket_x+5, jacket_y+22)
            
            if effector_text ~= "" then
                gfx.FontSize(16)
                drawScaledText(effector_text, jacket_x+level_text_width+5, jacket_y+21, jacket_size-level_text_width-10)
            end
        end
    
        local graph_height = jacket_size * 0.3
        local graph_y = jacket_y+jacket_size - graph_height
    
        gfx.BeginPath()
        gfx.FillColor(0,0,0,200)
        gfx.Rect(jacket_x, graph_y, jacket_size, graph_height)
        gfx.Fill()
        draw_gauge_graph(jacket_x, graph_y, jacket_size, graph_height)
        
        if gradeImg ~= nil then
            gfx.BeginPath()
            gfx.ImageRect(jacket_x+jacket_size-60*gradeAR, jacket_y+jacket_size-60, 60*gradeAR, 60, gradeImg, 1, 0)
        end
        
        gfx.BeginPath()
        gfx.FillColor(255,255,255)
        gfx.FontSize(20)
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_MIDDLE)
        gfx.Text(string.format("%.1f%%", result.gauge*100), jacket_x+jacket_size+10, jacket_y+jacket_size-graph_height*result.gauge)
        return
    end
    
    draw_y = jacket_y + jacket_size + 27
    
    if result.effector ~= nil and result.effector ~= "" then
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
        gfx.FillColor(255, 255, 255)
        gfx.FontSize(16)
        gfx.Text("Effected by", x+w/2, draw_y)
        gfx.FontSize(27)
        drawScaledText(result.effector, x+w/2, draw_y+24, w/2-5)
        draw_y = draw_y + 50
    end
    
    if result.illustrator ~= nil and result.illustrator ~= "" then
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
        gfx.FontSize(16)
        gfx.Text("Illustrated by", x+w/2, draw_y)
        gfx.FontSize(27)
        drawScaledText(result.illustrator, x+w/2, draw_y+24, w/2-5)
        draw_y = draw_y + 50
    end
end

draw_basic_hitstat = function(x, y, w, h, full)    
    local grade_width = 70 * gradeAR
    local stat_y = y
    local stat_gap = 15
    local stat_size = 30
    local stat_width = w-8
    
    local showRetryCount = (result.retryCount ~= nil and result.retryCount > 0) or (result.mission ~= nil and result.mission ~= "")
    
    if full then
        stat_gap = 6
        stat_size = 25
        stat_width = w-18
        
        if not showRetryCount then
            stat_gap = 25
            stat_y = stat_y + 15
        end
        
        gfx.BeginPath()
        gfx.ImageRect(x + (w-grade_width)/2 - 5, stat_y, grade_width, 70, gradeImg, 1, 0)
        stat_y = stat_y + 85
    else
        stat_y = y + 12
    
        if not showRetryCount then
            stat_gap = 30
        end
    end
    
    if clearTextBase ~= "" then
        if clearTextBase == "PERFECT" then gfx.FillColor(255, 255, math.floor(120+125*waveParam(2.0)))
        elseif clearTextBase == "FULL COMBO" then gfx.FillColor(255, 0, 200)
        elseif result.badge == 0 then
            local w = math.floor(128*waveParam(2.0))
            gfx.FillColor(255, w, w)
        else gfx.FillColor(255, 255, 255)
        end
        
        gfx.BeginPath()
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
        gfx.FontSize(20)
        gfx.Text(clearText, x+w/2 - 5, stat_y)
    end
    
    stat_y = stat_y + 50
    
    if result.score == 10000000 then
        gfx.FillColor(255, 255, math.floor(120+125*waveParam(2.0)))
    else
        gfx.FillColor(255, 255, 255)
    end
    
    if full then
        draw_score(result.score, x, stat_y, w, 72)
        stat_y = stat_y + 19
    else
        stat_y = stat_y + 10
        stat_gap = stat_gap - 8
        draw_score(result.score, x, stat_y, w, 88)
        stat_y = stat_y + 19
    end
    
    if highestScore > 0 then
        if highestScore > result.score then
            gfx.FillColor(255, 32, 32)
            draw_score(highestScore - result.score, x+w/2, stat_y, w/2, 25, "-")
        elseif highestScore == result.score then
            gfx.FillColor(128, 128, 128)
            draw_score(0, x+w/2, stat_y, w/2, 25, utf8.char(0xB1))
        else
            gfx.FillColor(32, 255, 32)
            draw_score(result.score - highestScore, x+w/2, stat_y, w/2, 25, "+")
        end
    end
    
    stat_y = stat_y + stat_gap
    
    gfx.FillColor(255, 255, 255)
    
    stat_y = draw_stat(x+4, stat_y, stat_width, stat_size, critText, result.perfects, "%d", 255, 150, 0)
    stat_y = draw_stat(x+4, stat_y, stat_width, stat_size, nearText, result.goods, "%d", 255, 0, 200)
    
    local early_late_width = w/2-20
    local late_x = x+stat_width-early_late_width
    draw_stat(late_x-early_late_width-10, stat_y, early_late_width, stat_size-6, "EARLY", result.earlies, "%d", 255, 0, 255)
    draw_stat(late_x, stat_y, early_late_width, stat_size-6, "LATE", result.lates, "%d", 0, 255, 255)
    
    stat_y = stat_y + stat_size + 5
    stat_y = draw_stat(x+4, stat_y, stat_width, stat_size, "ERROR", result.misses, "%d", 255, 0, 0)
    
    stat_y = draw_stat(x+4, stat_y+15, stat_width, stat_size, "MAX COMBO", result.maxCombo, "%d", 255, 255, 0)
    
    if showRetryCount then
        local retryCount = 0
        if result.retryCount ~= nil then retryCount = result.retryCount end
        
        stat_y = draw_stat(x+4, stat_y+15, stat_width, stat_size-6, "RETRY", retryCount, "%d")
        
        if result.mission ~= nil and result.mission ~= "" then
            gfx.LoadSkinFont("NotoSans-Regular.ttf")
            gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
            
            gfx.BeginPath()
            gfx.FontSize(16)
            gfx.Text(string.format("Mission: %s", result.mission), x+4, stat_y)
        end
    end
end

draw_graphs = function(x, y, w, h)    
    if not hasHitStat or hitDeltaScale == 0.0 then
        draw_left_graph(x, y, w, h)
    else
        draw_left_graph(x, y, w - w//4, h)
        draw_right_graph(x + (w - w//4), y, w//4, h)
    end
end

draw_guide = function(x, y, w, h, full)
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    
    local fxLText = "FX-L: more info"
    if full then
        fxLText = "FX-L: simple view"
    end
    
    local fxRText = "FX-R: toggle hiscore"
    
    gfx.FontSize(20)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
    
    gfx.BeginPath()
    gfx.FillColor(255, 255, 255, 96)
    gfx.Text(string.format("%s, %s", fxLText, fxRText), x+5, y+h)
end

draw_icons = function(x, y, w, h)    
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    
    local icon_x = x+w-h
    
    icon_x = draw_laser_icon(icon_x, y, h)
    icon_x = draw_speed_icon(icon_x, y, h)
    icon_x = draw_hidsud_icon(icon_x, y, h)
    icon_x = draw_mir_ran_icon(icon_x, y, h)
end

render = function(deltaTime)
    currTime = currTime + deltaTime
    
    -- Note: these keys are also used for viewing other players' scores on multiplayer.
    local fxLeft = game.GetButton(4)
    local fxRight = game.GetButton(5)
    
    if prevFXLeft ~= fxLeft then
        prevFXLeft = fxLeft
        
        if fxLeft then
            if result.uid == nil then showStatsHit = not showStatsHit end
            game.PlaySample("menu_click")
        end
    end
    
    if prevFXRight ~= fxRight then
        prevFXRight = fxRight
        
        if fxRight then
            if result.uid == nil then showHiScore = not showHiScore end
            game.PlaySample("menu_click")
        end
    end

    local resx,resy = game.GetResolution()
    
    if resx ~= currResX or resy ~= currResY or showHiScore ~= prevShowHiScore then
        prevShowHiScore = showHiScore
        
        if showHiScore then
            desw = 770
        else
            desw = 500
        end
        
        local scaleX = resx / desw
        local scaleY = resy / desh
    
        scale = math.min(scaleX, scaleY)
        
        if scaleX > scaleY then
            moveX = resx / (2*scale) - desw / 2
            moveY = 0
        else
            moveX = 0
            moveY = resy / (2*scale) - desh / 2
        end
        
        currResX = resX
        currResY = resY
    end
    
    -- For better screenshot display
    gfx.BeginPath()
    gfx.FillColor(0, 0, 0)
    gfx.Rect(0, 0, resx, resy)
    gfx.Fill()
    
    -- Background image    
    gfx.BeginPath()
    gfx.ImageRect(0, 0, resx, resy, backgroundImage, 0.5, 0);
    gfx.Scale(scale,scale)
    gfx.Translate(moveX,moveY)
    
    gfx.BeginPath()
    gfx.Rect(0,0,500,800)
    gfx.FillColor(30,30,30,128)
    gfx.Fill()
    
    -- Result
    draw_title(0, 0, 500, 110)
    
    if showStatsHit then
        draw_chart_info(0, 120, 280, 420, true)
        draw_basic_hitstat(280, 120, 220, 420, true)
        draw_graphs(0, 540, 500, 210)
    else
        draw_chart_info(0, 120, 500, 310, false)
        draw_basic_hitstat(50, 430, 400, 400, false)
    end
    
    if showGuide then
        draw_guide(0, 750, 500, 50, showStatsHit)
    end
    
    if showIcons and result.isSelf ~= false then
        draw_icons(0, 750, 500, 50)
    end
    
    if showHiScore then
        draw_highscores(showStatsHit)
    end
    
    -- Applause SFX
    if result.badge > 1 and not played then
        game.PlaySample("applause")
        played = true
    end
    
    -- Screenshot notification
    shotTimer = math.max(shotTimer - deltaTime, 0)
    if shotTimer > 1 then
        draw_shotnotif(505,755);
    end
end

get_capture_rect = function()
    local x = moveX * scale
    local y = moveY * scale
    local w = 500 * scale
    local h = 800 * scale
    return x,y,w,h
end

screenshot_captured = function(path)
    shotTimer = 10;
    shotPath = path;
    game.PlaySample("shutter")
end
