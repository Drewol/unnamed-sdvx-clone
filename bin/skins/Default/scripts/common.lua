
function string:trim()
   return self:gsub("^%s*(.-)%s*$", "%1")
end

ini = include "ini"
config = ini.Load("skin")

gfx.LoadSkinFont("segoeui.ttf");
