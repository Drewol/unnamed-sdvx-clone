local score = 0
local combo = 0
local jacket = nil
local resx,resy = game.GetResolution()
local portrait = resy > resx
local desw = 1280 --design width
if portrait then desw = 720 end
local critLinePos = { 0.95, 0.73 };
local desh = desw * (resy / resx) --design height
local scale = resx / desw
local songInfoWidth = 400
local jacketWidth = 100
local comboScale = 1.0
local late = false
local earlateTimer = 0;
local earlateColors = { {255,255,0}, {0,255,255} }
local alertTimers = {-2,-2}
local title = nil
local artist = nil
local jacketFallback = gfx.CreateSkinImage("song_select/loading.png", 0)
local bottomFill = gfx.CreateSkinImage("fill_bottom.png",0)
local topFill = gfx.CreateSkinImage("fill_top.png",0)
local critAnim = gfx.CreateSkinImage("crit_anim.png",0)
local critCap = gfx.CreateSkinImage("crit_cap.png",0)
local critCapBack = gfx.CreateSkinImage("crit_cap_back.png",0)
local laserCursor = gfx.CreateSkinImage("pointer.png",0)
local laserCursorOverlay = gfx.CreateSkinImage("pointer_overlay.png",0)
local diffNames = {"NOV", "ADV", "EXH", "INF"}
local introTimer = 2
local outroTimer = 0
local clearTexts = {"TRACK FAILED", "TRACK COMPLETE", "TRACK COMPLETE", "FULL COMBO", "PERFECT" }
local yshift = 0
local critAnimTimer = 0
local critAnimHeight = 15 * scale
local clw, clh = gfx.ImageSize(critAnim)
local critAnimWidth = critAnimHeight * (clw / clh)
local ccw, cch = gfx.ImageSize(critCap)
local critCapHeight = critAnimHeight * (cch / clh)
local critCapWidth = critCapHeight * (ccw / cch)
local cursorWidth = 40 * scale
local cw, ch = gfx.ImageSize(laserCursor)
local cursorHeight = cursorWidth * (ch / cw)

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

drawSongInfo = function(deltaTime)
    if jacket == nil or jacket == jacketFallback then
        jacket = gfx.LoadImageJob(gameplay.jacketPath, jacketFallback)
    end
    gfx.Save()
    if portrait then gfx.Scale(0.7,0.7) end
    gfx.BeginPath()
    gfx.LoadSkinFont("segoeui.ttf")
    gfx.Translate(5,5) --upper left margin
    gfx.FillColor(20,20,20,200);
    gfx.Rect(0,0,songInfoWidth,100)
    gfx.Fill()
    gfx.BeginPath()
    gfx.ImageRect(0,0,jacketWidth,jacketWidth,jacket,1,0)
    --begin diff/level
    gfx.BeginPath()
    gfx.Rect(0,85,60,15)
    gfx.FillColor(0,0,0,200)
    gfx.Fill()
    gfx.BeginPath()
    gfx.FillColor(255,255,255)
    draw_stat(0,85,55,15,diffNames[gameplay.difficulty + 1], gameplay.level, "%02d")
    --end diff/level
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
    gfx.FontSize(30)
    local textX = jacketWidth + 10
    titleWidth = songInfoWidth - jacketWidth - 20
    gfx.Save()
    x1,y1,x2,y2 = gfx.TextBounds(0,0,gameplay.title)
    textscale = math.min(titleWidth / x2, 1)
    gfx.Translate(textX, 30)
    gfx.Scale(textscale, textscale)
    gfx.Text(gameplay.title, 0, 0)
    gfx.Restore()
    x1,y1,x2,y2 = gfx.TextBounds(0,0,gameplay.artist)
    textscale = math.min(titleWidth / x2, 1)
    gfx.Save()
    gfx.Translate(textX, 60)
    gfx.Scale(textscale, textscale)
    gfx.Text(gameplay.artist, 0, 0)
    gfx.Restore()
    gfx.FillColor(255,255,255)
    gfx.FontSize(20)
    gfx.Text(string.format("BPM: %.1f", gameplay.bpm), textX, 85)
    gfx.BeginPath()
    gfx.FillColor(0,150,255)
    gfx.Rect(jacketWidth,jacketWidth-10,(songInfoWidth - jacketWidth) * gameplay.progress,10)
    gfx.Fill()
    if game.GetButton(game.BUTTON_STA) then
      gfx.BeginPath()
      gfx.FillColor(20,20,20,200);
      gfx.Rect(100,100, songInfoWidth - 100, 20)
      gfx.Fill()
      gfx.FillColor(255,255,255)
      gfx.Text(string.format("HiSpeed: %.0f x %.1f = %.0f", 
      gameplay.bpm, gameplay.hispeed, gameplay.bpm * gameplay.hispeed), 
      textX, 115)
    end
    gfx.Restore()
