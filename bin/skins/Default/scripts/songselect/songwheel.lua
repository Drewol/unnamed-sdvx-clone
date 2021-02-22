--Horizontal alignment
TEXT_ALIGN_LEFT 	= 1
TEXT_ALIGN_CENTER 	= 2
TEXT_ALIGN_RIGHT 	= 4
--Vertical alignment
TEXT_ALIGN_TOP 		= 8
TEXT_ALIGN_MIDDLE	= 16
TEXT_ALIGN_BOTTOM	= 32
TEXT_ALIGN_BASELINE	= 64

local jacket = nil;
local selectedIndex = 1
local selectedDiff = 1
local songCache = {}
local ioffset = 0
local doffset = 0
local soffset = 0
local diffColors = {{0,0,255}, {0,255,0}, {255,0,0}, {255, 0, 255}}
local timer = 0
local effector = 0
local searchText = gfx.CreateLabel("",5,0)
local searchIndex = 1
local jacketFallback = gfx.CreateSkinImage("song_select/loading.png", 0)
local showGuide = game.GetSkinSetting("show_guide")
local legendTable = {
  {["labelSingleLine"] =  gfx.CreateLabel("DIFFICULTY SELECT",16, 0), ["labelMultiLine"] =  gfx.CreateLabel("DIFFICULTY\nSELECT",16, 0), ["image"] = gfx.CreateSkinImage("legend/knob-left.png", 0)},
  {["labelSingleLine"] =  gfx.CreateLabel("MUSIC SELECT",16, 0),      ["labelMultiLine"] =  gfx.CreateLabel("MUSIC\nSELECT",16, 0),      ["image"] = gfx.CreateSkinImage("legend/knob-right.png", 0)},
  {["labelSingleLine"] =  gfx.CreateLabel("FILTER MUSIC",16, 0),      ["labelMultiLine"] =  gfx.CreateLabel("FILTER\nMUSIC",16, 0),      ["image"] = gfx.CreateSkinImage("legend/FX-L.png", 0)},
  {["labelSingleLine"] =  gfx.CreateLabel("SORT MUSIC",16, 0),        ["labelMultiLine"] =  gfx.CreateLabel("SORT\nMUSIC",16, 0),        ["image"] = gfx.CreateSkinImage("legend/FX-R.png", 0)},
  {["labelSingleLine"] =  gfx.CreateLabel("MUSIC MODS",16, 0),        ["labelMultiLine"] =  gfx.CreateLabel("MUSIC\nMODS",16, 0),        ["image"] = gfx.CreateSkinImage("legend/FX-LR.png", 0)},
  {["labelSingleLine"] =  gfx.CreateLabel("PLAY",16, 0),              ["labelMultiLine"] =  gfx.CreateLabel("PLAY",16, 0),               ["image"] = gfx.CreateSkinImage("legend/start.png", 0)}
}
local grades = {
  {["max"] = 6999999, ["image"] = gfx.CreateSkinImage("score/D.png", 0)},
  {["max"] = 7999999, ["image"] = gfx.CreateSkinImage("score/C.png", 0)},
  {["max"] = 8699999, ["image"] = gfx.CreateSkinImage("score/B.png", 0)},
  {["max"] = 8999999, ["image"] = gfx.CreateSkinImage("score/A.png", 0)},
  {["max"] = 9299999, ["image"] = gfx.CreateSkinImage("score/A+.png", 0)},
  {["max"] = 9499999, ["image"] = gfx.CreateSkinImage("score/AA.png", 0)},
  {["max"] = 9699999, ["image"] = gfx.CreateSkinImage("score/AA+.png", 0)},
  {["max"] = 9799999, ["image"] = gfx.CreateSkinImage("score/AAA.png", 0)},
  {["max"] = 9899999, ["image"] = gfx.CreateSkinImage("score/AAA+.png", 0)},
  {["max"] = 99999999, ["image"] = gfx.CreateSkinImage("score/S.png", 0)}
}

