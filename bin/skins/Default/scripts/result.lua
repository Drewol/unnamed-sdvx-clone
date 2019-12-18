local jacket = nil
local resx,resy = game.GetResolution()
local scale = math.min(resx / 800, resy /800)
local gradeImg;
local lastGrade=-1;
local gradear = 1 --grade aspect ratio
local desw = 800
local desh = 800
local moveX = 0
local moveY = 0
if resx / resy > 1 then
    moveX = resx / (2*scale) - 400
else
    moveY = resy / (2*scale) - 400
end
local diffNames = {"NOV", "ADV", "EXH", "INF"}
local backgroundImage = gfx.CreateSkinImage("bg.png", 1);
game.LoadSkinSample("applause")
local played = false
local shotTimer = 0;
local shotPath = "";
game.LoadSkinSample("shutter")


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

draw_shotnotif = function(x,y)
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

draw_line = function(x1,y1,x2,y2,w,r,g,b)
    gfx.BeginPath()
    gfx.MoveTo(x1,y1)
    gfx.LineTo(x2,y2)
    gfx.StrokeColor(r,g,b)
    gfx.StrokeWidth(w)
    gfx.Stroke()
end

draw_highscores = function()
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    gfx.FontSize(30)
    gfx.Text("Highscores:",510,30)
    for i,s in ipairs(result.highScores) do
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
        gfx.BeginPath()
        local ypos =  60 + (i - 1) * 80
        if result.displayIndex ~= nil and result.displayIndex + 1 == i then
            gfx.RoundedRectVarying(510-20,ypos, 280, 70,0,0,35,0)
        else
            gfx.RoundedRectVarying(510,ypos, 280, 70,0,0,35,0)
        end
        if result.uid ~= nil and result.uid == s.uid then
            gfx.FillColor(60,60,60)
            gfx.StrokeColor(0,128,255)
        else
            gfx.FillColor(15,15,15)
            gfx.StrokeColor(0,128,255)
        end
        gfx.Fill()
        gfx.Stroke()
        gfx.BeginPath()
        gfx.FillColor(255,255,255)
        gfx.FontSize(25)
        gfx.Text(string.format("#%d",i), 515, ypos + 25)
        gfx.LoadSkinFont("NovaMono.ttf")
        gfx.FontSize(60)
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_TOP)
        gfx.Text(string.format("%08d", s.score), 650, ypos - 4)
        gfx.LoadSkinFont("NotoSans-Regular.ttf")
        gfx.FontSize(20)
        if s.timestamp > 0 then
            gfx.Text(os.date("%Y-%m-%d %H:%M:%S", s.timestamp), 650, ypos + 45)
        end
        if s.name ~= nil then
            gfx.Text(s.name, 650, ypos + 45)
        end
    end
end

