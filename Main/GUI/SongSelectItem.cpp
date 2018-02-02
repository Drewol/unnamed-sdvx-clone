#include "stdafx.h"
#include "SongSelectItem.hpp"
#include <GUI/GUI.hpp>
#include "Application.hpp"

#include <Scoring.hpp>
#include <Beatmap/Beatmap.hpp>
#include <Beatmap/MapDatabase.hpp>

static float padding = 5.0f;

/* A frame that displays the jacket+frame of a single map difficulty */
class SongDifficultyFrame : public GUIElementBase
{
private:
	Ref<SongSelectStyle> m_style;
	static const Vector2 m_size;
	DifficultyIndex* m_diff;
	Texture m_jacket;
	Texture m_frame;
	Text m_lvlText;

	bool m_selected = false;
	float m_fade = 0.0f;

public:
	SongDifficultyFrame(Ref<SongSelectStyle> style, DifficultyIndex* diff)
	{
		m_style = style;
		m_diff = diff;
		m_frame = m_style->diffFrames[Math::Min<size_t>(diff->settings.difficulty, m_style->numDiffFrames-1)];
	}
	virtual void Render(GUIRenderData rd)
	{
		m_TickAnimations(rd.deltaTime);

		// Load jacket?
		if(!m_jacket || m_jacket == m_style->loadingJacketImage)
		{
			String jacketPath = m_diff->path;
			jacketPath = Path::Normalize(Path::RemoveLast(jacketPath) + Path::sep + m_diff->settings.jacketPath);
			m_jacket = m_style->GetJacketThumnail(jacketPath);
		}

		// Render lvl text?
		if(!m_lvlText)
		{
			WString lvlStr = Utility::WSprintf(L"%d", m_diff->settings.level);
			m_lvlText = rd.guiRenderer->font->CreateText(lvlStr, 20);
		}

		Rect area = GUISlotBase::ApplyFill(FillMode::Fit, m_size, rd.area);
		static const float scale = 0.1f;
		area.pos -= area.size * scale * m_fade * 0.5f;
		area.size += area.size * scale * m_fade;

		// Custom rendering to combine jacket image and frame
		Transform transform;
		transform *= Transform::Translation(area.pos);
		transform *= Transform::Scale(Vector3(area.size.x, area.size.y, 1.0f));
		MaterialParameterSet params;
		params.SetParameter("selected", m_selected ? 1.0f : 0.0f);
		params.SetParameter("frame", m_frame);
		if(m_jacket)
			params.SetParameter("jacket", m_jacket);
		rd.rq->Draw(transform, rd.guiRenderer->guiQuad, m_style->diffFrameMaterial, params);

		// Render level text
		Rect textRect = Rect(Vector2(), m_lvlText->size);
		Rect textFrameRect = textRect;
		textFrameRect.size.x = area.size.x * 0.25f;
		textFrameRect = GUISlotBase::ApplyAlignment(Vector2(1, 0), textFrameRect, area);
		textRect = GUISlotBase::ApplyAlignment(Vector2(0.5f, 0.5f), textRect, textFrameRect);
		rd.guiRenderer->RenderRect(textFrameRect, Color::Black.WithAlpha(0.5f));
		rd.guiRenderer->RenderText(m_lvlText, textRect.pos, Color::White);
	}
	virtual Vector2 GetDesiredSize(GUIRenderData rd)
	{
		Rect base = GUISlotBase::ApplyFill(FillMode::Fit, m_size, rd.area);
		return base.size;
	}
	virtual void SetSelected(bool selected)
	{
		if(m_selected != selected)
		{
			m_selected = selected;
			// Zoom in animation
			AddAnimation(Ref<IGUIAnimation>(
				new GUIAnimation<float>(&m_fade, selected ? 1.0f : 0.0f, 0.2f)), true);
		}
	}
	
	int GetScore()
	{
		if (m_diff->scores.size() == 0)
			return 0;

		return m_diff->scores.back()->score;
	}

	double GetGauge()
	{
		if (m_diff->scores.size() == 0)
			return 0;

		return m_diff->scores.back()->gauge;
	}

	bool HasScores()
	{
		return m_diff->scores.size() > 0;
	}

	uint32 CalculateGrade()
	{
		uint32 value = (uint32)(m_diff->scores.back()->score * 0.9 + m_diff->scores.back()->gauge * 1000000.0);
		if (value > 9800000) // AAA
			return 0;
		if (value > 9400000) // AA
			return 1;
		if (value > 8900000) // A
			return 2;
		if (value > 8000000) // B
			return 3;
		if (value > 7000000) // C
			return 4;
		return 5; // D
	}

};
const Vector2 SongDifficultyFrame::m_size = Vector2(512, 512);

