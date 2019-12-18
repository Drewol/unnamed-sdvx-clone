background.LoadTexture("mainTex", "bg.png")
resx, resy = game.GetResolution()
portrait = resy > resx
desw = portrait and 720 or 1280
desh = desw * (resy / resx)
scale = resx / desw
tex = gfx.CreateImage(background.GetPath() .. "petal.png", 0)
particleCount = 40
particleSizeSpread = 0.5
particles = {}
psize = 30

gradients = {}
for i=1,64 do
	local v = i * 4 - 1
	local v2 = math.floor(5 * i / 64)
	gfx.GradientColors(255 - v2, 255 - v2, 255, 255,
	                   255 - v, 255 - v, 255, 0)
	local ir = (i/64) * (psize / 7)
	gradients[i] = gfx.RadialGradient(0, 0, ir, psize/2)
end

function initializeParticle(initial)
	local particle = {}
	particle.x = math.random()
	particle.y = math.random() * 1.2 - 0.1
	if not initial then particle.y = -0.1 end
	particle.r = math.random()
	particle.s = (math.random() - 0.5) * particleSizeSpread + 1.0
	particle.xv = 0
	particle.yv = 0.1
	particle.rv = math.random() * 2.0 - 1.0
	particle.p = math.random() * math.pi * 2
	return particle
end

for i=1,particleCount do
	particles[i] = initializeParticle(true)
end

function sign(x)
  return x>0 and 1 or x<0 and -1 or 0
end

function render_bg(deltaTime)
	local alpha = 0.3 + 0.5 * background.GetClearTransition()
	gfx.GlobalAlpha(alpha)
	local grad = gradients[math.floor(background.GetClearTransition() * 63) + 1]
	for i,p in ipairs(particles) do
		p.x = p.x + p.xv * deltaTime
		p.y = p.y + p.yv * deltaTime
		p.r = p.r + p.rv * deltaTime
		p.p = (p.p + deltaTime) % (math.pi * 2)

		p.xv = 0.5 - ((p.x * 2) % 1) + (0.5 * sign(p.x - 0.5))
		p.xv = math.max(math.abs(p.xv * 2) - 1, 0) * sign(p.xv)
		p.xv = p.xv * p.y
		p.xv = p.xv + math.sin(p.p) * 0.01

		gfx.Save()
		gfx.ResetTransform()
		gfx.Translate(p.x * resx, p.y * resy)
		gfx.Rotate(p.r)
		gfx.Scale(p.s * scale, p.s * scale)
		gfx.BeginPath()
		gfx.Rect(-psize/2, -psize/2, psize, psize)
		gfx.FillPaint(grad);
		gfx.Fill()
		gfx.Restore()
		if p.y > 1.1 then
			particles[i] = initializeParticle(false)
		end
	end
	gfx.ForceRender()
end
