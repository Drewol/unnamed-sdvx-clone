#include "stdafx.h"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "LaserTrackBuilder.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <Beatmap/BeatmapObjects.hpp>
#include "AsyncAssetLoader.hpp"

const float Track::trackWidth = 1.0f;
const float Track::buttonWidth = 1.0f / 6;
const float Track::laserWidth = buttonWidth;
const float Track::fxbuttonWidth = buttonWidth * 2;
const float Track::buttonTrackWidth = buttonWidth * 4;

Track::Track()
{
	m_viewRange = 2.0f;
	if (g_aspectRatio < 1.0f)
		trackLength = 12.0f;
	else
		trackLength = 10.0f;
}
Track::~Track()
{
	if(loader)
		delete loader;

	for(uint32 i = 0; i < 2; i++)
	{
		if(m_laserTrackBuilder[i])
			delete m_laserTrackBuilder[i];
	}
	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end(); it++)
	{
		delete *it;
	}
	if(timedHitEffect)
		delete timedHitEffect;
}
bool Track::AsyncLoad()
{
	loader = new AsyncAssetLoader();
	String skin = g_gameConfig.GetString(GameConfigKeys::Skin);

	float laserHues[2] = { 0.f };
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);

	for (uint32 i = 0; i < 2; i++)
		laserColors[i] = Color::FromHSV(laserHues[i],1.0,1.0);

	// Load hit effect colors
	Image hitColorPalette;
	CheckedLoad(hitColorPalette = ImageRes::Create("skins/" + skin + "/textures/hitcolors.png"));
	assert(hitColorPalette->GetSize().x >= 4);
	for(uint32 i = 0; i < 4; i++)
		hitColors[i] = hitColorPalette->GetBits()[i];

	// mip-mapped and anisotropicaly filtered track textures
	loader->AddTexture(trackTexture, "track.png");
	loader->AddTexture(trackTickTexture, "tick.png");

	// Scoring texture
	loader->AddTexture(scoreHitTexture, "scorehit.png");


	for(uint32 i = 0; i < 3; i++)
	{
		loader->AddTexture(scoreHitTextures[i], Utility::Sprintf("score%d.png", i));
	}

	// Load Button object
	loader->AddTexture(buttonTexture, "button.png");
	loader->AddTexture(buttonHoldTexture, "buttonhold.png");

	// Load FX object
	loader->AddTexture(fxbuttonTexture, "fxbutton.png");
	loader->AddTexture(fxbuttonHoldTexture, "fxbuttonhold.png");

	// Load Laser object
	loader->AddTexture(laserTextures[0], "laser_l.png");
	loader->AddTexture(laserTextures[1], "laser_r.png");

	// Entry and exit textures for laser
	loader->AddTexture(laserTailTextures[0], "laser_entry_l.png");
	loader->AddTexture(laserTailTextures[1], "laser_entry_r.png");
	loader->AddTexture(laserTailTextures[2], "laser_exit_l.png");
	loader->AddTexture(laserTailTextures[3], "laser_exit_r.png");

	// Track materials
	loader->AddMaterial(trackMaterial, "track");
	loader->AddMaterial(spriteMaterial, "sprite"); // General purpose material
	loader->AddMaterial(buttonMaterial, "button");
	loader->AddMaterial(holdButtonMaterial, "holdbutton");
	loader->AddMaterial(laserMaterial, "laser");
	loader->AddMaterial(blackLaserMaterial, "blackLaser");
	loader->AddMaterial(trackOverlay, "overlay");

	return loader->Load();
}
bool Track::AsyncFinalize()
{
	// Finalizer loading textures/material/etc.
	bool success = loader->Finalize();
	delete loader;
	loader = nullptr;

	// Set Texture states
	trackTexture->SetMipmaps(false);
	trackTexture->SetFilter(true, true, 16.0f);
	trackTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	trackTickTexture->SetMipmaps(true);
	trackTickTexture->SetFilter(true, true, 16.0f);
	trackTickTexture->SetWrap(TextureWrap::Repeat, TextureWrap::Clamp);
	trackTickLength = trackTickTexture->CalculateHeight(buttonTrackWidth);
	scoreHitTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);

	buttonTexture->SetMipmaps(true);
	buttonTexture->SetFilter(true, true, 16.0f);
	buttonHoldTexture->SetMipmaps(true);
	buttonHoldTexture->SetFilter(true, true, 16.0f);
	buttonLength = buttonTexture->CalculateHeight(buttonWidth);
	buttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(buttonWidth, buttonLength));
	buttonMaterial->opaque = false;

	fxbuttonTexture->SetMipmaps(true);
	fxbuttonTexture->SetFilter(true, true, 16.0f);
	fxbuttonHoldTexture->SetMipmaps(true);
	fxbuttonHoldTexture->SetFilter(true, true, 16.0f);
	fxbuttonLength = fxbuttonTexture->CalculateHeight(fxbuttonWidth);
	fxbuttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(fxbuttonWidth, fxbuttonLength));

	holdButtonMaterial->opaque = false;

	for (uint32 i = 0; i < 4; i++)
	{
		laserTextures[i]->SetMipmaps(true);
		laserTextures[i]->SetFilter(true, true, 16.0f);
		laserTextures[i]->SetWrap(TextureWrap::Clamp, TextureWrap::Repeat);
	}

	for(uint32 i = 0; i < 4; i++)
	{
		laserTailTextures[i]->SetMipmaps(true);
		laserTailTextures[i]->SetFilter(true, true, 16.0f);
		laserTailTextures[i]->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	}

	// Track and sprite material (all transparent)
	trackMaterial->opaque = false;
	spriteMaterial->opaque = false;

	// Laser object material, allows coloring and sampling laser edge texture
	laserMaterial->blendMode = MaterialBlendMode::Additive;
	laserMaterial->opaque = false;
	blackLaserMaterial->opaque = false;

	// Overlay shader
	trackOverlay->opaque = false;

	// Create a laser track builder for each laser object
	// these will output and cache meshes for rendering lasers
	for(uint32 i = 0; i < 2; i++)
	{
		m_laserTrackBuilder[i] = new LaserTrackBuilder(g_gl, this, i);
		m_laserTrackBuilder[i]->laserBorderPixels = 12;
		m_laserTrackBuilder[i]->laserLengthScale = trackLength / (GetViewRange() * laserSpeedOffset);
		m_laserTrackBuilder[i]->Reset(); // Also initializes the track builder
	}

	// Generate simple planes for the playfield track and elements
	trackMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -1), Vector2(trackWidth, trackLength + 1));
	trackTickMesh = MeshGenerators::Quad(g_gl, Vector2(-buttonTrackWidth * 0.5f, 0.0f), Vector2(buttonTrackWidth, trackTickLength));
	centeredTrackMesh = MeshGenerators::Quad(g_gl, Vector2(-0.5f, -0.5f), Vector2(1.0f, 1.0f));

	timedHitEffect = new TimedHitEffect(false);
	timedHitEffect->time = 0;
	timedHitEffect->track = this;

	return success;
}
void Track::Tick(class BeatmapPlayback& playback, float deltaTime)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	if(&currentTimingPoint != m_lastTimingPoint)
	{
		m_lastTimingPoint = &currentTimingPoint;
	}

	// Calculate track origin transform
	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;

	// Button Hit FX
	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end();)
	{
		(*it)->Tick(deltaTime);
		if((*it)->time <= 0.0f)
		{
			delete *it;
			it = m_hitEffects.erase(it);
			continue;
		}
		it++;
	}
	timedHitEffect->Tick(deltaTime);

	MapTime currentTime = playback.GetLastTime();

	// Set the view range of the track
	trackViewRange = Vector2((float)currentTime, 0.0f);
	trackViewRange.y = trackViewRange.x + GetViewRange();

	// Update ticks separating bars to draw
	double tickTime = (double)currentTime;
	MapTime rangeEnd = currentTime + playback.ViewDistanceToDuration(m_viewRange);
	const TimingPoint* tp = playback.GetTimingPointAt((MapTime)tickTime);
	double stepTime = tp->GetBarDuration(); // Every xth note based on signature

	// Overflow on first tick
	double firstOverflow = fmod((double)tickTime - tp->time, stepTime);
	if(fabs(firstOverflow) > 1)
		tickTime -= firstOverflow;

	m_barTicks.clear();

	// Add first tick
	m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));

	while(tickTime < rangeEnd)
	{
		double next = tickTime + stepTime;

		const TimingPoint* tpNext = playback.GetTimingPointAt((MapTime)tickTime);
		if(tpNext != tp)
		{
			tp = tpNext;
			tickTime = tp->time;
			stepTime = tp->GetBarDuration(); // Every xth note based on signature
		}
		else
		{
			tickTime = next;
		}

		// Add tick
		m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));
	}

	// Update track hide status
	m_trackHide += m_trackHideSpeed * deltaTime;
	m_trackHide = Math::Clamp(m_trackHide, 0.0f, 1.0f);

	// Set Object glow
	int32 startBeat = 0;
	uint32 numBeats = playback.CountBeats(m_lastMapTime, currentTime - m_lastMapTime, startBeat, 4);
	objectGlowState = currentTime % 100 < 50 ? 0 : 1;
	m_lastMapTime = currentTime;

	objectGlow = fabs((currentTime % 100) / 50.0 - 1) * 0.5 + 0.5;

	/*
	if(numBeats > 0)
	{
		objectGlow = 1.0f;
	}
	else
	{
		objectGlow -= 7.0f * deltaTime;
		if(objectGlow < 0.0f)
			objectGlow = 0.0f;
	}
	*/

	// Perform laser track cache cleanup, etc.
	for(uint32 i = 0; i < 2; i++)
	{
		m_laserTrackBuilder[i]->Update(m_lastMapTime);

		//laserAlertOpacity[i] = (-pow(m_alertTimer[i], 2.0f) + (1.5f * m_alertTimer[i])) * 5.0f;
		//laserAlertOpacity[i] = Math::Clamp<float>(laserAlertOpacity[i], 0.0f, 1.0f);
		//m_alertTimer[i] += deltaTime;
	}


}