end

drawBestDiff = function(deltaTime,x,y)
    if not gameplay.scoreReplays[1] then return end
    gfx.BeginPath()
    gfx.FontSize(40)
    difference = score - gameplay.scoreReplays[1].currentScore
    local prefix = ""
    gfx.FillColor(255,255,255)
    if difference < 0 then 
        gfx.FillColor(255,50,50)
        difference = math.abs(difference)
        prefix = "-"
    end
    gfx.Text(string.format("%s%08d", prefix, difference), x, y)
end

drawScore = function(deltaTime)
    gfx.BeginPath()
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.BeginPath()
    gfx.RoundedRectVarying(desw - 210,5,220,62,0,0,0,20)
    gfx.FillColor(20,20,20)
    gfx.StrokeColor(0,128,255)
    gfx.StrokeWidth(2)
    gfx.Fill()
    gfx.Stroke()
    gfx.Translate(-5,5) -- upper right margin
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_TOP)
    gfx.FontSize(60)
    gfx.Text(string.format("%08d", score),desw,0)
    drawBestDiff(deltaTime, desw, 66)
    gfx.Translate(5,-5) -- undo margin
end

drawGauge = function(deltaTime)
    local height = 1024 * scale * 0.35
    local width = 512 * scale * 0.35
    local posy = resy / 2 - height / 2
    local posx = resx - width * (1 - math.max(introTimer - 1, 0))
    if portrait then
        width = width * 0.8
        height = height * 0.8
        posy = posy - 30
        posx = resx - width * (1 - math.max(introTimer - 1, 0))
    end
    gfx.DrawGauge(gameplay.gauge, posx, posy, width, height, deltaTime)

	--draw gauge % label
	posx = posx / scale
	posx = posx + (100 * 0.35) 
	height = 880 * 0.35
	posy = posy / scale
	if portrait then
		height = height * 0.8;
	end

	posy = posy + (70 * 0.35) + height - height * gameplay.gauge
	gfx.BeginPath()
	gfx.Rect(posx-35, posy-10, 40, 20)
	gfx.FillColor(0,0,0,200)
	gfx.Fill()
	gfx.FillColor(255,255,255)
	gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE)
	gfx.FontSize(20)
	gfx.Text(string.format("%d%%", math.floor(gameplay.gauge * 100)), posx, posy )

end

drawCombo = function(deltaTime)
    if combo == 0 then return end
    local posx = desw / 2
    local posy = desh * critLinePos[1] - 100
    if portrait then posy = desh * critLinePos[2] - 150 end
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    if gameplay.comboState == 2 then
        gfx.FillColor(100,255,0) --puc
    elseif gameplay.comboState == 1 then
        gfx.FillColor(255,200,0) --uc
    else
        gfx.FillColor(255,255,255) --regular
    end
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.FontSize(70 * math.max(comboScale, 1))
    comboScale = comboScale - deltaTime * 3
    gfx.Text(tostring(combo), posx, posy)
end

drawEarlate = function(deltaTime)
    earlateTimer = math.max(earlateTimer - deltaTime,0)
    if earlateTimer == 0 then return nil end
    local alpha = math.floor(earlateTimer * 20) % 2
    alpha = alpha * 200 + 55
    gfx.BeginPath()
    gfx.FontSize(35)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_MIDDLE)
    local ypos = desh * critLinePos[1] - 150
    if portrait then ypos = desh * critLinePos[2] - 150 end
    if late then
        gfx.FillColor(0,255,255, alpha)
        gfx.Text("LATE", desw / 2, ypos)
    else
        gfx.FillColor(255,0,255, alpha)
        gfx.Text("EARLY", desw / 2, ypos)
    end
end

