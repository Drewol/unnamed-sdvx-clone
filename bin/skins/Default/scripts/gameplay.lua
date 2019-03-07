local RECT_FILL = "fill"
local RECT_STROKE = "stroke"
local RECT_FILL_STROKE = RECT_FILL .. RECT_STROKE

gfx._ImageAlpha = 1

gfx._FillColor = gfx.FillColor
gfx._StrokeColor = gfx.StrokeColor
gfx._SetImageTint = gfx.SetImageTint

-- we aren't even gonna overwrite it, it's just dead to us
gfx.SetImageTint = nil

function gfx.FillColor(r, g, b, a)
    r = math.floor(r or 255)
    g = math.floor(g or 255)
    b = math.floor(b or 255)
    a = math.floor(a or 255)

    gfx._ImageAlpha = a / 255
    gfx._FillColor(r, g, b, a)
    gfx._SetImageTint(r, g, b)
end

function gfx.StrokeColor(r, g, b)
    r = math.floor(r or 255)
    g = math.floor(g or 255)
    b = math.floor(b or 255)

    gfx._StrokeColor(r, g, b)
end

function gfx.DrawRect(kind, x, y, w, h)
    local doFill = kind == RECT_FILL or kind == RECT_FILL_STROKE
    local doStroke = kind == RECT_STROKE or kind == RECT_FILL_STROKE

    local doImage = not (doFill or doStroke)

    gfx.BeginPath()

    if doImage then
        gfx.ImageRect(x, y, w, h, kind, gfx._ImageAlpha, 0)
    else
        gfx.Rect(x, y, w, h)
        if doFill then gfx.Fill() end
        if doStroke then gfx.Stroke() end
    end
end

local buttonStates = { }
local buttonsInOrder = {
    game.BUTTON_BTA,
    game.BUTTON_BTB,
    game.BUTTON_BTC,
    game.BUTTON_BTD,
    
    game.BUTTON_FXL,
    game.BUTTON_FXR,

    game.BUTTON_STA,
}

function UpdateButtonStatesAfterProcessed()
    for i = 1, 6 do
        local button = buttonsInOrder[i]
        buttonStates[button] = game.GetButton(button)
    end
end

game.GetButtonPressed = function(button)
    return game.GetButton(button) and not buttonStates[button]
end

-- -------------------------------------------------------------------------- --
-- Global data used by many things:                                           --
local resx, resy -- The resolution of the window
local portrait -- whether the window is in portrait orientation
local desw, desh -- The resolution of the deisign
local scale -- the scale to get from design to actual units

local jacketFallback = gfx.CreateSkinImage("song_select/loading.png", 0)
local bottomFill = gfx.CreateSkinImage("console/console.png", 0)
local topFill = gfx.CreateSkinImage("fill_top.png", 0)
local critAnim = gfx.CreateSkinImage("crit_anim.png", 0)
local critCap = gfx.CreateSkinImage("crit_cap.png", 0)
local critCapBack = gfx.CreateSkinImage("crit_cap_back.png", 0)
local laserCursor = gfx.CreateSkinImage("pointer.png", 0)
local laserCursorOverlay = gfx.CreateSkinImage("pointer_overlay.png", 0)

local ioConsoleDetails = {
    gfx.CreateSkinImage("console/detail_left.png", 0),
    gfx.CreateSkinImage("console/detail_right.png", 0),
}

local consoleAnimImages = {
    gfx.CreateSkinImage("console/glow_bta.png", 0),
    gfx.CreateSkinImage("console/glow_btb.png", 0),
    gfx.CreateSkinImage("console/glow_btc.png", 0),
    gfx.CreateSkinImage("console/glow_btd.png", 0),
    
    gfx.CreateSkinImage("console/glow_fxl.png", 0),
    gfx.CreateSkinImage("console/glow_fxr.png", 0),

    gfx.CreateSkinImage("console/glow_voll.png", 0),
    gfx.CreateSkinImage("console/glow_volr.png", 0),
}

local introTimer = 2
local outroTimer = 0

local alertTimers = {-2,-2}

local earlateTimer = 0
local earlateColors = { {255,255,0}, {0,255,255} }

local critAnimTimer = 0

local consoleAnimSpeed = 10
local consoleAnimTimers = { 0, 0, 0, 0, 0, 0, 0, 0 }