local badges = {
    gfx.CreateSkinImage("badges/played.png", 0),
    gfx.CreateSkinImage("badges/clear.png", 0),
    gfx.CreateSkinImage("badges/hard-clear.png", 0),
    gfx.CreateSkinImage("badges/full-combo.png", 0),
    gfx.CreateSkinImage("badges/perfect.png", 0)
}

local recordCache = {}

gfx.LoadSkinFont("NotoSans-Regular.ttf");

game.LoadSkinSample("menu_click")
game.LoadSkinSample("click-02")
game.LoadSkinSample("woosh")

local wheelSize = 12

get_page_size = function()
    return math.floor(wheelSize/2)
end

-- Responsive UI variables
-- Aspect Ratios
local aspectFloat = 1.850
local aspectRatio = "widescreen"
local landscapeWidescreenRatio = 1.850
local landscapeStandardRatio = 1.500
local portraitWidescreenRatio = 0.5

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
  elseif x <= y and portraitWidescreenRatio <= a and a < landscapeStandardRatio then
    aspectRatio = "PortraitWidescreen"
    aspectFloat = 0.5
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


check_or_create_cache = function(song, loadJacket)
    if not songCache[song.id] then songCache[song.id] = {} end

    if not songCache[song.id]["title"] then
        songCache[song.id]["title"] = gfx.CreateLabel(song.title, 40, 0)
    end

    if not songCache[song.id]["artist"] then
        songCache[song.id]["artist"] = gfx.CreateLabel(song.artist, 25, 0)
    end

    if not songCache[song.id]["bpm"] then
        songCache[song.id]["bpm"] = gfx.CreateLabel(string.format("BPM: %s",song.bpm), 20, 0)
    end
	
	if not songCache[song.id]["effector"] then
        songCache[song.id]["effector"] = gfx.CreateLabel(string.format("BPM: %s",song.bpm), 20, 0)
    end

    if not songCache[song.id]["jacket"] and loadJacket then
        songCache[song.id]["jacket"] = gfx.CreateImage(song.difficulties[1].jacketPath, 0)
    end
end

function get_record(hash)
	if recordCache[hash] then 
		return recordCache[hash]
	end
	
	-- TODO: Set a field for when this cache expires (every 5 minutes)
	recordCache[hash] = NetServ.GetScoresForTrack(hash, 1, 0)
end

