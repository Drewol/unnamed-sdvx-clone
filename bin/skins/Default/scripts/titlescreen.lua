local mposx = 0;
local mposy = 0;
local hovered = nil;
local cursorIndex = 1
local buttonWidth = 250;
local buttonHeight = 50;
local buttonBorder = 2;
local label = -1;

local gr_r, gr_g, gr_b, gr_a = game.GetSkinSetting("col_test")
gfx.GradientColors(0,127,255,255,0,128,255,0)
local gradient = gfx.LinearGradient(0,0,0,1)
local bgPattern = gfx.CreateSkinImage("bg_pattern.png", gfx.IMAGE_REPEATX + gfx.IMAGE_REPEATY)
local bgAngle = 0.5
local bgPaint = gfx.ImagePattern(0,0, 256,256, bgAngle, bgPattern, 1.0)
local bgPatternTimer = 0
local cursorYs = {}
local buttons = nil
local resx, resy = game.GetResolution();

view_update = function()
    if package.config:sub(1,1) == '\\' then --windows
        updateUrl, updateVersion = game.UpdateAvailable()
        os.execute("start " .. updateUrl)
    else --unix
        --TODO: Mac solution
        os.execute("xdg-open " .. updateUrl)
    end
end

mouse_clipped = function(x,y,w,h)
    return mposx > x and mposy > y and mposx < x+w and mposy < y+h;
end;

draw_button = function(button, x, y)
	local name = button[1]
    local rx = x - (buttonWidth / 2);
    local ty = y - (buttonHeight / 2);
    gfx.BeginPath();
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);

	gfx.FontSize(40);

	if mouse_clipped(rx,ty, buttonWidth, buttonHeight) then
       hovered = button[2];
	   b_r, b_g, b_b, b_a = game.GetSkinSetting("col_test")
       gfx.FillColor(b_r,b_g,b_b);
	   gfx.Text(name, x+1, y+1);
	   gfx.Text(name, x-1, y+1);
	   gfx.Text(name, x+1, y-1);
	   gfx.Text(name, x-1, y-1);
    end
	gfx.FillColor(255,255,255);
    gfx.Text(name, x, y);
	return buttonHeight + 5
end;

function updateGradient()
	gr_r, gr_g, gr_b, gr_a = game.GetSkinSetting("col_test")
	if gr_r == nil then return end
	gfx.GradientColors(gr_r,gr_g,gr_b,gr_a,0,128,255,0)
	--gradient = gfx.LinearGradient(0,0,0,1)
end

function updatePattern(dt)
	bgPatternTimer = (bgPatternTimer + dt) % 1.0
	local bgx = math.cos(bgAngle) * (bgPatternTimer * 256)
	local bgy = math.sin(bgAngle) * (bgPatternTimer * 256)
	gfx.UpdateImagePattern(bgPaint, bgx, bgy, 256, 256, bgAngle, 1.0)
end

function setButtons()
	if buttons == nil then
		buttons = {}
		buttons[1] = {"Start", Menu.Start}
		buttons[2] = {"Multiplayer", Menu.Multiplayer}
		buttons[3] = {"Get Songs", Menu.DLScreen}
		buttons[4] = {"Settings", Menu.Settings}
		buttons[5] = {"Exit", Menu.Exit}
	end
end

local renderY = resy/2
function draw_cursor(x,y,deltaTime)
	gfx.Save()
	gfx.BeginPath()

	local size = 8

	renderY = renderY - (renderY - y) * deltaTime * 30

	gfx.MoveTo(x-size,renderY-size)
	gfx.LineTo(x,renderY)
	gfx.LineTo(x-size,renderY+size)

	gfx.StrokeWidth(3)
	gfx.StrokeColor(255,255,255)
	gfx.Stroke()

	gfx.Restore()
end


function sign(x)
  return x>0 and 1 or x<0 and -1 or 0
end

function roundToZero(x)
	if x<0 then return math.ceil(x)
	elseif x>0 then return math.floor(x)
	else return 0 end
end

function deltaKnob(delta)
	if math.abs(delta) > 1.5 * math.pi then
		return delta + 2 * math.pi * sign(delta) * -1
	end
	return delta
end



local lastKnobs = nil
local knobProgress = 0
function handle_controller()
	if lastKnobs == nil then
		lastKnobs = {game.GetKnob(0), game.GetKnob(1)}
	else
		local newKnobs = {game.GetKnob(0), game.GetKnob(1)}

		knobProgress = knobProgress - deltaKnob(lastKnobs[1] - newKnobs[1]) * 1.2
		knobProgress = knobProgress - deltaKnob(lastKnobs[2] - newKnobs[2]) * 1.2

		lastKnobs = newKnobs

		if math.abs(knobProgress) > 1 then
			cursorIndex = (((cursorIndex - 1) + roundToZero(knobProgress)) % #buttons) + 1
			knobProgress = knobProgress - roundToZero(knobProgress)
		end
	end
end

render = function(deltaTime)
	setButtons()
	updateGradient()
	updatePattern(deltaTime)
    resx,resy = game.GetResolution();
    mposx,mposy = game.GetMousePos();
    gfx.Scale(resx, resy / 3)
    gfx.Rect(0,0,1,1)
    gfx.FillPaint(gradient)
    gfx.Fill()
    gfx.ResetTransform()
    gfx.BeginPath()
	gfx.Scale(0.5,0.5)
	gfx.Rect(0,0,resx * 2,resy * 2)
	gfx.GlobalCompositeOperation(gfx.BLEND_OP_DESTINATION_IN)
    gfx.FillPaint(bgPaint)
    gfx.Fill()
	gfx.ResetTransform()
	gfx.BeginPath()
	gfx.GlobalCompositeOperation(gfx.BLEND_OP_SOURCE_OVER)

	cursorGet = 1
    buttonY = resy / 2;
    hovered = nil;

    gfx.LoadSkinFont("NotoSans-Regular.ttf");

	for i=1,#buttons do
		cursorYs[i] = buttonY
		buttonY = buttonY + draw_button(buttons[i], resx / 2, buttonY);
		if hovered == buttons[i][2] then
			cursorIndex = i
		end
	end

	handle_controller()

	draw_cursor(resx/2 - 100, cursorYs[cursorIndex], deltaTime)

    gfx.BeginPath();
    gfx.FillColor(255,255,255);
    gfx.FontSize(120);
    if label == -1 then
        label = gfx.CreateLabel("unnamed_sdvx_clone", 120, 0);
    end
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    gfx.DrawLabel(label, resx / 2, resy / 2 - 200, resx-40);
    updateUrl, updateVersion = game.UpdateAvailable()
    if updateUrl then
       gfx.BeginPath()
       gfx.TextAlign(gfx.TEXT_ALIGN_BOTTOM + gfx.TEXT_ALIGN_LEFT)
       gfx.FontSize(30)
       gfx.Text(string.format("Version %s is now available", updateVersion), 5, resy - buttonHeight - 10)
       draw_button({"View", view_update}, buttonWidth / 2 + 5, resy - buttonHeight / 2 - 5);
       draw_button({"Update", Menu.Update}, buttonWidth * 1.5 + 15, resy - buttonHeight / 2 - 5)
    end
end;

mouse_pressed = function(button)
    if hovered then
        hovered()
    end
    return 0
end

function button_pressed(button)
    if button == game.BUTTON_STA then
        buttons[cursorIndex][2]()
    elseif button == game.BUTTON_BCK then
        Menu.Exit()
    end
end
