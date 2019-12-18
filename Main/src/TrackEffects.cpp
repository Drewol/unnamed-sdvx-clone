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

ButtonHitEffect::ButtonHitEffect(uint32 buttonCode, Color color) : TimedEffect(0.2f), buttonCode(buttonCode), color(color)
{
	assert(buttonCode < 6);
}
void ButtonHitEffect::Draw(class RenderQueue& rq)
{
	float x = 0.0f;
	float w = track->buttonWidth;
	float yMult = 2.0f;
	if(buttonCode < 4)
	{
		w = track->buttonWidth;
		x = (-track->buttonWidth * 1.5f) + w * buttonCode;
	}
	else
	{
		yMult = 1.0f;
		w = track->buttonWidth * 2.0f;
		x = -track->buttonWidth + w * (buttonCode - 4);
	}

	Vector2 hitEffectSize = Vector2(w, 0.0f);
	hitEffectSize.y = track->scoreHitTexture->CalculateHeight(hitEffectSize.x) * yMult;
	Color c = color.WithAlpha(GetRate());
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
	float x = 0.0f;
	float w = track->buttonWidth;
	float y = 0.0f;
	if(buttonCode < 4)
	{
		w = track->buttonWidth;
		x = (-track->buttonWidth * 1.5f) + w * buttonCode;
		y = 0.15f;
	}
	else
	{
		w = track->buttonWidth * 2.0f;
		x = -track->buttonWidth + w * (buttonCode - 4);
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
		Vector2 hitEffectSize = Vector2(track->buttonWidth * (1.0f + add), 0.0f);
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
	float w = track->buttonWidth * 2;
	float y = 0.5f;

	float iScale = 1.0f;
	uint32 on = (uint32)floorf(time * 20) % 2;

	if (on == 1)
	{
		//Texture hitTexture = track->scoreTimeTextures[late ? 1 : 0];
		Texture hitTexture = track->scoreHitTextures[late ? 1 : 0];

		// Size of effect
		Vector2 hitEffectSize = Vector2(track->buttonWidth * 2.f, 0.0f);
		hitEffectSize.y = hitTexture->CalculateHeight(hitEffectSize.x);

		// Fade out
		Color c = Color::White;
		// Intensity scale
		Utility::Reinterpret<Vector3>(c) *= iScale;

		track->DrawSprite(rq, Vector3(x, y + hitEffectSize.y * 0.5f, -0.02f), hitEffectSize, hitTexture, c, 0.0f);
	}
}