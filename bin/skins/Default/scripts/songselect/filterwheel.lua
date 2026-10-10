--Horizontal alignment
TEXT_ALIGN_LEFT 	= 1;
TEXT_ALIGN_CENTER 	= 2;
TEXT_ALIGN_RIGHT 	= 4;
--Vertical alignment
TEXT_ALIGN_TOP 		= 8;
TEXT_ALIGN_MIDDLE	= 16;
TEXT_ALIGN_BOTTOM	= 32;
TEXT_ALIGN_BASELINE	= 64;

local timer = 0;

local selectingFolders = true;
local selectedLevel = 1;
local selectedFolder = 1;
local levelLabels = {}
local folderLabels = {}

--timing settings
local levelOffset = 0;
local folderOffset = 0;

local drawFolderIndicator = function()
    local folder = filters and filters.folder and filters.folder[selectedFolder]
    if not folder or folder == "All" then
        return
    end

    local resx, resy = game.GetResolution()
    local fontSize = math.max(18, math.min(26, math.floor(resy / 45)))
    local padding = 10

    gfx.LoadSkinFont("NotoSans-Regular.ttf")
    gfx.FontSize(fontSize)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE)

    local x = resx - 20
    local y = math.max(40, resy * 0.075)
    local textX = x - padding
    local width = gfx.FastTextSize(folder)
    local maxWidth = resx - 60
    if width > maxWidth then
        fontSize = fontSize * maxWidth / width
        gfx.FontSize(fontSize)
        width = gfx.FastTextSize(folder)
    end

    local height = fontSize + padding * 2

    gfx.BeginPath()
    gfx.RoundedRect(x - width - padding * 2, y - height / 2, width + padding * 2, height, 6)
    gfx.FillColor(0, 0, 0, 190)
    gfx.Fill()
    gfx.StrokeColor(0, 128, 255, 220)
    gfx.StrokeWidth(1)
    gfx.Stroke()

    gfx.FillColor(255, 255, 255, 255)
    gfx.FastText(folder, textX, y)
end

render = function(deltaTime, shown)
    if not shown then
        drawFolderIndicator()
        return
    end
    timer = (timer + deltaTime)
    timer = timer % 2
    resx,resy = game.GetResolution();
    gfx.FillColor(0,0,0,200)
    gfx.FastRect(0,0,resx,resy)
    gfx.BeginPath();
    gfx.LoadSkinFont("NotoSans-Regular.ttf");
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    gfx.FastText(folderOffset,0,0)
    if selectingFolders then
        for i,f in ipairs(filters.folder) do
            if not folderLabels[i] then
               folderLabels[i] = gfx.CreateLabel(f, 40, 0)
            end
            if i == selectedFolder then
                gfx.FillColor(255,255,255,255)
            else
                gfx.FillColor(255,255,255,128)
            end
            local xpos = resx - 100 + ((i - selectedFolder - folderOffset) ^ 2) * 1
            local ypos = resy/2 + 50  * (i - selectedFolder - folderOffset)
            gfx.DrawLabel(folderLabels[i], xpos, ypos);
        end
    else
        for i,l in ipairs(filters.level) do
            if not levelLabels[i] then
               levelLabels[i] = gfx.CreateLabel(l, 40, 0)
            end
            if i == selectedLevel then
                gfx.FillColor(255,255,255,255)
            else
                gfx.FillColor(255,255,255,128)
            end
            local xpos = resx - 100 + ((i - selectedLevel - levelOffset) ^ 2) * 1
            local ypos = resy/2 + 50  * (i - selectedLevel - levelOffset)
            gfx.DrawLabel(levelLabels[i], xpos, ypos);
        end
    end
    levelOffset = levelOffset * 0.7
    folderOffset = folderOffset * 0.7
end

set_selection = function(newIndex, isFolder)
    if isFolder then
      folderOffset = folderOffset + selectedFolder - newIndex
      selectedFolder = newIndex
    else
      levelOffset = levelOffset + selectedLevel - newIndex
      selectedLevel = newIndex
    end
end

set_mode = function(isFolder)
    selectingFolders = isFolder
end

function tables_set()
    gfx.LoadSkinFont("NotoSans-Regular.ttf");
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    for i,f in ipairs(filters.folder) do
       folderLabels[i] = gfx.CreateLabel(f, 40, 0)
    end

    for i,l in ipairs(filters.level) do
        if not levelLabels[i] then
           levelLabels[i] = gfx.CreateLabel(l, 40, 0)
        end
    end
end