local score = 0
local combo = 0
local jacket = nil
local critLinePos = { 0.95, 0.73 };
local songInfoWidth = 400
local jacketWidth = 100
local comboScale = 1.0
local late = false
local title = nil
local artist = nil
local diffNames = {"NOV", "ADV", "EXH", "INF"}
local clearTexts = {"TRACK FAILED", "TRACK COMPLETE", "TRACK COMPLETE", "FULL COMBO", "PERFECT" }
-- -------------------------------------------------------------------------- --
-- IsUserInputActive:                                                         --
-- Used to determine if (valid) controller input is happening.                --
-- Valid meaning that laser motion will not return true unless the laser is   --
--  active in gameplay as well.                                               --
-- This restriction is not applied to buttons.                                --
-- The player may press their buttons whenever and the function returns true. --
-- Lane starts at 1 and ends with 8.                                          --
function IsUserInputActive(lane)
    if lane < 7 then
        return game.GetButton(buttonsInOrder[lane])
    end
    return gameplay.IsLaserHeld(lane - 7)
end
-- -------------------------------------------------------------------------- --
-- SetFillToLaserColor:                                                       --
-- Sets the current fill color to the laser color of the given index.         --
-- An optional alpha value may be given as well.                              --
-- Index may be 1 or 2.                                                       --
function SetFillToLaserColor(index, alpha)
    alpha = math.floor(alpha or 255)
    local r, g, b = game.GetLaserColor(index - 1)
    gfx.FillColor(r, g, b, alpha)
end
-- -------------------------------------------------------------------------- --
-- ResetLayoutInformation:                                                    --
-- Resets the layout values used by the skin.                                 --
function ResetLayoutInformation()
    resx, resy = game.GetResolution()
    portrait = resy > resx
    desw = portrait and 720 or 1280 
    desh = desw * (resy / resx)
    scale = resx / desw
end
-- -------------------------------------------------------------------------- --
-- render:                                                                    --
-- The primary & final render call.                                           --
-- Use this to render basically anything that isn't the crit line or the      --
--  intro/outro transitions.                                                  --
function render(deltaTime)
    -- make sure that our transform is cleared, clean working space
    -- TODO: this shouldn't be necessary!!!
    gfx.ResetTransform()

    -- While the intro timer is running, we fade in from black
    if introTimer > 0 then
        gfx.FillColor(0, 0, 0, math.floor(255 * math.min(introTimer, 1)))
        gfx.DrawRect(RECT_FILL, 0, 0, resx, resy)
    end
    
    gfx.Scale(scale, scale)
    local yshift = 0

    -- In portrait, we draw a banner across the top.
    -- The rest of the UI needs to be drawn below that banner.
    -- TODO: this isn't how it'll work in the long run, I don't think.
    if portrait then yshift = DrawBanner(deltaTime) end

    gfx.Translate(0, yshift - 150 * math.max(introTimer - 1, 0))
    drawSongInfo(deltaTime)
    drawScore(deltaTime)
    gfx.Translate(0, -yshift + 150 * math.max(introTimer - 1, 0))
    drawGauge(deltaTime)
    drawEarlate(deltaTime)
    drawCombo(deltaTime)
    drawAlerts(deltaTime)
end
-- -------------------------------------------------------------------------- --
-- SetUpCritTransform:                                                        --
-- Utility function which aligns the graphics transform to the center of the  --
--  crit line on screen, rotation include.                                    --
-- This function resets the graphics transform, it's up to the caller to      --
--  save the transform if needed.                                             --
function SetUpCritTransform()
    -- start us with a clean empty transform
    gfx.ResetTransform()
    -- translate and rotate accordingly
    gfx.Translate(gameplay.critLine.x, gameplay.critLine.y)
    gfx.Rotate(-gameplay.critLine.rotation)
end
-- -------------------------------------------------------------------------- --
-- GetCritLineCenteringOffset:                                                --
-- Utility function which returns the magnitude of an offset to center the    --
--  crit line on the screen based on its position and rotation.               --
function GetCritLineCenteringOffset()
    local distFromCenter = resx / 2 - gameplay.critLine.x
    local dvx = math.cos(gameplay.critLine.rotation)
    local dvy = math.sin(gameplay.critLine.rotation)
    return math.sqrt(dvx * dvx + dvy * dvy) * distFromCenter
