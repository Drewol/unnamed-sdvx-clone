#include "stdafx.h"
#include "Game.hpp"
#include "Track.hpp"

TimedEffect::TimedEffect(float duration)
{
	Reset(duration);
}
void TimedEffect::Reset(float duration)
{
	this->duration = duration; 
	time = duration;
}
void TimedEffect::Tick(float deltaTime)
{
	time -= deltaTime;
}

ButtonHitEffect::ButtonHitEffect() : TimedEffect(0)
{
}

void ButtonHitEffect::Reset(int buttonCode, Color color, bool hold)
{
	assert(buttonCode < 6);
	this->color = color;
	duration = hitEffectDuration;
	time = hitEffectDuration + (hold ? 0 : delayFadeDuration);
	held = buttonCode < 4 && ((track->hitEffectAutoplay && hold) || !track->hitEffectAutoplay);
}

void ButtonHitEffect::Tick(float deltaTime)
{
	if (held && delayFadeDuration)
		return;
	time = Math::Max(time - deltaTime, 0.f);
}

void ButtonHitEffect::Draw(class RenderQueue& rq)
{
	float x;
	float w;
	float hiSpeedAlphaOffset = 0;
	float yMult = 2.0f;
	if (buttonCode < 4)
	{
		// Scale hit effect alpha between hispeed range of 100 to 600
		if (delayFadeDuration > 0)
            hiSpeedAlphaOffset = 0.25f * (Math::Clamp(track->scrollSpeed - 100, 0.f, 500.f) / 500);

		w = Track::buttonWidth;
		x = (-Track::buttonWidth * 1.5f) + w * buttonCode;
		if (buttonCode < 2)
			x -= 0.5 * track->centerSplit * Track::buttonWidth;
		else
			x += 0.5 * track->centerSplit * Track::buttonWidth;
	}
	else
	{
		yMult = 1.0f;
		w = Track::buttonWidth * 2.0f;
		x = -Track::buttonWidth + w * (buttonCode - 4);
		if (buttonCode < 5)
			x -= 0.5 * track->centerSplit * Track::buttonWidth;
		else
			x += 0.5 * track->centerSplit * Track::buttonWidth;
	}

	Vector2 hitEffectSize = Vector2(w, 0.0f);
	hitEffectSize.y = track->scoreHitTexture->CalculateHeight(hitEffectSize.x) * yMult;
	Color c = color.WithAlpha(GetRate() * (alphaScale + hiSpeedAlphaOffset));
	c.w *= yMult / 2.f;
	track->DrawSprite(rq, Vector3(x, hitEffectSize.y * 0.5f, 0.0f), hitEffectSize, track->scoreHitTexture, c);
}

ButtonHitRatingEffect::ButtonHitRatingEffect(uint32 buttonCode, ScoreHitRating rating) : TimedEffect(0.3f), buttonCode(buttonCode), rating(rating)
{
	assert(buttonCode < 6);
	if(rating == ScoreHitRating::Miss)
		Reset(0.4f);
}

void ButtonHitRatingEffect::Draw(class RenderQueue& rq)
{
	float x;
	float w;
	float y;
	if(buttonCode < 4)
	{
		w = Track::buttonWidth;
		x = (-Track::buttonWidth * 1.5f) + w * buttonCode;
		if (buttonCode < 2)
		{
			x -= 0.5 * track->centerSplit * Track::buttonWidth;
		}
		else
		{
			x += 0.5 * track->centerSplit * Track::buttonWidth;
		}
		y = 0.15f;
	}
	else
	{
		w = Track::buttonWidth * 2.0f;
		x = -Track::buttonWidth + w * (buttonCode - 4);
		if (buttonCode < 5)
		{
			x -= 0.5 * track->centerSplit * Track::buttonWidth;
		}
		else
		{
			x += 0.5 * track->centerSplit * Track::buttonWidth;
		}
		y = 0.175f;
	}

	float iScale = 1.0f;
	uint32 on = 1;
	if(rating == ScoreHitRating::Miss) // flicker
		on = (uint32)floorf(GetRate() * 6.0f) % 2;
	else if(rating == ScoreHitRating::Perfect)
		iScale = cos(GetRate() * 12.0f) * 0.5f + 1.0f;

	if(on == 1)
	{
		Texture hitTexture = track->scoreHitTextures[(size_t)rating];

		// Shrink
		float t = GetRate();
		float add = 0.4f * t * (2 - t);

		// Size of effect
		Vector2 hitEffectSize = Vector2(Track::buttonWidth * (1.0f + add), 0.0f);
		hitEffectSize.y = hitTexture->CalculateHeight(hitEffectSize.x);

		// Fade out
		Color c = Color::White.WithAlpha(GetRate());
		// Intensity scale
		Utility::Reinterpret<Vector3>(c) *= iScale;

		track->DrawSprite(rq, Vector3(x, y + hitEffectSize.y * 0.5f, -0.02f), hitEffectSize, hitTexture, c, 0.0f);
	}
}

TimedHitEffect::TimedHitEffect(bool late) : TimedEffect(0.75f), late(late)
{

}

void TimedHitEffect::Draw(class RenderQueue& rq)
{
	float x = 0.0f;
    float y = 0.5f;

	float iScale = 1.0f;
	uint32 on = (uint32)floorf(time * 20) % 2;

	if (on == 1)
	{
		//Texture hitTexture = track->scoreTimeTextures[late ? 1 : 0];
		Texture hitTexture = track->scoreHitTextures[late ? 1 : 0];

		// Size of effect
		Vector2 hitEffectSize = Vector2(Track::buttonWidth * 2.f, 0.0f);
		hitEffectSize.y = hitTexture->CalculateHeight(hitEffectSize.x);

		// Fade out
		Color c = Color::White;
		// Intensity scale
		Utility::Reinterpret<Vector3>(c) *= iScale;

		track->DrawSprite(rq, Vector3(x, y + hitEffectSize.y * 0.5f, -0.02f), hitEffectSize, hitTexture, c, 0.0f);
	}
}