draw_scores = function(difficulty, x, y, w, h)
  -- draw the top score for this difficulty
	local xOffset = 5
  local height = h/3 - 10
  local ySpacing = h/3
	local yOffset = h/3
	
  gfx.FontSize(30);
  gfx.TextAlign(gfx.TEXT_ALIGN_BOTTOM + gfx.TEXT_ALIGN_CENTER);
  gfx.FastText("LOCAL SCORE", x +(w/4), y+(h/2))
  gfx.FastText("ONLINE RECORD", x + (3/4 * w), y + (h/2))
  
  -- local high score rect
  gfx.BeginPath()
  gfx.Rect(x+xOffset,y+h/2,w/2-(xOffset*2),h/2)
  gfx.FillColor(30,30,30,10)
  gfx.StrokeColor(0,128,255)
  gfx.StrokeWidth(1)
  gfx.Fill()
  gfx.Stroke()
  
  -- online record rect
  gfx.BeginPath()
  gfx.Rect(x + xOffset + w/2,y+h/2,w/2-(xOffset*2),h/2)
  gfx.FillColor(30,30,30,10)
  gfx.StrokeColor(0,128,255)
  gfx.StrokeWidth(1)
  gfx.Fill()
  gfx.Stroke()
  
  if difficulty.scores[1] ~= nil then
	local highScore = difficulty.scores[1]
	scoreLabel = gfx.CreateLabel(string.format("%08d",highScore.score), 40, 0)
	for i,v in ipairs(grades) do
        if v.max > highScore.score then
            gfx.BeginPath()
            iw,ih = gfx.ImageSize(v.image)
            iarr = ih / iw
            oldheight = h/2 - 10
            newheight =  iarr * (h/2-10)
            centreoffset = (oldheight - newheight)/2 + 3 -- +3 is stupid but ehhh
            gfx.ImageRect(x+xOffset, y+h/2 + centreoffset, oldheight,  newheight, v.image, 1, 0) --this is nasty but it works for me
            break
        end
    end
    if difficulty.topBadge ~= 0 then
        gfx.BeginPath()
        gfx.ImageRect(x+xOffset+w/2-h/2, y+h/2 +5, (h/2-10), h/2-10, badges[difficulty.topBadge], 1, 0)
    end

    gfx.FillColor(255,255,255)
	gfx.FontSize(40);
    gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_CENTER);
    gfx.DrawLabel(scoreLabel, x+(w/4),y+(h/4)*3,w/2)
	end
	
    onlinerecord = get_record(difficulty.sha3_512)
	
	if onlinerecord == nil then --record not set, but can be tracked
        recordLabel = gfx.CreateLabel(string.format("%08d", 0), 40, 0)
        gfx.FillColor(170, 170, 170)
        gfx.FontSize(40)
        gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_CENTER);
        gfx.DrawLabel(recordLabel, x+(w * 3/4),y+(h/4)*3,w/2)
    else
		for i,s in ipairs(onlinerecord) do
			recordScoreLabel = gfx.CreateLabel(string.format("%08d", s.score), 26, 0)
			recordPlayerLabel = gfx.CreateLabel(s.player, 26, 0)
			
			gfx.BeginPath()
			gfx.ImageRect(x+xOffset+w-h/2, y+h/2 +5, (h/2-10), h/2-10, badges[s.status], 1, 0)

			for ii,v in ipairs(grades) do
				if v.max > s.score then
					gfx.BeginPath()
					iw,ih = gfx.ImageSize(v.image)
					iarr = ih / iw
					oldheight = h/2 - 10
					newheight =  iarr * (h/2-10)
					centreoffset = (oldheight - newheight)/2 + 3 -- +3 is stupid but ehhh
					gfx.ImageRect(x+xOffset+w/2, y+h/2 + centreoffset, oldheight,  newheight, v.image, 1, 0) --this is nasty but it works for me
					break
				end
			end

			gfx.FillColor(255, 255, 255)
			gfx.FontSize(40)
			gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_CENTER);
			gfx.DrawLabel(recordPlayerLabel, x+(w * 3/4),y+(h/4)*2.55,w/2)
			gfx.DrawLabel(recordScoreLabel, x+(w * 3/4),y+(h/4)*3.45,w/2)
		end
	end
	
end

draw_song = function(song, x, y, w, h, selected)
    check_or_create_cache(song)
    gfx.BeginPath()
    gfx.RoundedRectVarying(x,y, w, h,0,0,0,40)
    gfx.FillColor(30,30,30)
    gfx.StrokeColor(0,128,255)
    gfx.StrokeWidth(1)
    if selected then
        gfx.StrokeColor(255,128,0)
        gfx.StrokeWidth(2)
    end
    gfx.Fill()
    gfx.Stroke()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
    gfx.DrawLabel(songCache[song.id]["title"], x+10, y + 5, w-10)
    gfx.DrawLabel(songCache[song.id]["artist"], x+20, y + 50, w-10)
    gfx.ForceRender()

end

draw_diff_icon = function(diff, x, y, w, h, selected)
    local shrinkX = w/4
    local shrinkY = h/4
    if selected then
      gfx.FontSize(h/2)
      shrinkX = w/6
      shrinkY = h/6
    else
      gfx.FontSize(math.floor(h / 3))
    end
    gfx.BeginPath()
    gfx.RoundedRectVarying(x+shrinkX,y+shrinkY,w-shrinkX*2,h-shrinkY*2,0,0,0,0)
    gfx.FillColor(15,15,15)
    gfx.StrokeColor(table.unpack(diffColors[diff.difficulty + 1]))
    gfx.StrokeWidth(2)
    gfx.Fill()
    gfx.Stroke()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_CENTER)
    gfx.FastText(tostring(diff.level), x+(w/2),y+(h/2))
