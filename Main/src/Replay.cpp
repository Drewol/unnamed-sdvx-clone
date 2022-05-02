#include "stdafx.h"
#include "Replay.hpp"
#include "Scoring.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Shared/FileStream.hpp"
#include "Beatmap/MapDatabase.hpp"
#include <stdio.h>
#include "zlib.h"
#include "Shared/CompressedFileStream.hpp"

bool Replay::Save(String path)
{
	File replayFile;
	if (!replayFile.OpenWrite(path))
		return false;
	CompressedFileWriter fw(replayFile);
	bool res = fw.SerializeObject(*this) && fw.FinishCompression();
	if (!res)
		Logf("[replay] Failed to save replay for '%s'", Logger::Severity::Error, *this->m_chartInfo.title);
	else
		Logf("[replay] Replay for '%s' saved to '%s'", Logger::Severity::Info, *this->m_chartInfo.title, *path);
	replayFile.Close();
	return res;
}

Replay* Replay::Load(String path, ReplayType type)
{
	Replay* replay = new Replay(type);
	File replayFile;
	if (!replayFile.OpenRead(path))
		return nullptr;

	CompressedFileReader fr(replayFile);
	if (!fr.SerializeObject(replay))
	{
		Logf("[replay] Failed to load replay '%s'", Logger::Severity::Error, *path);
		delete replay;
		replayFile.Close();
		return nullptr;
	}
	replayFile.Close();
	replay->filePath = path;

	if (replay->m_requiresRewrite)
		replay->Save(path);

	return replay;
}

bool Replay::SerializeLegacy(BinaryStream& stream, Replay*& obj)
{
	if (obj == nullptr)
		return false;

	// We don't support writing legacy replays
	if (!stream.IsReading() || obj->m_initialized)
		return false;

	Vector<SimpleHitStat> legacy;
	if (!stream.SerializeObject(legacy))
		return false;
	for (auto& l : legacy)
	{
		obj->m_judgementEvents.push_back(l);
	}

	// If we have an old legacy replay it will use the old hitwindows
	//https://github.com/Drewol/unnamed-sdvx-clone/blob/ae736ebd0d497ea1004e07941fbe43ab9c86d5aa/Main/src/Scoring.cpp#L7
	if (!stream.Serialize(&(obj->m_hitWindow.perfect), 4))
		obj->m_hitWindow.perfect = 46;
	if (!stream.Serialize(&(obj->m_hitWindow.good), 4))
		obj->m_hitWindow.good = 92;
	if (!stream.Serialize(&(obj->m_hitWindow.hold), 4))
		obj->m_hitWindow.hold = 138;
	if (!stream.Serialize(&(obj->m_hitWindow.miss), 4))
		obj->m_hitWindow.miss = 250;
	if (!stream.Serialize(&(obj->m_hitWindow.slam), 4))
		obj->m_hitWindow.slam = 75;

	// Maybe use current offset instead?
	obj->SetOffsets(ReplayOffsets(0,0,0,0));


	obj->m_initialized = true;
	obj->m_type = Replay::ReplayType::Legacy;
	return true;
}

bool Replay::StaticSerialize(BinaryStream& stream, Replay*& obj)
{
	bool isRead = stream.IsReading();

	if (obj == nullptr)
		return false;

	uint16 version = REPLAY_VERSION;
	if (isRead)
	{
		if (obj->m_initialized)
			return false;

		uint32 magic = 0;
		if (!stream.SerializeObject(magic))
			return false;

		bool hasNullPadding = magic == 0;

		// The first 4 bytes may be null to prevent crashes on old versions
		if (hasNullPadding && !stream.SerializeObject(magic))
			return false;

		if (magic != REPLAY_MAGIC && magic != COMPRESSED_REPLAY_MAGIC)
		{
			stream.Seek(0);
			return Replay::SerializeLegacy(stream, obj);
		}

		if (!hasNullPadding)
			obj->m_requiresRewrite = true;

		// File is not legacy
		if (obj->m_type == Replay::ReplayType::Legacy)
			return false;

		if (!stream.SerializeObject(version) || version > REPLAY_VERSION)
		{
			// May not be backwards compatable so ignore it
			Logf("[replay] Unable to open replay %s. This replay is too new for this version of USC", Logger::Severity::Error, *obj->filePath);
			return false;
		}

		if (magic == COMPRESSED_REPLAY_MAGIC)
		{
			CompressedFileReader* cfr = dynamic_cast<CompressedFileReader*>(&stream);
			if (!cfr || !cfr->StartCompression())
			{

				Logf("[replay] Unable to open replay '%s'. Compressed replays are not supported", Logger::Severity::Error, *obj->filePath);
				return false;
			}
		}

	}
	else if (obj->m_initialized)
	{
		uint32 padding = 0;
		uint32 magic = REPLAY_MAGIC;

		CompressedFileWriter* cfw = dynamic_cast<CompressedFileWriter*>(&stream);
#ifdef ZLIB_FOUND
		bool useCompression = g_gameConfig.GetBool(GameConfigKeys::UseCompressedReplay);
		if (cfw && useCompression && cfw->InitCompression())
			magic = COMPRESSED_REPLAY_MAGIC;
#endif

		stream << padding << magic << version;
		if (!stream.IsOk())
			return false;

		if (magic == COMPRESSED_REPLAY_MAGIC && cfw)
			cfw->StartCompression();
	}
	else
	{
		return false;
	}

	// c++ is mean :(
	auto* ci = &obj->m_chartInfo;
	if (!ReplayChartInfo::StaticSerialize(stream, ci))
		return false;
	auto* si = &obj->m_scoreInfo;
	if (!ReplayScoreInfo::StaticSerialize(stream, si))
		return false;

	stream << obj->m_hitWindow;

	auto* oi = &obj->m_offsets;
	if (!ReplayOffsets::StaticSerialize(stream, oi))
		return false;

	stream << obj->m_judgementEvents;

	// Must be last thing since its optional to read
	if (!isRead || obj->m_type != Replay::ReplayType::NoInput)
		stream << obj->m_inputEvents;

	if (isRead && stream.IsOk())
		obj->m_initialized = true;

	return stream.IsOk();
}