drawFill = function(deltaTime)
    bw,bh = gfx.ImageSize(bottomFill)
    bottomAspect = bh/bw
    bottomHeight = desw * bottomAspect
    gfx.Translate(0, (bottomHeight + 100) * math.max(introTimer - 1, 0))
    gfx.BeginPath()
    gfx.Rect(0, desh * critLinePos[2] + bottomHeight - 20, desw, 100)
    gfx.ImageRect(0, desh * critLinePos[2],  desw, bottomHeight, bottomFill, 1,0)
    gfx.Translate(0, (bottomHeight + 100) * -math.max(introTimer - 1, 0))


    gfx.BeginPath()
    tw,th = gfx.ImageSize(topFill)
    topAspect = th/tw
    topHeight = desw * topAspect
    local ar = desh/desw
    ar = ar - 1
    ar = ar / (16/9 - 1)
    ar = 1 - ar
    local yoff = ar * topHeight
    local retoff = ar * 20
    if ar < 0 then yoff = 0 end
    yoff = yoff + bottomHeight * math.max(introTimer - 1, 0)
    gfx.ImageRect(0,-yoff,desw, topHeight, topFill, 1, 0)
    gfx.BeginPath()
    return topHeight - yoff - retoff
end

drawAlerts = function(deltaTime)
    alertTimers[1] = math.max(alertTimers[1] - deltaTime,-2)
    alertTimers[2] = math.max(alertTimers[2] - deltaTime,-2)
    if alertTimers[1] > 0 then --draw left alert
        gfx.Save()
        local posx = desw / 2 - 350
        local posy = desh * critLinePos[1] - 135
        if portrait then 
            posy = desh * critLinePos[2] - 135 
            posx = 65
        end
        gfx.Translate(posx,posy)
        r,g,b = game.GetLaserColor(0)
        local alertScale = (-(alertTimers[1] ^ 2.0) + (1.5 * alertTimers[1])) * 5.0
        alertScale = math.min(alertScale, 1)
        gfx.Scale(1, alertScale)
        gfx.BeginPath()
        gfx.RoundedRectVarying(-50,-50,100,100,20,0,20,0)
        gfx.StrokeColor(r,g,b)
        gfx.FillColor(20,20,20)
        gfx.StrokeWidth(2)
        gfx.Fill()
        gfx.Stroke()
        gfx.BeginPath()
        gfx.FillColor(r,g,b)
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
        gfx.FontSize(90)
        gfx.Text("L",0,0)
        gfx.Restore()
    end
    if alertTimers[2] > 0 then --draw right alert
        gfx.Save()
        local posx = desw / 2 + 350
        local posy = desh * critLinePos[1] - 135
        if portrait then 
            posy = desh * critLinePos[2] - 135 
            posx = desw - 65
        end
        gfx.Translate(posx,posy)
        r,g,b = game.GetLaserColor(1)
        local alertScale = (-(alertTimers[2] ^ 2.0) + (1.5 * alertTimers[2])) * 5.0
        alertScale = math.min(alertScale, 1)
        gfx.Scale(1, alertScale)
        gfx.BeginPath()
        gfx.RoundedRectVarying(-50,-50,100,100,0,20,0,20)
        gfx.StrokeColor(r,g,b)
        gfx.FillColor(20,20,20)
        gfx.StrokeWidth(2)
        gfx.Fill()
        gfx.Stroke()
        gfx.BeginPath()
        gfx.FillColor(r,g,b)
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
        gfx.FontSize(90)
        gfx.Text("R",0,0)
        gfx.Restore()
    end
end

setupCritTransform = function()
    gfx.Translate(gameplay.critLine.x, gameplay.critLine.y)
    gfx.Rotate(-gameplay.critLine.rotation)
end