end

draw_cursor = function(x,y,rotation,width)
	gfx.Save()
    gfx.BeginPath();
    gfx.Translate(x,y)
    gfx.Rotate(rotation)
    gfx.StrokeColor(255,128,0)
    gfx.StrokeWidth(4)
    gfx.Rect(-width/2, -width/2, width, width)
    gfx.Stroke()
    gfx.Restore()
end

draw_diffs = function(diffs, x, y, w, h)
    local diffWidth = w/2.5
    local diffHeight = w/2.5
    local diffCount = #diffs
    gfx.Scissor(x,y,w,h)
    for i = math.max(selectedDiff - 2, 1), math.max(selectedDiff - 1,1) do
      local diff = diffs[i]
      local xpos = x + ((w/2 - diffWidth/2) + (selectedDiff - i + doffset)*(-0.8*diffWidth))
      if  i ~= selectedDiff then
        draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, false)
      end
    end

    --after selected
  for i = math.min(selectedDiff + 2, diffCount), selectedDiff + 1,-1 do
      local diff = diffs[i]
      local xpos = x + ((w/2 - diffWidth/2) + (selectedDiff - i + doffset)*(-0.8*diffWidth))
      if  i ~= selectedDiff then
        draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, false)
      end
    end
    local diff = diffs[selectedDiff]
    local xpos = x + ((w/2 - diffWidth/2) + (doffset)*(-0.8*diffWidth))
  draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, true)
  gfx.BeginPath()
  gfx.FillColor(0,128,255)
  gfx.Rect(x,y+10,2,diffHeight-h/6)
  gfx.Fill()
  gfx.BeginPath()
  gfx.Rect(x+w-2,y+10,2,diffHeight-h/6)
  gfx.Fill()
  gfx.ResetScissor()
  draw_cursor(x + w/2, y +diffHeight/2, timer * math.pi, diffHeight / 1.5)
end

