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

render = function(deltaTime, shown)
    if not shown then
        return
    end
    timer = (timer + deltaTime)
    timer = timer % 2
    resx,resy = game.GetResolution();
    gfx.FillColor(0,0,0,200)
    gfx.FastRect(0,0,resx,resy)
    gfx.BeginPath();
    gfx.LoadSkinFont("segoeui.ttf");
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
