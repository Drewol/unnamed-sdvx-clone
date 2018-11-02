local score = 0
local combo = 0
local jacket = nil
local resx,resy = game.GetResolution()
local scale = resx / 1280
local desw = 1280 --design width
local desh = desw * (resy / resx) --design height
local songInfoWidth = 400
local jacketWidth = 100
local comboScale = 1.0
local diffStrings = {"NOVICE","ADVANCED","EXHAUST","INFINITE","MAXIMUM"}

-- Responsive UI variables
-- Aspect Ratios
local aspectFloat = 1.850
local aspectRatio = "widescreen"
local landscapeWidescreenRatio = 1.850
local landscapeStandardRatio = 1.500
local portraitWidescreenRatio = 0.5625

-- Responsive sizes
local fifthX = 0
local fourthX= 0
local thirdX = 0
local halfX  = 0
local fullX  = 0

local fifthY = 0
local fourthY= 0
local thirdY = 0
local halfY  = 0
local fullY  = 0

adjustScreen = function(x,y)
  local a = x/y;
  if x >= y and a <= landscapeStandardRatio then
    aspectRatio = "landscapeStandard"
    aspectFloat = 1.1
  elseif x >= y and landscapeStandardRatio <= a and a <= landscapeWidescreenRatio then
    aspectRatio = "landscapeWidescreen"
    aspectFloat = 1.2
    scale = resx / 1280
  elseif x <= y and portraitWidescreenRatio <= a and a < landscapeStandardRatio then
    aspectRatio = "PortraitWidescreen"
    aspectFloat = 0.5
    scale = resy / 1380
  else
    aspectRatio = "landscapeWidescreen"
    aspectFloat = 1.0
  end
  fifthX = x/5
  fourthX= x/4
  thirdX = x/3
  halfX  = x/2
  fullX  = x

  fifthY = y/5
  fourthY= y/4
  thirdY = y/3
  halfY  = y/2
  fullY  = y
end


drawSongInfo = function(deltaTime)
    gfx.LoadSkinFont("segoeui.ttf")
    local titleLabel = gfx.CreateLabel(gameplay.title, 30, 0)
    local artistLabel = gfx.CreateLabel(gameplay.artist, 20, 0)
    local difficultyLabel = gfx.CreateLabel(string.format("%s %s - BPM: %s",gameplay.level,diffStrings[gameplay.difficulty + 1],gameplay.bpm), 16, 0)
    local width = 0
    if jacket == nil then
        jacket = gfx.CreateImage(gameplay.jacketPath, 0)
    end
    gfx.BeginPath()
    gfx.LoadSkinFont("segoeui.ttf")
    if aspectRatio == "PortraitWidescreen" then
      jacketWidth = 100
      width = thirdX
      gfx.Translate(5,fifthY/2)
    else
      jacketWidth = 150
      width = fourthX
      gfx.Translate(5,5)
    end
     --upper left margin
    --gfx.FillColor(100,100,100);
    --gfx.Rect(0,0,songInfoWidth,100)
    --gfx.Fill()
    gfx.BeginPath()
    -- Border
    gfx.BeginPath()
    gfx.RoundedRectVarying(0,0, width, jacketWidth,0,0,30,0)
    gfx.FillColor(30,30,30)
    gfx.StrokeColor(0,128,255)
    gfx.StrokeWidth(3)
    gfx.Fill()
    gfx.Stroke()
    gfx.BeginPath()
    if jacket then
        gfx.ImageRect(0,0,jacketWidth,jacketWidth,jacket,1,0)
    end
    --Text within info
    local textX = jacketWidth + 4
    gfx.BeginPath()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
    gfx.FontSize(30)
    gfx.DrawLabel(titleLabel, textX, 0, width-textX-4);
    gfx.DrawLabel(artistLabel, textX, 35, width-textX-4);
    --Difficulty Text
    gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
    gfx.DrawLabel(difficultyLabel,textX,jacketWidth-20, width-textX-30)
    gfx.BeginPath()
    gfx.FillColor(20,20,20)
    gfx.Rect(jacketWidth,jacketWidth-35,(width - jacketWidth-2),10)
    gfx.Fill()
    gfx.BeginPath()
    gfx.FillColor(0,150,255)
    gfx.Rect(jacketWidth,jacketWidth-35,(width - jacketWidth-2) * gameplay.progress,10)
    gfx.Fill()
    if aspectRatio == "PortraitWidescreen" then
      gfx.Translate(-5,-fifthY/2)
    else
      gfx.Translate(-5,-5)
    end
end

drawScore = function(deltaTime)
    gfx.BeginPath()
    gfx.LoadSkinFont("NovaMono.ttf")
    if aspectRatio == "PortraitWidescreen" then
      width = thirdX
      gfx.Translate(resx-270,fifthY/2)
    else
      width = fourthX
      gfx.Translate(resx-270,5)
    end
    gfx.BeginPath()
    gfx.StrokeWidth(3)
    gfx.StrokeColor(0,128,255)
    gfx.FillColor(15,15,15)
    gfx.RoundedRect(0,0,270,60,30);
    gfx.Fill()
    gfx.Stroke()
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
    gfx.FontSize(60)
    gfx.FillColor(255,255,255)
    gfx.Text(string.format("%08d", score),35,0)

    if aspectRatio == "PortraitWidescreen" then
      gfx.Translate(-(resx-270),-(fifthY/2))
    else
      gfx.Translate(-(resx-270),-5)
    end -- undo margin
end

drawGauge = function(deltaTime)
    local height = 1024 * scale * 0.35
    local width = 512 * scale * 0.35
    local posx = 0
    local posy = 0
    if aspectRatio == "PortraitWidescreen" then
      posy = resy / 2 - (height / 4) * 3
    else
      posy = resy / 2 - height / 2
    end
    posx = resx - width
    gfx.DrawGauge(gameplay.gauge, posx, posy, width, height, deltaTime);
end

drawCombo = function(deltaTime)
    local fontSize = 50
    local posx = resx / 2
    local posy = fifthY*4
    if aspectRatio == "PortraitWidescreen" then
      fontSize = 70
      posy = fifthY*3
    else
      fontSize = 50
    end
    if combo == 0 then return end
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
    gfx.FontSize(fontSize * math.max(comboScale, 1))
    comboScale = comboScale - deltaTime * 3
    gfx.Text(tostring(combo), posx, posy)
end

render = function(deltaTime)
    resx,resy = game.GetResolution()
    adjustScreen(resx, resy)
    drawSongInfo(deltaTime)
    drawScore(deltaTime)
    drawGauge(deltaTime)
    drawCombo(deltaTime)
end

update_score = function(newScore)
    score = newScore
end

update_combo = function(newCombo)
    combo = newCombo
    comboScale = 1.5
end

near_hit = function(wasLate) --for updating early/late display

end

laser_alert = function(isRight) --for starting laser alert animations
  if isRight == true then
    gfx.BeginPath()
    gfx.StrokeWidth(3)
    gfx.StrokeColor(0,128,255)
    gfx.FillColor(15,15,15)
    gfx.RoundedRect(0,0,1000,1000,30);
    gfx.Fill()
    gfx.Stroke()
  else
    gfx.BeginPath()
    gfx.StrokeWidth(3)
    gfx.StrokeColor(0,128,255)
    gfx.FillColor(15,15,15)
    gfx.RoundedRect(0,0,1000,1000,30);
    gfx.Fill()
    gfx.Stroke()
  end
end