SongSelectItem::SongSelectItem(Ref<SongSelectStyle> style)
{
	m_style = style;

	// Background image
	{
		m_bg = new Panel();
		m_bg->imageFillMode = FillMode::None;
		m_bg->imageAlignment = Vector2(1.0f, 0.5f);
		Slot* bgSlot = Add(m_bg->MakeShared());
		bgSlot->anchor = Anchors::Full;
	}

	// Add Main layout container
	{
		m_mainVert = new LayoutBox();
		m_mainVert->layoutDirection = LayoutBox::Vertical;
		Slot* slot = Add(m_mainVert->MakeShared());
	}

	// Add Titles
	{
		m_title = new Label();
		m_title->SetFontSize(40);
		m_title->SetText(L"<title>");
		LayoutBox::Slot* slot = m_mainVert->Add(m_title->MakeShared());
		slot->padding = Margin(0, -5.0f);

		m_artist = new Label();
		m_artist->SetFontSize(32);
		m_artist->SetText(L"<artist>");
		slot = m_mainVert->Add(m_artist->MakeShared());
		slot->padding = Margin(0, -5.0f);
	}

	// Add diff select
	{
		m_diffSelect = new LayoutBox();
		m_diffSelect->layoutDirection = LayoutBox::Horizontal;
		Slot* slot = Add(m_diffSelect->MakeShared());
		slot->anchor = Anchor(0.0f, 0.4f, 1.0f, 1.0f - 0.09f);
		slot->padding = Margin(padding, 0, 0, 0);
		slot->allowOverflow = true;
	}



	SwitchCompact(true);
}

void SongSelectItem::PreRender(GUIRenderData rd, GUIElementBase*& inputElement)
{
	Canvas::PreRender(rd, inputElement);
}
void SongSelectItem::Render(GUIRenderData rd)
{
	// Update fade
	m_title->color.w = fade;
	m_artist->color.w = fade;

	// Update inner offset
	m_mainVert->slot->padding.left = padding + innerOffset;
	m_diffSelect->slot->padding.left = padding + innerOffset;

	// Render canvas
	Canvas::Render(rd);
}
Vector2 SongSelectItem::GetDesiredSize(GUIRenderData rd)
{
	Vector2 sizeOut = m_bg->texture->GetSize();
	sizeOut.x = Math::Min(sizeOut.x, rd.area.size.x);
	return sizeOut;
}
// TODO(local): Change this to SetEntry or something
void SongSelectItem::SetIndex(struct SongSelectIndex map)
{
	const BeatmapSettings& settings = map.GetDifficulties()[0]->settings;
	m_title->SetText(Utility::ConvertToWString(settings.title));
	m_artist->SetText(Utility::ConvertToWString(settings.artist));

	// Add all difficulty icons
	m_diffSelect->Clear();
	m_diffSelectors.clear();
	for(auto d : map.GetDifficulties())
	{
		SongDifficultyFrame* frame = new SongDifficultyFrame(m_style, d);
		LayoutBox::Slot* slot = m_diffSelect->Add(frame->MakeShared());
		slot->padding = Margin(2);
		slot->allowOverflow = true;
		m_diffSelectors.Add(frame);
	}

	// Add score display to the end of diff select
	{
		m_score = new Label();
		m_score->SetFontSize(32);
		m_score->SetText(L"<score>");
		LayoutBox::Slot* slot = m_diffSelect->Add(m_score->MakeShared());
		slot->padding = Margin(10);
		slot->alignment = Vector2(0.0f, 0.5f);
		slot->allowOverflow = true;
	}
}
void SongSelectItem::SwitchCompact(bool compact)
{
	Slot* mainSlot = (Slot*)m_mainVert->slot;
	Slot* bgSlot = (Slot*)m_bg;
	if(compact)
	{
		m_bg->texture = m_style->frameSub;
		m_bg->texture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
		m_diffSelect->visibility = Visibility::Collapsed;

		mainSlot->anchor = Anchor(0.0f, 0.5f, 1.0, 0.5f);
		mainSlot->autoSizeX = true;
		mainSlot->autoSizeY = true;
		mainSlot->alignment = Vector2(0.0f, 0.5f);
	}
	else
	{
		m_bg->texture = m_style->frameMain;
		m_bg->texture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
		m_diffSelect->visibility = Visibility::Visible;

		// Take 0.3 of top
		mainSlot->anchor = Anchor(0.0f, 0.09f, 1.0, 0.3f);
		mainSlot->autoSizeX = true;
		mainSlot->autoSizeY = true;
		mainSlot->alignment = Vector2(0.0f, 0.5f);
	}
}
void SongSelectItem::SetSelectedDifficulty(int32 selectedIndex)
{
	if(selectedIndex < 0)
		return;
	if(selectedIndex < (int32)m_diffSelectors.size())
	{
		if(m_selectedDifficulty < (int32)m_diffSelectors.size())
		{
			m_diffSelectors[m_selectedDifficulty]->SetSelected(false);
		}
		m_diffSelectors[selectedIndex]->SetSelected(true);

		if (m_diffSelectors[selectedIndex]->HasScores())
		{
			int score = m_diffSelectors[selectedIndex]->GetScore();
			double gauge = m_diffSelectors[selectedIndex]->GetGauge();
			WString grade = Utility::ConvertToWString(Scoring::CalculateGrade(score));
			int gaugeDisplay = gauge * 100;

			m_score->SetText(Utility::WSprintf(L"%08d\n%d%%\n%ls", score, gaugeDisplay, grade));
		}
		else
		{
			m_score->SetText(L"00000000\n0\%\nNo Play");
		}
		m_selectedDifficulty = selectedIndex;
	}
}

SongStatistics::SongStatistics(Ref<SongSelectStyle> style)
{
	m_style = style;

	m_bg = new Panel();
	m_bg->color = Color::White.WithAlpha(0.5f);
	Slot* slot = Add(m_bg->MakeShared());
	slot->anchor = Anchors::Full;
	slot->SetZOrder(-1);
}