draw_graph = function(x,y,w,h)
    gfx.BeginPath()
    gfx.Rect(x,y,w,h)
    gfx.FillColor(0,0,0,210)
    gfx.Fill()
    gfx.BeginPath()
    gfx.MoveTo(x,y + h - h * result.gaugeSamples[1])
    for i = 2, #result.gaugeSamples do
        gfx.LineTo(x + i * w / #result.gaugeSamples,y + h - h * result.gaugeSamples[i])
    end
	if result.flags & 1 ~= 0 then
		gfx.StrokeWidth(2.0)
		gfx.StrokeColor(255,80,0)
		gfx.Stroke()
	else
		gfx.StrokeWidth(2.0)
		gfx.StrokeColor(0,180,255)
		gfx.Scissor(x, y + h * 0.3, w, h * 0.7)
		gfx.Stroke()
		gfx.ResetScissor()
		gfx.Scissor(x,y,w,h*0.3)
		gfx.StrokeColor(255,0,255)
		gfx.Stroke()
		gfx.ResetScissor()
	end
end

render = function(deltaTime, showStats)
	gfx.BeginPath()
    gfx.ImageRect(0, 0, resx, resy, backgroundImage, 0.5, 0);
    gfx.Scale(scale,scale)
    gfx.Translate(moveX,moveY)
    if result.badge > 1 and not played then
        game.PlaySample("applause")
        played = true
    end
    if jacket == nil then
        jacket = gfx.CreateImage(result.jacketPath, 0)
    end
    if not gradeImg or result.grade ~= lastGrade then
        gradeImg = gfx.CreateSkinImage(string.format("score/%s.png", result.grade),0)
        local gradew,gradeh = gfx.ImageSize(gradeImg)
        gradear = gradew/gradeh
        lastGrade = result.grade
    end
    gfx.BeginPath()
    gfx.Rect(0,0,500,800)
    gfx.FillColor(30,30,30)
    gfx.Fill()

    --Title and jacket
    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    gfx.BeginPath()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER)
    gfx.FontSize(30)
    gfx.Text(result.title, 250, 35)
    gfx.FontSize(20)
    gfx.Text(result.artist, 250, 60)
    if jacket then
        gfx.ImageRect(100,90,300,300,jacket,1,0)
    end
    gfx.BeginPath()
    gfx.Rect(100,90,60,20)
    gfx.FillColor(0,0,0,200)
    gfx.Fill()
    gfx.BeginPath()
    gfx.FillColor(255,255,255)
    draw_stat(100,90,55,20,diffNames[result.difficulty + 1], result.level, "%02d")
    draw_graph(100,300,300,90)
    gfx.BeginPath()
    gfx.ImageRect(400 - 60 * gradear,330,60 * gradear,60,gradeImg,1,0)
    gfx.BeginPath()
    gfx.FontSize(20)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_MIDDLE)
    gfx.Text(string.format("%d%%", math.floor(result.gauge * 100)),410,390 - 90 * result.gauge)

	if result.autoplay then
	    gfx.FontSize(50)
		gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
		gfx.Text("Autoplay", 250, 345)
	end

    --Score data
    gfx.BeginPath()
    gfx.RoundedRect(120,400,500 - 240,60,30);
    gfx.FillColor(15,15,15)
    gfx.StrokeColor(0,128,255)
    gfx.StrokeWidth(2)
    gfx.Fill()
    gfx.Stroke()
    gfx.BeginPath()
    gfx.FillColor(255,255,255)
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.FontSize(60)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_TOP)
    gfx.Text(string.format("%08d", result.score), 250, 400)
    --Left Column
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT)
    gfx.FontSize(30)
    gfx.Text("CRIT:",10, 500);
    gfx.Text("NEAR:",10, 540);
    gfx.Text("ERROR:",10, 580);
    --Right Column
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT)
    gfx.FontSize(30)
    gfx.Text(string.format("%d", result.perfects),480, 500);
    gfx.Text(string.format("%d", result.goods),480, 540);
    gfx.Text(string.format("%d", result.misses),480, 580);
    --Separator Lines
    draw_line(10,505,480,505, 1.5, 255,150,0)
    draw_line(10,545,480,545, 1.5, 255,0,200)
    draw_line(10,585,480,585, 1.5, 255,0,0)

    local staty = 620
    staty = draw_stat(10,staty,470,30,"MAX COMBO", result.maxCombo, "%d")
    staty = staty + 10
    staty = draw_stat(10,staty,470,25,"EARLY", result.earlies, "%d",255,0,255)
    staty = draw_stat(10,staty,470,25,"LATE", result.lates, "%d",0,255,255)
    staty = staty + 10
    staty = draw_stat(10,staty,470,25,"MEDIAN DELTA", result.medianHitDelta, "%dms")
    staty = draw_stat(10,staty,470,25,"MEAN DELTA", result.meanHitDelta, "%.1fms")


    draw_highscores()

    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    shotTimer = math.max(shotTimer - deltaTime, 0)
    if shotTimer > 1 then
        draw_shotnotif(505,755);
    end

end