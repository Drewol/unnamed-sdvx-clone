render = function(deltaTime, shown)
    resx,resy = game.GetResolution();
    gfx.BeginPath();
    gfx.LoadSkinFont("segoeui.ttf");
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    if shown then
        gfx.FillColor(0,0,0,200)
        gfx.Rect(0,0,resx,resy)
        gfx.Fill()
        gfx.BeginPath()
        for i,setting in ipairs(settings) do
            game.Log(string.format("%s, %s",i,CurrentSelection), game.LOGGER_INFO)
            if i == settings.currentSelection then
              gfx.FillColor(222,222,222,255)
              gfx.FontSize(50);
            else
              gfx.FillColor(222,222,222,170)
              gfx.FontSize(40);
            end
            gfx.Text(string.format("%s: %s", setting.name, setting.value), resx/2, resy/2 + 40 * (i - settings.currentSelection), 40);
        end
    end
end