void Track::DrawLaserBase(RenderQueue& rq, class BeatmapPlayback& playback, const Vector<ObjectState*>& objects)
{
	for (auto obj : objects)
	{
		if (obj->type != ObjectType::Laser)
			continue;

		LaserObjectState* laser = (LaserObjectState*)obj;
		if ((laser->flags & LaserObjectState::flag_Extended) != 0 || m_trackHide > 0.f)
		{
			// Calculate height based on time on current track
			float viewRange = GetViewRange();
			float position = playback.TimeToViewDistance(obj->time);
			float posmult = trackLength / (m_viewRange * laserSpeedOffset);

			Mesh laserMesh = m_laserTrackBuilder[laser->index]->GenerateTrackMesh(playback, laser);

			MaterialParameterSet laserParams;
			laserParams.SetParameter("mainTex", laserTextures[laser->index]);

			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			laserTransform *= Transform::Translation(Vector3{ 0.0f, posmult * position, 0.0f });

			if (laserMesh)
			{
				rq.Draw(laserTransform, laserMesh, blackLaserMaterial, laserParams);
			}
		}
	}
}

void Track::DrawBase(class RenderQueue& rq)
{
	// Base
	MaterialParameterSet params;
	Transform transform = trackOrigin;
	params.SetParameter("mainTex", trackTexture);
	params.SetParameter("lCol", laserColors[0]);
	params.SetParameter("rCol", laserColors[1]);
	params.SetParameter("hidden", m_trackHide);
	rq.Draw(transform, trackMesh, trackMaterial, params);

	// Draw the main beat ticks on the track
	params.SetParameter("mainTex", trackTickTexture);
	params.SetParameter("hasSample", false);
	for(float f : m_barTicks)
	{
		float fLocal = f / m_viewRange;
		Vector3 tickPosition = Vector3(0.0f, trackLength * fLocal - trackTickLength * 0.5f, 0.01f);
		Transform tickTransform = trackOrigin;
		tickTransform *= Transform::Translation(tickPosition);
		rq.Draw(tickTransform, trackTickMesh, buttonMaterial, params);
	}
}
void Track::DrawObjectState(RenderQueue& rq, class BeatmapPlayback& playback, ObjectState* obj, bool active)
{
	// Calculate height based on time on current track
	float viewRange = GetViewRange();
	float position = playback.TimeToViewDistance(obj->time) / viewRange;
	float glow = 0.0f;

	if(obj->type == ObjectType::Single || obj->type == ObjectType::Hold)
	{
		bool isHold = obj->type == ObjectType::Hold;
		MultiObjectState* mobj = (MultiObjectState*)obj;
		MaterialParameterSet params;
		Material mat = buttonMaterial;
		Mesh mesh;
		float width;
		float xposition;
		float length;
		float currentObjectGlow = active ? objectGlow : 0.3f;
		int currentObjectGlowState = active ? 2 + objectGlowState : 0;
		if(mobj->button.index < 4) // Normal button
		{
			width = buttonWidth;
			xposition = buttonTrackWidth * -0.5f + width * mobj->button.index;
			length = buttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex", isHold ? buttonHoldTexture : buttonTexture);
			mesh = buttonMesh;
		}
		else // FX Button
		{
			width = fxbuttonWidth;
			xposition = buttonTrackWidth * -0.5f + fxbuttonWidth *(mobj->button.index - 4);
			length = fxbuttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex", isHold ? fxbuttonHoldTexture : fxbuttonTexture);
			mesh = fxbuttonMesh;
		}

		if(isHold)
		{
			if(!active && mobj->hold.GetRoot()->time > playback.GetLastTime())
				params.SetParameter("hitState", 1);
			else
				params.SetParameter("hitState", currentObjectGlowState);

			params.SetParameter("objectGlow", currentObjectGlow);
			mat = holdButtonMaterial;
		}

		Vector3 buttonPos = Vector3(xposition, trackLength * position, 0.0f);

		Transform buttonTransform = trackOrigin;
		buttonTransform *= Transform::Translation(buttonPos);
		float scale = 1.0f;
		if(isHold) // Hold Note?
		{
			scale = (playback.DurationToViewDistanceAtTime(mobj->time, mobj->hold.duration) / viewRange) / length  * trackLength;
		}
		buttonTransform *= Transform::Scale({ 1.0f, scale, 1.0f });
		rq.Draw(buttonTransform, mesh, mat, params);
	}
	else if(obj->type == ObjectType::Laser) // Draw laser
	{
		
		position = playback.TimeToViewDistance(obj->time);
		float posmult = trackLength / (m_viewRange * laserSpeedOffset);
		LaserObjectState* laser = (LaserObjectState*)obj;

		// Draw segment function
		auto DrawSegment = [&](Mesh mesh, Texture texture, int part)
		{
			MaterialParameterSet laserParams;

			// Make not yet hittable lasers slightly glowing
			if (laser->GetRoot()->time > playback.GetLastTime())
			{
				laserParams.SetParameter("objectGlow", 0.6f);
				laserParams.SetParameter("hitState", 1);
			}
			else
			{
				laserParams.SetParameter("objectGlow", active ? objectGlow : 0.4f);
				laserParams.SetParameter("hitState", active ? 2 + objectGlowState : 0);
			}
			laserParams.SetParameter("mainTex", texture);
			laserParams.SetParameter("laserPart", part);

			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			laserTransform *= Transform::Translation(Vector3{ 0.0f, posmult * position,
				0.0f });

			// Set laser color
			laserParams.SetParameter("color", laserColors[laser->index]);

			if(mesh)
			{
				rq.Draw(laserTransform, mesh, laserMaterial, laserParams);
			}
		};

		// Draw entry?
		if(!laser->prev)
		{
			Mesh laserTail = m_laserTrackBuilder[laser->index]->GenerateTrackEntry(playback, laser);
			DrawSegment(laserTail, laserTailTextures[laser->index], 1);
		}

		// Body
		Mesh laserMesh = m_laserTrackBuilder[laser->index]->GenerateTrackMesh(playback, laser);
		DrawSegment(laserMesh, laserTextures[laser->index], 0);

		// Draw exit?
		if(!laser->next && (laser->flags & LaserObjectState::flag_Instant) != 0) // Only draw exit on slams
		{
			Mesh laserTail = m_laserTrackBuilder[laser->index]->GenerateTrackExit(playback, laser);
			DrawSegment(laserTail, laserTailTextures[2 + laser->index], 2);
		}
	}
}
void Track::DrawOverlays(class RenderQueue& rq)
{
	// Draw button hit effect sprites
	for(auto& hfx : m_hitEffects)
	{
		hfx->Draw(rq);
	}
	if(timedHitEffect->time > 0.0f)
		timedHitEffect->Draw(rq);
}
void Track::DrawTrackOverlay(RenderQueue& rq, Texture texture, float heightOffset /*= 0.05f*/, float widthScale /*= 1.0f*/)
{
	MaterialParameterSet params;
	params.SetParameter("mainTex", texture);
	Transform transform = trackOrigin;
	transform *= Transform::Scale({ widthScale, 1.0f, 1.0f });
	transform *= Transform::Translation({ 0.0f, heightOffset, 0.0f });
	rq.Draw(transform, trackMesh, trackOverlay, params);
}
void Track::DrawSprite(RenderQueue& rq, Vector3 pos, Vector2 size, Texture tex, Color color /*= Color::White*/, float tilt /*= 0.0f*/)
{
	Transform spriteTransform = trackOrigin;
	spriteTransform *= Transform::Translation(pos);
	spriteTransform *= Transform::Scale({ size.x, size.y, 1.0f });
	if(tilt != 0.0f)
		spriteTransform *= Transform::Rotation({ tilt, 0.0f, 0.0f });

	MaterialParameterSet params;
	params.SetParameter("mainTex", tex);
	params.SetParameter("color", color);
	rq.Draw(spriteTransform, centeredTrackMesh, spriteMaterial, params);
}
void Track::DrawCombo(RenderQueue& rq, uint32 score, Color color, float scale)
{
	if(score == 0)
		return;
	Vector<Mesh> meshes;
	while(score > 0)
	{
		uint32 c = score % 10;
		meshes.Add(comboSpriteMeshes[c]);
		score -= c;
		score /= 10;
	}
	const float charWidth = trackWidth * 0.15f * scale;
	const float seperation = charWidth * 0.7f;
	float size = (float)(meshes.size()-1) * seperation;
	float halfSize = size * 0.5f;

	///TODO: cleanup
	MaterialParameterSet params;
	params.SetParameter("mainTex", 0);
	params.SetParameter("color", color);
	for(uint32 i = 0; i < meshes.size(); i++)
	{
		float xpos = -halfSize + seperation * (meshes.size()-1-i);
		Transform t = trackOrigin;
		t *= Transform::Translation({ xpos, 0.3f, -0.004f});
		t *= Transform::Scale({charWidth, charWidth, 1.0f});
		rq.Draw(t, meshes[i], spriteMaterial, params);
	}
}