draw_selected = function(song, x, y, w, h)
    check_or_create_cache(song)
    -- set up padding and margins
    local xPadding = math.floor(w/16)
    local yPadding =  math.floor(h/32)
    local xMargin = math.floor(w/16)
    local yMargin =  math.floor(h/32)
    local width = (w-(xMargin*2))
    local height = (h-(yMargin*2))
    local xpos = x+xMargin
    local ypos = y+yMargin
    if aspectRatio == "PortraitWidescreen" then
      xPadding = math.floor(w/64)
      yPadding =  math.floor(h/32)
      xMargin = math.floor(w/64)
      yMargin =  math.floor(h/32)
      width = (w-(xMargin*2))
      height = (h-(yMargin*2))
      xpos = x+xMargin
      ypos = y+yMargin
    end
    --Border
    local diff = song.difficulties[selectedDiff]
    gfx.BeginPath()
    gfx.RoundedRectVarying(xpos,ypos,width,height,yPadding,yPadding,yPadding,yPadding)
    gfx.FillColor(30,30,30)
    gfx.StrokeColor(0,128,255)
    gfx.StrokeWidth(1)
    gfx.Fill()
    gfx.Stroke()
    -- jacket should take up 1/3 of height, always be square, and be centered
    local imageSize = math.floor(height/3)
    local imageXPos = ((width/2) - (imageSize/2)) + x+xMargin
    if aspectRatio == "PortraitWidescreen" then
      --Unless its portrait widesreen..
      imageSize = math.floor((height/3)*2)
      imageXPos = x+xMargin+xPadding
    end
    if not songCache[song.id][selectedDiff] or songCache[song.id][selectedDiff] ==  jacketFallback then
        songCache[song.id][selectedDiff] = gfx.LoadImageJob(diff.jacketPath, jacketFallback, 200,200)
    end

    if songCache[song.id][selectedDiff] then
        gfx.BeginPath()
        gfx.ImageRect(imageXPos, y+yMargin+yPadding, imageSize, imageSize, songCache[song.id][selectedDiff], 1, 0)
    end
    -- difficulty should take up 1/6 of height, full width, and be centered
    if aspectRatio == "PortraitWidescreen" then
      --difficulty wheel should be right below the jacketImage, and the same width as
      --the jacketImage
      draw_diffs(song.difficulties,xpos+xPadding,(ypos+yPadding+imageSize),imageSize,math.floor((height/3)*1)-yPadding)
    else
      -- difficulty should take up 1/6 of height, full width, and be centered
      draw_diffs(song.difficulties,(w/2)-(imageSize/2),(ypos+yPadding+imageSize),imageSize,math.floor(height/6))
    end
    -- effector / bpm should take up 1/3 of height, full width
    if aspectRatio == "PortraitWidescreen" then
      gfx.FontSize(40)
      gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
      gfx.DrawLabel(songCache[song.id]["title"], xpos+xPadding+imageSize, y+yMargin+yPadding, width-imageSize-20)
      gfx.FontSize(30)
      gfx.DrawLabel(songCache[song.id]["artist"], xpos+xPadding+imageSize+3, y+yMargin+yPadding + 45, width-imageSize-20)
      gfx.FontSize(20)
      gfx.DrawLabel(songCache[song.id]["bpm"], xpos+xPadding+imageSize+3, y+yMargin+yPadding + 85, width-imageSize-20)
      gfx.FastText(string.format("Effector: %s", diff.effector), xpos+xPadding+imageSize+3, y+yMargin+yPadding + 115)
    else
      gfx.FontSize(40)
      gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
      gfx.DrawLabel(songCache[song.id]["title"], xpos+10, (height/10)*6, width-20)
      gfx.FontSize(30)
      gfx.DrawLabel(songCache[song.id]["artist"], xpos+10, (height/10)*6 + 45, width-20)
      gfx.FillColor(255,255,255)
      gfx.FontSize(20)
      gfx.DrawLabel(songCache[song.id]["bpm"], xpos+10, (height/10)*6 + 85)
      gfx.FastText(string.format("Effector: %s", diff.effector),xpos+10, (height/10)*6 + 115)
    end
    if aspectRatio == "PortraitWidescreen" then
      draw_scores(diff, xpos+xPadding+imageSize+3,  (height/3)*2, width-imageSize-20, (height/3)-yPadding)
    else
      draw_scores(diff, xpos, (height/6)*5, width, (height/6))
    end
    gfx.ForceRender()
end