end
-- -------------------------------------------------------------------------- --
-- render_crit_base:                                                          --
-- Called after rendering the highway and playable objects, but before        --
--  the built-in hit effects.                                                 --
-- This is the first render function to be called each frame.                 --
-- This call resets the graphics transform, it's up to the caller to          --
--  save the transform if needed.                                             --
function render_crit_base(deltaTime)
    -- Kind of a hack, but here (since this is the first render function
    --  that gets called per frame) we update the layout information.
    -- This means that the player can resize their window and
    --  not break everything.
    ResetLayoutInformation()

    critAnimTimer = critAnimTimer + deltaTime
    SetUpCritTransform()
    
    -- Figure out how to offset the center of the crit line to remain
    --  centered on the players screen.
    local xOffset = GetCritLineCenteringOffset()
    gfx.Translate(xOffset, 0)
    
    -- Draw a transparent black overlay below the crit line.
    -- This darkens the play area as it passes.
    gfx.FillColor(0, 0, 0, 200)
    gfx.DrawRect(RECT_FILL, -resx, 0, resx * 2, resy)

    -- The absolute width of the crit line itself.
    -- we check to see if we're playing in portrait mode and
    --  change the width accordingly.
    local critWidth = resx * (portrait and 1 or 0.8)
    
    -- get the scaled dimensions of the crit line pieces
    local clw, clh = gfx.ImageSize(critAnim)
    local critAnimHeight = 15 * scale
    local critAnimWidth = critAnimHeight * (clw / clh)

    local ccw, cch = gfx.ImageSize(critCap)
    local critCapHeight = critAnimHeight * (cch / clh)
    local critCapWidth = critCapHeight * (ccw / cch)

    -- draw the back half of the caps at each end
    do
        gfx.FillColor(255, 255, 255)
        -- left side
        gfx.DrawRect(critCapBack, -critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight)
        gfx.Scale(-1, 1) -- scale to flip horizontally
        -- right side
        gfx.DrawRect(critCapBack, -critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight)
        gfx.Scale(-1, 1) -- unflip horizontally
    end

    -- render the core of the crit line
    do
        -- The crit line is made up of many small pieces scrolling outward.
        -- Calculate how many pieces, starting at what offset, are require to
        --  completely fill the space with no gaps from edge to center.
        local numPieces = 1 + math.ceil(critWidth / (critAnimWidth * 2))
        local startOffset = critAnimWidth * ((critAnimTimer * 1.5) % 1)

        -- left side
        -- Use a scissor to limit the drawable area to only what should be visible
        gfx.Scissor(-critWidth / 2, -critAnimHeight / 2, critWidth / 2, critAnimHeight)
        for i = 1, numPieces do
            gfx.DrawRect(critAnim, -startOffset - critAnimWidth * (i - 1), -critAnimHeight / 2, critAnimWidth, critAnimHeight)
        end
        gfx.ResetScissor()

        -- right side
        -- exactly the same, but in reverse
        gfx.Scissor(0, -critAnimHeight / 2, critWidth / 2, critAnimHeight)
        for i = 1, numPieces do
            gfx.DrawRect(critAnim, -critAnimWidth + startOffset + critAnimWidth * (i - 1), -critAnimHeight / 2, critAnimWidth, critAnimHeight)
        end
        gfx.ResetScissor()
    end

    -- Draw the front half of the caps at each end
    do
        gfx.FillColor(255, 255, 255)
        -- left side
        gfx.DrawRect(critCap, -critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight)
        gfx.Scale(-1, 1) -- scale to flip horizontally
        -- right side
        gfx.DrawRect(critCap, -critWidth / 2 - critCapWidth / 2, -critCapHeight / 2, critCapWidth, critCapHeight)
        gfx.Scale(-1, 1) -- unflip horizontally
    end

    -- we're done, reset graphics stuffs
    gfx.FillColor(255, 255, 255)
    gfx.ResetTransform()
