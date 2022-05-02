#include "stdafx.h"
#include "LaserTrackBuilder.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include "Track.hpp"
#include "GameConfig.hpp"
#include <algorithm>

using Shared::Rect;
using Shared::Rect3D;


LaserTrackBuilder::LaserTrackBuilder(class OpenGL* gl, class Track* track, uint32 laserIndex)
{
	m_gl = gl;
	m_track = track;
	m_laserIndex = laserIndex;
	m_trackWidth = track->trackWidth;
	m_laserWidth = track->laserWidth;
	laserTextureSize = track->laserTextures[0]->GetSize(); // NOTE: expects left/right textures to be the same size!
	laserEntryTextureSize = track->laserTailTextures[0]->GetSize();
	laserExitTextureSize = track->laserTailTextures[2]->GetSize();
}
Mesh LaserTrackBuilder::GenerateTrackMesh(class BeatmapPlayback& playback, LaserObjectState* laser)
{
	if(m_objectCache.Contains(laser))
		return m_objectCache[laser];

	Mesh newMesh = MeshRes::Create(m_gl);

	const float length = playback.ToViewDistance(laser->time, laser->duration);

	if((laser->flags & LaserObjectState::flag_Instant) != 0) // Slam segment
	{
		float left, right, offsetT, offsetB, uvT, uvB;
		left = laser->points[0] * effectiveWidth - effectiveWidth * 0.5f;
		right = laser->points[1] * effectiveWidth - effectiveWidth * 0.5f;
		float halfWidth = actualLaserWidth * 0.5f;
		offsetT = -actualLaserWidth;
		offsetB = -offsetT;
		uvB = 1.5f;
		uvT = -0.5f;


		if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		{
			left = (laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;
			right = (laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;
		}

		// If corners should be placed, connecting the texture to the previous laser
		bool swapped = false;


		if(laser->points[0] > laser->points[1])
		{
			// <------
			std::swap(left, right);
			std::swap(offsetT, offsetB);
			std::swap(uvB, uvT);
			swapped = true;
		}// else ------>

		// Generate positions for middle top and bottom
		float slamLength = playback.ToViewDistance(laser->time, slamDuration) * laserLengthScale;
		float halfLength = slamLength * 0.5;
		Rect3D centerMiddle = Rect3D(left, slamLength + halfLength, right, -halfLength);

		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
			{ { centerMiddle.Right() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 1.0f } }, // BR
			{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR

			{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
			{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR
			{ { centerMiddle.Left() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 0.0f } }, // TL
		};

		// Generate left corner
		{
			Rect3D leftCenter = Rect3D(left - actualLaserWidth, centerMiddle.Top() - halfLength, left + halfWidth, centerMiddle.Bottom() + halfLength);

			Vector<MeshGenerators::SimpleVertex> leftVerts;
			if(swapped)
			{

				leftVerts =
				{
					{ { leftCenter.Right(), leftCenter.Top(),  0.0f },{ 1.0f, 0.0f } }, // BR
					{ { leftCenter.Left(), leftCenter.Top(),  0.0f },{ -0.5f, 0.0f } }, // BL
					{ { leftCenter.Left(), leftCenter.Bottom() - halfLength,  0.0f },{ -0.5f, 1.0f } }, // TL
				};

			}
			else
			{
				leftVerts =
				{
					{ { leftCenter.Left(), leftCenter.Bottom(),  0.0f },{ -0.5f, 0.0f } }, // BL
					{ { leftCenter.Right(), leftCenter.Bottom(),  0.0f },{ 1.0f, 0.0f } }, // BR
					{ { leftCenter.Left(), leftCenter.Top() + halfLength,  0.0f },{ -0.5f, 1.0f } }, // TL
				};
			}
			for (auto& v : leftVerts)
				verts.Add(v);
		}

		// Generate right corner
		{
			Rect3D rightCenter = Rect3D(right - halfWidth, centerMiddle.Top() - halfLength, right + actualLaserWidth, centerMiddle.Bottom() + halfLength);
			Vector<MeshGenerators::SimpleVertex> rightVerts;
			if(swapped)
			{
				rightVerts =
				{
					{ { rightCenter.Left(), rightCenter.Bottom(),  0.0f },{ 0.0f, 0.0f } }, // BL
					{ { rightCenter.Right(), rightCenter.Bottom(),  0.0f },{ 1.5f, 1.0f } }, // BR
					{ { rightCenter.Right(), rightCenter.Top() + halfLength,  0.0f },{ 1.5f, 1.0f } }, // TR
				};
			}
			else
			{
				rightVerts =
				{
					{ { rightCenter.Right(), rightCenter.Bottom() - halfLength,  0.0f },{ 1.5f, 1.0f } }, // TR
					{ { rightCenter.Right(), rightCenter.Top(),  0.0f },{ 1.5f, 1.0f } }, // BR
					{ { rightCenter.Left(), rightCenter.Top(),  0.0f },{ 0.0f, 0.0f } }, // BL
				};
			}

			for (auto& v : rightVerts)
				verts.Add(v);
		}

		newMesh->SetData(verts);
		newMesh->SetPrimitiveType(PrimitiveType::TriangleList);
	}
	else
	{
		float prevLength = 0.0f;
		if(laser->prev && (laser->prev->flags & LaserObjectState::flag_Instant) != 0)
		{
			// Previous slam length
			prevLength = playback.ToViewDistance(laser->prev->time, slamDuration) * laserLengthScale;
		}

		Vector2 points[2];

		// Connecting center points
		points[0] = Vector2(laser->points[0] * effectiveWidth - effectiveWidth * 0.5f, prevLength); // Bottom
		points[1] = Vector2(laser->points[1] * effectiveWidth - effectiveWidth * 0.5f, length * laserLengthScale); // Top
		if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		{
			points[0] = Vector2((laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f, prevLength); // Bottom
			points[1] = Vector2((laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f, length * laserLengthScale); // Top
		}

		float uMin = -0.5f;
		float uMax = 1.5f;

		float vMin = 0.0f;
		float vMax = (int)((length * laserLengthScale) / actualLaserHeight);

		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { points[0].x - actualLaserWidth, points[0].y,  0.0f },{ uMin, vMax } }, // BL
			{ { points[0].x + actualLaserWidth, points[0].y,  0.0f },{ uMax, vMax } }, // BR
			{ { points[1].x + actualLaserWidth, points[1].y,  0.0f },{ uMax, vMin } }, // TR

			{ { points[0].x - actualLaserWidth, points[0].y,  0.0f },{ uMin, vMax } }, // BL
			{ { points[1].x + actualLaserWidth, points[1].y,  0.0f },{ uMax, vMin } }, // TR
			{ { points[1].x - actualLaserWidth, points[1].y,  0.0f },{ uMin, vMin } }, // TL
		};


		newMesh->SetData(verts);
		newMesh->SetPrimitiveType(PrimitiveType::TriangleList);
	}

	// Cache this mesh
	m_objectCache.Add(laser, newMesh);
	return newMesh;
}

Mesh LaserTrackBuilder::GenerateTrackEntry(class BeatmapPlayback& playback, LaserObjectState* laser)
{
	assert(laser->prev == nullptr);
	if(m_cachedEntries.Contains(laser))
		return m_cachedEntries[laser];

	Mesh newMesh = MeshRes::Create(m_gl);

	// Starting point of laser
	float startingX = laser->points[0] * effectiveWidth - effectiveWidth * 0.5f;
	if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		startingX = (laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;



	// Length of the tail
	float length = (float)laserEntryTextureSize.y / (float)laserEntryTextureSize.x * actualLaserWidth;

	Vector<MeshGenerators::SimpleVertex> verts;
	Rect3D pos = Rect3D(Vector2(startingX - actualLaserWidth, -length), Vector2(actualLaserWidth * 2, length));
	Rect uv = Rect(-0.5f, 0.0f, 1.5f, 1.0f);
	MeshGenerators::GenerateSimpleXYQuad(pos, uv, verts);

	newMesh->SetData(verts);
	newMesh->SetPrimitiveType(PrimitiveType::TriangleList);

	// Cache this mesh
	m_cachedEntries.Add(laser, newMesh);
	return newMesh;

}
Mesh LaserTrackBuilder::GenerateTrackExit(class BeatmapPlayback& playback, LaserObjectState* laser)
{
	assert(laser->next == nullptr);
	if(m_cachedExits.Contains(laser))
		return m_cachedExits[laser];

	Mesh newMesh = MeshRes::Create(m_gl);

	// Ending point of laser 
	float startingX = laser->points[1] * effectiveWidth - effectiveWidth * 0.5f;
	if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		startingX = (laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;

	// Length of the tail
	float length = (float)laserExitTextureSize.y / (float)laserExitTextureSize.x * actualLaserWidth;

	// Length of this segment
	float prevLength = 0.0f;
	if((laser->flags & LaserObjectState::flag_Instant) != 0)
	{
		prevLength = playback.ToViewDistance(laser->time, slamDuration) * laserLengthScale;
	}
	else
	{
		prevLength = playback.ToViewDistance(laser->time, laser->duration) * laserLengthScale;
	}

	Vector<MeshGenerators::SimpleVertex> verts;
	Rect3D pos = Rect3D(Vector2(startingX - actualLaserWidth, prevLength), Vector2(actualLaserWidth * 2, length));
	Rect uv = Rect(-0.5f, 0.0f, 1.5f, 1.0f);
	MeshGenerators::GenerateSimpleXYQuad(pos, uv, verts);

	newMesh->SetData(verts);
	newMesh->SetPrimitiveType(PrimitiveType::TriangleList);

	// Cache this mesh
	m_cachedExits.Add(laser, newMesh);
	return newMesh;
}

float LaserTrackBuilder::GetLaserLengthScaleAt(MapTime time)
{
	/// TODO: return scale based on speed of timing point to change horizontal laser thickness
	return 1.0f;
}

void LaserTrackBuilder::m_RecalculateConstants()
{
	// Calculate amount to scale laser size to fit the texture border in
	//assert(laserTextureSize.x == laserTextureSize.y); // Use Square texture
	const float laserCenterAmount = ((float)laserTextureSize.x - ((float)laserBorderPixels * 2)) / (float)(laserTextureSize.x);
	const float laserBorderAmount = (1.0f - laserCenterAmount);

	// The uv coordinates to sample the laser without the border, or only the border
	textureBorder = (float)laserBorderPixels / laserTextureSize.x;
	invTextureBorder = 1.0f - textureBorder;

	// The the size of the laser with compensation added for the border
	actualLaserWidth = m_laserWidth;
	actualLaserHeight = actualLaserWidth * laserTextureSize.y / laserTextureSize.x;

	// The width of the laser without the border
	laserWidthNoBorder = actualLaserWidth * laserCenterAmount;
	// World size of a single border around a laser
	realBorderSize = (actualLaserWidth - laserWidthNoBorder) * 0.5f;

	// The length of the horizontal slam segments
	slamDuration = 50 * g_gameConfig.GetFloat(GameConfigKeys::SlamThicknessMultiplier);

	// The effective area in which the center point of the laser can move
	effectiveWidth = m_trackWidth - m_laserWidth;
}

void LaserTrackBuilder::m_Cleanup(MapTime newTime, Map<LaserObjectState*, Mesh>& arr)
{
	// Cleanup unused meshes
	for(auto it = arr.begin(); it != arr.end();)
	{
		LaserObjectState* obj = it->first;
		MapTime endTime = obj->time + obj->duration + 1000;
		if(newTime > endTime)
		{
			it = arr.erase(it);
			continue;
		}
		it++;
	}
}
void LaserTrackBuilder::Reset()
{
	m_objectCache.clear();
	m_cachedEntries.clear();
	m_cachedExits.clear();
	m_RecalculateConstants();
}
void LaserTrackBuilder::Update(MapTime newTime)
{
	m_Cleanup(newTime, m_objectCache);
	m_Cleanup(newTime, m_cachedEntries);
	m_Cleanup(newTime, m_cachedExits);
}