void Replay::UpdateToTime(MapTime lastTime)
{
	if (lastTime <= m_lastEvalTime)
		return;

	// TODO(replay) adjust the time using the offsets?
	for (; m_nextJudgementIndex < m_judgementEvents.size(); m_nextJudgementIndex++)
	{
		const ReplayJudgement* j = &m_judgementEvents[m_nextJudgementIndex];

		// Legacy has the recorded time in the time field
		MapTime time = (m_type == ReplayType::Legacy ?
			j->time : j->GetHitTime());

		if (time > lastTime)
			break;

		if (m_isPlaying)
		{
			// We can only do this if we don't reallocate this vector
			// so we use m_initialized to keep it immutable after init
			m_playbackQueue[j->lane].push_back(j);
			Logf("[replay] queueing j %u[l%u][d%d][r%u]", Logger::Severity::Debug,
				j->time, j->lane, j->delta, j->rating
			);
		}
		else if (j->rating < 3)
		{
			m_currentMaxScore += 2;
			m_currentScore += j->rating;
		}
	}
	m_lastEvalTime = lastTime;
}

const ReplayJudgement* Replay::FindNextJudgement(int lane, MapTime future/*=0*/) const
{
	assert(m_isPlaying);
	assert(lane < 8);
	if (auto* ret = PeekNextJudgement(lane))
		return ret;

	for (size_t i = m_nextJudgementIndex; i < m_judgementEvents.size(); i++)
	{
		const ReplayJudgement* j = &m_judgementEvents[i];

		MapTime time = j->GetHitTime();

		if (time > m_lastEvalTime + future)
			break;

		if (j->lane != lane)
			continue;

		return j;
	}
	return nullptr;
}

const ReplayJudgement* Replay::PeekNextJudgement(int lane) const
{
	assert(m_isPlaying);
	assert(lane < 8);
	const auto& q = m_playbackQueue[lane];
	if (!q.empty())
		return q.front();
	return nullptr;
}

const ReplayJudgement* Replay::PopNextJudgement(int lane, bool score)
{
	assert(m_isPlaying);
	assert(lane < 8);
	auto& q = m_playbackQueue[lane];
	if (q.empty())
		return nullptr;

	auto* ret = q.front();
	q.pop_front();
	if (score && ret->rating < 3)
	{
		m_currentMaxScore += 2;
		m_currentScore += ret->rating;
	}
	return ret;
}

bool ReplayScoreInfo::MatchesScore(const ScoreIndex* s)
{
	if (s->score != this->score
		|| s->crit != this->crit
		|| s->almost != this->almost
		|| s->miss != this->miss
		|| s->gauge != this->gauge)
	{
		return false;
	}
	return true;
}

ChartIndex* Replay::FindChart(MapDatabase** databaseHandle) const
{
	MapDatabase* database = nullptr;
	if (databaseHandle)
		database = *databaseHandle;

	if (!database)
	{
		database = new MapDatabase(true);
		database->SetChartUpdateBehavior(g_gameConfig.GetBool(GameConfigKeys::TransferScoresOnChartUpdate));
		database->FinishInit();
		database->LoadDatabaseWithoutSearching();
	}

	String hash;
	if (m_chartInfo.IsInitialized())
		hash = m_chartInfo.hash;
	else if (m_scoreIndex)
		hash = m_scoreIndex->chartHash;
	else
	{
		String path = Path::RemoveLast(this->filePath);
		Path::RemoveLast(path, &hash);
	}

	ChartIndex* chart = database->FindFirstChartByHash(hash);

	if (databaseHandle)
		*databaseHandle = database;

	return chart;
}