draw_songwheel = function(x,y,w,h)
  local offsetX = fifthX/2
  local width = math.floor((w/5)*4)
  if aspectRatio == "landscapeWidescreen" then
    wheelSize = 12
    offsetX = 80
  elseif aspectRatio == "landscapeStandard" then
    wheelSize = 10
    offsetX = 40
  elseif aspectRatio == "PortraitWidescreen" then
    wheelSize = 20
    offsetX = 20
    width = w
  end
  local height = math.floor((h/wheelSize)*1.5)

  for i = math.max(selectedIndex - wheelSize/2, 1), math.max(selectedIndex - 1,0) do
      local song = songwheel.songs[i]
      local xpos = x + offsetX + ((selectedIndex - i + ioffset) ^ 2) * 3
      local offsetY = (selectedIndex - i + ioffset) * ( height - (wheelSize/2*((selectedIndex-i + ioffset)*aspectFloat)))
      local ypos = y+((h/2 - height/2) - offsetY)
      draw_song(song, xpos, ypos, width, height)
  end

  --after selected
  for i = math.min(selectedIndex + wheelSize/2, #songwheel.songs), selectedIndex + 1,-1 do
      local song = songwheel.songs[i]
      local xpos = x + offsetX + ((i - selectedIndex - ioffset) ^ 2) * 2
      local offsetY = (selectedIndex - i + ioffset) * ( height - (wheelSize/2*((i-selectedIndex - ioffset)*aspectFloat)))
      local ypos = y+((h/2 - height/2) - (selectedIndex - i) - offsetY)
      local alpha = 255 - (selectedIndex - i + ioffset) * 31
      draw_song(song, xpos, ypos, width, height)
  end
  -- draw selected
  local xpos = x + offsetX/1.2 + ((-ioffset) ^ 2) * 2
  local offsetY = (ioffset) * ( height - (wheelSize/2*((1)*aspectFloat)))
  local ypos = y+((h/2 - height/2) - (ioffset) - offsetY)
  draw_song(songwheel.songs[selectedIndex], xpos, ypos, width, height, true)
  return songwheel.songs[selectedIndex]
end
draw_legend_pane = function(x,y,w,h,obj)
  local xpos = x+5
  local ypos = y
  local imageSize = h
  gfx.BeginPath()
  gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_LEFT)
  gfx.ImageRect(x, y, imageSize, imageSize, obj.image, 1, 0)
  xpos = xpos + imageSize + 5
  gfx.FontSize(16);
  if h < (w-(10+imageSize))/2 then
    gfx.DrawLabel(obj.labelSingleLine, xpos, y+(h/2), w-(10+imageSize))
  else
    gfx.DrawLabel(obj.labelMultiLine, xpos, y+(h/2), w-(10+imageSize))
  end
  gfx.ForceRender()
end

draw_legend = function(x,y,w,h)
  gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_LEFT);
  gfx.BeginPath()
  gfx.FillColor(0,0,0,170)
  gfx.Rect(x,y,w,h)
  gfx.Fill()
  local xpos = 10;
  local legendWidth = math.floor((w-20)/#legendTable)
  for i,v in ipairs(legendTable) do
    local xOffset = draw_legend_pane(xpos+(legendWidth*(i-1)), y+5,legendWidth,h-10,legendTable[i])
  end
end

draw_search = function(x,y,w,h)
  soffset = soffset + (searchIndex) - (songwheel.searchInputActive and 0 or 1)
  if searchIndex ~= (songwheel.searchInputActive and 0 or 1) then
      game.PlaySample("woosh")
  end
  searchIndex = songwheel.searchInputActive and 0 or 1

  gfx.BeginPath()
  local bgfade = 1 - (searchIndex + soffset)
  --if not songwheel.searchInputActive then bgfade = soffset end
  gfx.FillColor(0,0,0,math.floor(200 * bgfade))
  gfx.Rect(0,0,resx,resy)
  gfx.Fill()
  gfx.ForceRender()
  local xpos = x + (searchIndex + soffset)*w
  gfx.UpdateLabel(searchText ,string.format("Search: %s",songwheel.searchText), 30, 0)
  gfx.BeginPath()
  gfx.RoundedRect(xpos,y,w,h,h/2)
  gfx.FillColor(30,30,30)
  gfx.StrokeColor(0,128,255)
  gfx.StrokeWidth(1)
  gfx.Fill()
  gfx.Stroke()
  gfx.BeginPath();
  gfx.LoadSkinFont("NotoSans-Regular.ttf");
  gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_MIDDLE);
  gfx.DrawLabel(searchText, xpos+10,y+(h/2), w-20)

end