end
-- -------------------------------------------------------------------------- --
-- render_crit_overlay:                                                       --
-- Called after rendering built-int crit line effects.                        --
-- Use this to render laser cursors or an IO Console in portrait mode!        --
-- This call resets the graphics transform, it's up to the caller to          --
--  save the transform if needed.                                             --
render_crit_overlay = function(deltaTime)
    SetUpCritTransform()

    -- Figure out how to offset the center of the crit line to remain
    --  centered on the players screen.
    local xOffset = GetCritLineCenteringOffset()

    -- When in portrait, we can draw the console at the bottom
    if portrait then
        -- We're going to make temporary modifications to the transform
        gfx.Save()
        gfx.Translate(xOffset * 0.5, 0)

        local bfw, bfh = gfx.ImageSize(bottomFill)

        local distBetweenKnobs = 0.446
        local distCritVertical = 0.098

        local ioFillTx = bfw / 2
        local ioFillTy = bfh * distCritVertical -- 0.098

        -- The total dimensions for the console image
        local io_x, io_y, io_w, io_h = -ioFillTx, -ioFillTy, bfw, bfh

        -- Adjust the transform accordingly first
        local consoleFillScale = (resx * 0.775) / (bfw * distBetweenKnobs)
        gfx.Scale(consoleFillScale, consoleFillScale);

        -- Actually draw the fill
        gfx.FillColor(255, 255, 255)
        gfx.DrawRect(bottomFill, io_x, io_y, io_w, io_h)

        -- Then draw the details which need to be colored to match the lasers
        for i = 1, 2 do
            SetFillToLaserColor(i)
            gfx.DrawRect(ioConsoleDetails[i], io_x, io_y, io_w, io_h)
        end

        -- Draw the button press animations by overlaying transparent images
        gfx.GlobalCompositeOperation(gfx.BLEND_OP_LIGHTER)
        for i = 1, 6 do
            -- While a button is held, increment a timer.
            -- If not held, that timer is set back to 0
            if game.GetButton(buttonsInOrder[i]) then
                consoleAnimTimers[i] = consoleAnimTimers[i] + deltaTime * consoleAnimSpeed * 3.14 * 2
            else 
                consoleAnimTimers[i] = 0
            end

            -- If the timer is active, flash based on a sin wave
            local timer = consoleAnimTimers[i]
            if timer ~= 0 then
                local image = consoleAnimImages[i]
                local alpha = (math.sin(timer) * 0.5 + 0.5) * 0.5 + 0.25
                gfx.FillColor(255, 255, 255, alpha * 255);
                gfx.DrawRect(image, io_x, io_y, io_w, io_h)
            end
        end
        gfx.GlobalCompositeOperation(gfx.BLEND_OP_SOURCE_OVER)
        
        -- Undo those modifications
        gfx.Restore();
    end

    local cw, ch = gfx.ImageSize(laserCursor)
    local cursorWidth = 40 * scale
    local cursorHeight = cursorWidth * (ch / cw)

    -- draw each laser cursor
    for i = 1, 2 do
        local cursor = gameplay.critLine.cursors[i - 1]
        local pos, skew = cursor.pos, cursor.skew

        -- Add a kinda-perspective effect with a horizontal skew
        gfx.SkewX(skew)

        -- Draw the colored background with the appropriate laser color
        SetFillToLaserColor(i, cursor.alpha * 255)
        gfx.DrawRect(laserCursor, pos - cursorWidth / 2, -cursorHeight / 2, cursorWidth, cursorHeight)
        -- Draw the uncolored overlay on top of the color
        gfx.FillColor(255, 255, 255, cursor.alpha * 255)
        gfx.DrawRect(laserCursorOverlay, pos - cursorWidth / 2, -cursorHeight / 2, cursorWidth, cursorHeight)
        -- Un-skew
        gfx.SkewX(-skew)
    end

    -- We're done, reset graphics stuffs
    gfx.FillColor(255, 255, 255)
    gfx.ResetTransform()
end

function DrawBanner(deltaTime)
    local bannerWidth, bannerHeight = gfx.ImageSize(topFill)
    local actualHeight = desw * (bannerHeight / bannerWidth)

    gfx.FillColor(255, 255, 255)
    gfx.DrawRect(topFill, 0, 0, desw, actualHeight)

    return actualHeight
end
-- -------------------------------------------------------------------------- --
















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
    gfx.FillColor(255, 255, 255)
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