render_crit_base = function(deltaTime)
    critAnimTimer = critAnimTimer + deltaTime
    gfx.Save()
    
    local distFromCenter = resx / 2 - gameplay.critLine.x
    local dvx = math.cos(gameplay.critLine.rotation)
    local dvy = math.sin(gameplay.critLine.rotation)
    local rotOffset = math.sqrt(dvx * dvx + dvy * dvy) * distFromCenter

    --local rotOffset = -gameplay.critLine.rotation * resx * 0.25

    setupCritTransform()
    gfx.Translate(rotOffset, 0)
    
    gfx.BeginPath()
    gfx.Rect(-resx, 0, resx * 2, resy)
    gfx.FillColor(0, 0, 0, 225)
    gfx.Fill()

    local critWidth = resx * 0.8
    local numPieces = 1 + math.ceil(critWidth / (critAnimWidth * 2))
    local startOffset = critAnimWidth * ((critAnimTimer * 1.5) % 1)

    gfx.BeginPath()
    gfx.ImageRect(-critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight, critCapBack, 1, 0)
    gfx.Scale(-1, 1)
    gfx.BeginPath()
    gfx.ImageRect(-critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight, critCapBack, 1, 0)
    gfx.Scale(-1, 1)

    -- right side
    gfx.Scissor(0, -critAnimHeight / 2, critWidth / 2, critAnimHeight)
    for i = 1, numPieces do
        gfx.BeginPath()
        gfx.ImageRect(-critAnimWidth + startOffset + critAnimWidth * (i - 1), -critAnimHeight / 2, critAnimWidth, critAnimHeight, critAnim, 1, 0)
    end
    gfx.ResetScissor()
    -- left side
    gfx.Scissor(-critWidth / 2, -critAnimHeight / 2, critWidth / 2, critAnimHeight)
    for i = 1, numPieces do
        gfx.BeginPath()
        gfx.ImageRect(-startOffset - critAnimWidth * (i - 1), -critAnimHeight / 2, critAnimWidth, critAnimHeight, critAnim, 1, 0)
    end
    gfx.ResetScissor()

    gfx.BeginPath()
    gfx.ImageRect(-critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight, critCap, 1, 0)
    gfx.Scale(-1, 1)
    gfx.BeginPath()
    gfx.ImageRect(-critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight, critCap, 1, 0)
    gfx.Scale(-1, 1)

    gfx.Restore()
end

render_crit_overlay = function(deltaTime)
    gfx.Save()

    setupCritTransform()

    local drawCursor = function(i)
        local cursor = gameplay.critLine.cursors[i]
        local r, g, b = game.GetLaserColor(i)
        local pos, skew = cursor.pos, cursor.skew

        gfx.BeginPath()
        gfx.SkewX(skew)
        gfx.SetImageTint(r, g, b)
        gfx.ImageRect(pos - cursorWidth / 2, -cursorHeight / 2, cursorWidth, cursorHeight, laserCursor, cursor.alpha, 0)
        gfx.SetImageTint(255, 255, 255)
        gfx.ImageRect(pos - cursorWidth / 2, -cursorHeight / 2, cursorWidth, cursorHeight, laserCursorOverlay, cursor.alpha, 0)
        gfx.SkewX(-skew)
    end

    drawCursor(0)
    drawCursor(1)

    gfx.SetImageTint(255, 255, 255)
    gfx.Restore()
end

render = function(deltaTime)
    gfx.ResetTransform()
    if introTimer > 0 then
        gfx.BeginPath()
        gfx.Rect(0,0,resx,resy)
        gfx.FillColor(0,0,0, math.floor(255 * math.min(introTimer, 1)))
        gfx.Fill()
    end
    gfx.Scale(scale,scale)
    if portrait then yshift = drawFill(deltaTime) end
    gfx.Translate(0, yshift - 150 * math.max(introTimer - 1, 0))
    drawSongInfo(deltaTime)
    drawScore(deltaTime)
    gfx.Translate(0, -yshift + 150 * math.max(introTimer - 1, 0))
    drawGauge(deltaTime)
    drawEarlate(deltaTime)
    drawCombo(deltaTime)
    drawAlerts(deltaTime)
end

render_intro = function(deltaTime)
    if not game.GetButton(game.BUTTON_STA) then
        introTimer = introTimer - deltaTime
    end
    introTimer = math.max(introTimer, 0)
    return introTimer <= 0
end

render_outro = function(deltaTime, clearState)
    if clearState == 0 then return true end
    gfx.ResetTransform()
    gfx.BeginPath()
    gfx.Rect(0,0,resx,resy)
    gfx.FillColor(0,0,0, math.floor(127 * math.min(outroTimer, 1)))
    gfx.Fill()
    gfx.Scale(scale,scale)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    gfx.FillColor(255,255,255, math.floor(255 * math.min(outroTimer, 1)))
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.FontSize(70)
    gfx.Text(clearTexts[clearState], desw / 2, desh / 2)
    outroTimer = outroTimer + deltaTime
    return outroTimer > 2, 1 - outroTimer
end

update_score = function(newScore)
    score = newScore
end

update_combo = function(newCombo)
    combo = newCombo
    comboScale = 1.5
end

near_hit = function(wasLate) --for updating early/late display
    late = wasLate
    earlateTimer = 0.75
end

laser_alert = function(isRight) --for starting laser alert animations
    if isRight and alertTimers[2] < -1.5 then alertTimers[2] = 1.5
    elseif alertTimers[1] < -1.5 then alertTimers[1] = 1.5
    end
end