render = function(deltaTime)
    timer = (timer + deltaTime)
    timer = timer % 2
    resx,resy = game.GetResolution();
    adjustScreen(resx,resy);
    gfx.BeginPath();
    gfx.LoadSkinFont("NotoSans-Regular.ttf");
    gfx.FontSize(40);
    gfx.FillColor(255,255,255);
    if songwheel.songs[1] ~= nil then
      --draw songwheel and get selected song
      if aspectRatio == "PortraitWidescreen" then
        local song = draw_songwheel(0,0,fullX,fullY)
        --render selected song information
        draw_selected(song, 0,0,fullX,fifthY)
      else
        local song = draw_songwheel(fifthX*2,0,fifthX*3,fullY)
        --render selected song information
        draw_selected(song, 0,0,fifthX*2,(fifthY/2)*9)
      end
    end
    --Draw Legend Information
	if showGuide then
		if aspectRatio == "PortraitWidescreen" then
			draw_legend(0,(fifthY/3)*14, fullX, (fifthY/3)*1)
		else
			draw_legend(0,(fifthY/2)*9, fullX, (fifthY/2))
		end
	end

    --draw text search
    if aspectRatio == "PortraitWidescreen" then
      draw_search(fifthX*2,5,fifthX*3,fifthY/5)
    else
      draw_search(fifthX*2,5,fifthX*3,fifthY/3)
    end

    ioffset = ioffset * 0.9
    doffset = doffset * 0.9
    soffset = soffset * 0.8
	if songwheel.searchStatus then
		gfx.BeginPath()
		gfx.FillColor(255,255,255)
		gfx.FontSize(20);
		gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
		gfx.Text(songwheel.searchStatus, 3, 3)
	end
	if totalForce then
		gfx.BeginPath()
		gfx.FillColor(255,255,255)
		gfx.FontSize(20);
		gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_BOTTOM)
		local forceText = string.format("Force: %.2f", totalForce)
		gfx.Text(forceText, 0, fullY)
	end
    gfx.LoadSkinFont("NotoSans-Regular.ttf");
    gfx.ResetTransform()
    gfx.ForceRender()
end

set_index = function(newIndex)
    if newIndex ~= selectedIndex then
        game.PlaySample("menu_click")
    end
    ioffset = ioffset + selectedIndex - newIndex
    selectedIndex = newIndex
end;

set_diff = function(newDiff)
    if newDiff ~= selectedDiff then
        game.PlaySample("click-02")
    end
    doffset = doffset + selectedDiff - newDiff
    selectedDiff = newDiff
end;

-- force calculation
--------------------
totalForce = nil

local badgeRates = {
	0.5,  -- Played
	1.0,  -- Cleared
	1.02, -- Hard clear
	1.04, -- UC
	1.1   -- PUC
}

local gradeRates = {
	{["min"] = 9900000, ["rate"] = 1.05}, -- S
	{["min"] = 9800000, ["rate"] = 1.02}, -- AAA+
	{["min"] = 9700000, ["rate"] = 1},    -- AAA
	{["min"] = 9500000, ["rate"] = 0.97}, -- AA+
	{["min"] = 9300000, ["rate"] = 0.94}, -- AA
	{["min"] = 9000000, ["rate"] = 0.91}, -- A+
	{["min"] = 8700000, ["rate"] = 0.88}, -- A
	{["min"] = 7500000, ["rate"] = 0.85}, -- B
	{["min"] = 6500000, ["rate"] = 0.82}, -- C
	{["min"] =       0, ["rate"] = 0.8}   -- D
}

calculate_force = function(diff)
	if #diff.scores < 1 then
		return 0
	end
	local score = diff.scores[1]
	local badgeRate = badgeRates[diff.topBadge]
	local gradeRate
    for i, v in ipairs(gradeRates) do
      if score.score >= v.min then
        gradeRate = v.rate
		break
      end
    end
	return math.floor((diff.level * 2) * (score.score / 10000000) * gradeRate * badgeRate) / 100
end

songs_changed = function(withAll)
	if not withAll then return end

	local diffs = {}
	for i = 1, #songwheel.allSongs do
		local song = songwheel.allSongs[i]
		for j = 1, #song.difficulties do
			local diff = song.difficulties[j]
			diff.force = calculate_force(diff)
			table.insert(diffs, diff)
		end
	end
	table.sort(diffs, function (l, r)
		return l.force > r.force
	end)
	totalForce = 0
	for i = 1, 50 do
		if diffs[i] then
			totalForce = totalForce + diffs[i].force
		end
	end
end