Vector3 Track::TransformPoint(const Vector3 & p)
{
	return trackOrigin.TransformPoint(p);
}

TimedEffect* Track::AddEffect(TimedEffect* effect)
{
	m_hitEffects.Add(effect);
	effect->track = this;
	return effect;
}
void Track::ClearEffects()
{
	m_trackHide = 0.0f;
	m_trackHideSpeed = 0.0f;

	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end(); it++)
	{
		delete *it;
	}
	m_hitEffects.clear();
}

void Track::SetViewRange(float newRange)
{
	if(newRange != m_viewRange)
	{
		m_viewRange = newRange;

		// Update view range
		float newLaserLengthScale = trackLength / (m_viewRange * laserSpeedOffset);
		m_laserTrackBuilder[0]->laserLengthScale = newLaserLengthScale;
		m_laserTrackBuilder[1]->laserLengthScale = newLaserLengthScale;

		// Reset laser tracks cause these won't be correct anymore
		m_laserTrackBuilder[0]->Reset();
		m_laserTrackBuilder[1]->Reset();
	}
}

void Track::SendLaserAlert(uint8 laserIdx)
{
	if (m_alertTimer[laserIdx] > 3.0f)
		m_alertTimer[laserIdx] = 0.0f;
}

void Track::SetLaneHide(bool hide, double duration)
{
	m_trackHideSpeed = hide ? 1.0f / duration : -1.0f / duration;
}

float Track::GetViewRange() const
{
	return m_viewRange;
}

float Track::GetButtonPlacement(uint32 buttonIdx)
{
	if(buttonIdx < 4)
		return buttonIdx * buttonWidth - (buttonWidth * 1.5f);
	else
		return (buttonIdx - 4) * fxbuttonWidth - (fxbuttonWidth * 0.5f);
}

