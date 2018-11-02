--Horizontal alignment
TEXT_ALIGN_LEFT 	= 1;
TEXT_ALIGN_CENTER 	= 2;
TEXT_ALIGN_RIGHT 	= 4;
--Vertical alignment
TEXT_ALIGN_TOP 		= 8;
TEXT_ALIGN_MIDDLE	= 16;
TEXT_ALIGN_BOTTOM	= 32;
TEXT_ALIGN_BASELINE	= 64;
local backgroundImage = gfx.CreateSkinImage("bg.png", 1);

render = function(deltaTime)
    resx,resy = game.GetResolution();
    gfx.BeginPath();
    gfx.TextAlign(TEXT_ALIGN_CENTER + TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    gfx.ImageRect(0, 0, resx, resy, backgroundImage, 1, 0);
end
