#include "stdafx.h"
#include "BackbeatCatalog.hpp"

#include <Beatmap/Beatmap.hpp>
#include <Beatmap/MapDatabase.hpp>
#include <Beatmap/TinySHA1.hpp>
#include <backbeat.h>
#include <limits>
#include <sqlite3.h>
#include <zlib.h>

namespace
{
	struct TableOwner
	{
		bkb_table* table = nullptr;
		~TableOwner() { if (table) bkb_table_free(table); }
	};

	struct CourseOwner
	{
		bkb_course* course = nullptr;
		~CourseOwner() { if (course) bkb_course_free(course); }
	};

	struct CollectionListOwner
	{
		bkb_collection_metadata_list* list = nullptr;
		~CollectionListOwner() { if (list) bkb_collection_metadata_list_free(list); }
	};

	String CopyString(bkb_str value)
	{
		return value.ptr ? String(value.ptr, value.len) : String();
	}

	String TakeString(bkb_string value)
	{
		String result(value.ptr, value.len);
		bkb_string_free(value);
		return result;
	}

	String Sha1(const uint8* data, size_t size)
	{
		uint32 digest[5];
		sha1::SHA1 hash;
		hash.processBytes(data, size);
		hash.getDigest(digest);
		return Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);
	}

	bool LogBackbeatError(const char* operation, bkb_error_code code)
	{
		if (code == BKB_OK)
			return false;
		Logf("Backbeat %s failed: %s", Logger::Severity::Warning, operation, bkb_error_string(code));
		return true;
	}

	bool InflateChart(const void* input, size_t inputSize, size_t outputSize, Buffer& output)
	{
		if (!input || inputSize > std::numeric_limits<uInt>::max() || outputSize > std::numeric_limits<uInt>::max())
			return false;

		output.resize(outputSize);
		z_stream stream{};
		stream.next_in = (Bytef*)input;
		stream.avail_in = (uInt)inputSize;
		stream.next_out = output.data();
		stream.avail_out = (uInt)output.size();
		if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK)
			return false;
		int result = inflate(&stream, Z_FINISH);
		inflateEnd(&stream);
		return result == Z_STREAM_END && stream.total_out == outputSize;
	}

	void BindText(sqlite3_stmt* statement, int index, const String& value)
	{
		sqlite3_bind_text(statement, index, value.c_str(), (int)value.size(), SQLITE_TRANSIENT);
	}

	enum class CourseTagType
	{
		Boolean,
		Integer,
		Float,
	};

	struct CourseTagSetting
	{
		const char* key;
		const char* setting;
		CourseTagType type;
	};

	nlohmann::json CourseSettings(const bkb_course& course, const String& url)
	{
		static const CourseTagSetting supported[] = {
			{ "kshoot/course-clear", "clear", CourseTagType::Boolean },
			{ "kshoot/course-min-percentage", "min_percentage", CourseTagType::Integer },
			{ "kshoot/course-min-gauge", "min_gauge", CourseTagType::Float },
			{ "kshoot/course-min-average-percentage", "min_average_percentage", CourseTagType::Integer },
			{ "kshoot/course-mirror", "mirror", CourseTagType::Boolean },
			{ "kshoot/course-use-sdvx-complete-percentage", "use_sdvx_complete_percentage", CourseTagType::Boolean },
		};
		nlohmann::json settings = nlohmann::json::object();
		for (size_t i = 0; i < course.tags_len; i++)
		{
			String key = CopyString(course.tags[i].key);
			String value = CopyString(course.tags[i].value);
			if (key == "kshoot/gauge")
			{
				if (value == "excessive")
					settings["excessive_gauge"] = true;
				else if (value == "permissive")
					settings["permissive_gauge"] = true;
				else if (value == "blastive")
					settings["blastive_gauge"] = true;
				else
				{
					Logf("Ignoring invalid Backbeat course tag %s=%s on %s", Logger::Severity::Warning, key, value, url);
				}
				continue;
			}

			const CourseTagSetting* match = nullptr;
			for (const CourseTagSetting& candidate : supported)
			{
				if (key == candidate.key)
				{
					match = &candidate;
					break;
				}
			}
			if (!match)
				continue;

			nlohmann::json parsed = nlohmann::json::parse(value, nullptr, false);
			bool valid = (match->type == CourseTagType::Boolean && parsed.is_boolean())
				|| (match->type == CourseTagType::Integer && parsed.is_number_integer())
				|| (match->type == CourseTagType::Float && parsed.is_number());
			if (!valid)
			{
				Logf("Ignoring invalid Backbeat course tag %s=%s on %s", Logger::Severity::Warning, key, value, url);
				continue;
			}
			if (match->type == CourseTagType::Float)
				parsed = parsed.get<double>();
			settings[match->setting] = std::move(parsed);
		}
		return settings;
	}
}

struct BackbeatCatalog::Impl
{
	Ref<BackbeatStore> store = GetBackbeatStore();
	std::mutex catalogMutex;
	bool initialSyncComplete = false;
};

Ref<BackbeatCatalog> GetBackbeatCatalog()
{
	static Ref<BackbeatCatalog> catalog = std::make_shared<BackbeatCatalog>();
	return catalog;
}

BackbeatCatalog::BackbeatCatalog()
	: m_impl(std::make_unique<Impl>())
{
}

BackbeatCatalog::~BackbeatCatalog() = default;

bool BackbeatCatalog::IsOpen() const
{
	return m_impl->store->IsOpen();
}

bool BackbeatCatalog::Prepare()
{
	std::lock_guard<std::mutex> lock(m_impl->catalogMutex);
	if (m_impl->initialSyncComplete)
		return true;
	m_impl->initialSyncComplete = m_PullCatalog();
	return m_impl->initialSyncComplete;
}

bool BackbeatCatalog::PullCatalog()
{
	std::lock_guard<std::mutex> lock(m_impl->catalogMutex);
	bool result = m_PullCatalog();
	if (result)
		m_impl->initialSyncComplete = true;
	return result;
}

bool BackbeatCatalog::m_PullCatalog()
{
	bkb_store* store = m_impl->store->GetNativeHandle();
	if (!store)
		return false;

	bkb_string attachValue{};
	if (LogBackbeatError("database attach", bkb_store_sqlite_attach_command(store, &attachValue)))
		return false;
	String attach = TakeString(attachValue);

	sqlite3* database = nullptr;
	String databasePath = Path::Absolute("maps.db");
	if (sqlite3_open_v2(databasePath.c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, nullptr) != SQLITE_OK)
	{
		Logf("Failed to open the USC map catalog: %s", Logger::Severity::Warning, database ? sqlite3_errmsg(database) : "unknown error");
		if (database) sqlite3_close(database);
		return false;
	}

	char* error = nullptr;
	if (sqlite3_exec(database, attach.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
	{
		Logf("Failed to attach the Backbeat catalog: %s", Logger::Severity::Warning, error ? error : sqlite3_errmsg(database));
		sqlite3_free(error);
		sqlite3_close(database);
		return false;
	}
	if (sqlite3_exec(database, "BEGIN", nullptr, nullptr, &error) != SQLITE_OK)
	{
		Logf("Failed to begin Backbeat catalog sync: %s", Logger::Severity::Warning, error ? error : sqlite3_errmsg(database));
		sqlite3_free(error);
		sqlite3_close(database);
		return false;
	}

	auto fail = [&](const char* operation)
	{
		Logf("Failed to %s: %s", Logger::Severity::Warning, operation, sqlite3_errmsg(database));
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		sqlite3_close(database);
		return false;
	};

	if (sqlite3_exec(database, "DELETE FROM main.TableSectionCharts", nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("clear table charts");
	if (sqlite3_exec(database, "DELETE FROM main.TableSections", nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("clear table sections");
	if (sqlite3_exec(database, "DELETE FROM main.Tables", nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("clear tables");
	if (sqlite3_exec(database, "DELETE FROM main.PackCharts", nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("clear pack charts");

	const char* staleCharts =
		"SELECT rowid FROM main.Charts WHERE backbeat_bundle_id IS NOT NULL "
		"AND NOT EXISTS (SELECT 1 FROM backbeat.bundle b WHERE b.id=backbeat_bundle_id AND b.extension='ksh')";
	String removePracticeSetups = "DELETE FROM main.PracticeSetups WHERE chart_id IN (" + String(staleCharts) + ")";
	if (sqlite3_exec(database, removePracticeSetups.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("remove stale Backbeat practice setups");
	String removeCollections =
		"DELETE FROM main.Collections WHERE folderid IN (SELECT folderid FROM main.Charts WHERE rowid IN (" + String(staleCharts) + "))";
	if (sqlite3_exec(database, removeCollections.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("remove stale Backbeat collections");
	const char* removeCharts =
		"DELETE FROM main.Charts WHERE backbeat_bundle_id IS NOT NULL "
		"AND NOT EXISTS (SELECT 1 FROM backbeat.bundle b WHERE b.id=backbeat_bundle_id AND b.extension='ksh')";
	if (sqlite3_exec(database, removeCharts, nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("remove stale Backbeat charts");
	const char* removeFolders =
		"DELETE FROM main.Folders WHERE path LIKE 'backbeat://bundle/%' "
		"AND rowid NOT IN (SELECT folderid FROM main.Charts)";
	if (sqlite3_exec(database, removeFolders, nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("remove stale Backbeat folders");

	const char* sql =
		"SELECT b.id,b.filename,cd.gzip_data,cd.uncompressed_size "
		"FROM backbeat.bundle b "
		"JOIN backbeat.chart_data cd ON cd.sha256=b.chart_sha256 "
		"LEFT JOIN main.Charts c ON c.backbeat_bundle_id=b.id "
		"WHERE b.extension='ksh' AND c.rowid IS NULL";
	sqlite3_stmt* statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
		return fail("query the Backbeat catalog");

	sqlite3_stmt* addFolder = nullptr;
	if (sqlite3_prepare_v2(database, "INSERT INTO main.Folders(path) VALUES(?)", -1, &addFolder, nullptr) != SQLITE_OK)
	{
		sqlite3_finalize(statement);
		return fail("prepare Backbeat folder insert");
	}
	const char* addChartSql =
		"INSERT INTO main.Charts(folderid,title,artist,title_translit,artist_translit,jacket_path,effector,illustrator,"
		"diff_name,diff_shortname,path,bpm,diff_index,level,preview_offset,preview_length,lwt,hash,preview_file,custom_offset,backbeat_bundle_id) "
		"VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
	sqlite3_stmt* addChart = nullptr;
	if (sqlite3_prepare_v2(database, addChartSql, -1, &addChart, nullptr) != SQLITE_OK)
	{
		sqlite3_finalize(addFolder);
		sqlite3_finalize(statement);
		return fail("prepare Backbeat chart insert");
	}

	const String diffNames[4] = { "Novice", "Advanced", "Exhaust", "Infinite" };
	const String diffShortNames[4] = { "NOV", "ADV", "EXH", "INF" };
	int stepResult;
	while ((stepResult = sqlite3_step(statement)) == SQLITE_ROW)
	{
		String bundleId((const char*)sqlite3_column_text(statement, 0));
		String filename((const char*)sqlite3_column_text(statement, 1));
		const void* compressed = sqlite3_column_blob(statement, 2);
		int compressedSize = sqlite3_column_bytes(statement, 2);
		int64 uncompressedSize = sqlite3_column_int64(statement, 3);
		if (compressedSize < 0 || uncompressedSize <= 0 || uncompressedSize > 1024ll * 1024ll * 1024ll)
			continue;

		Buffer chartBytes;
		if (!InflateChart(compressed, (size_t)compressedSize, (size_t)uncompressedSize, chartBytes))
		{
			Logf("Skipping unreadable Backbeat KSH bundle %s", Logger::Severity::Warning, bundleId);
			continue;
		}
		MemoryReader reader(chartBytes);
		Beatmap beatmap;
		if (!beatmap.Load(reader, true))
		{
			Logf("Skipping malformed Backbeat KSH bundle %s", Logger::Severity::Warning, bundleId);
			continue;
		}

		const BeatmapSettings& settings = beatmap.GetMapSettings();
		String folderPath = "backbeat://bundle/" + bundleId;
		String chartPath = folderPath + "/" + filename;
		int diffIndex = Math::Clamp<int32>(settings.difficulty, 0, 3);

		BindText(addFolder, 1, folderPath);
		if (sqlite3_step(addFolder) != SQLITE_DONE)
		{
			sqlite3_finalize(addChart);
			sqlite3_finalize(addFolder);
			sqlite3_finalize(statement);
			return fail("insert Backbeat folder");
		}
		sqlite3_reset(addFolder);
		sqlite3_clear_bindings(addFolder);
		sqlite3_int64 folderId = sqlite3_last_insert_rowid(database);

		sqlite3_bind_int64(addChart, 1, folderId);
		BindText(addChart, 2, settings.title);
		BindText(addChart, 3, settings.artist);
		BindText(addChart, 4, "");
		BindText(addChart, 5, "");
		BindText(addChart, 6, settings.jacketPath);
		BindText(addChart, 7, settings.effector);
		BindText(addChart, 8, settings.illustrator);
		BindText(addChart, 9, diffNames[diffIndex]);
		BindText(addChart, 10, diffShortNames[diffIndex]);
		BindText(addChart, 11, chartPath);
		BindText(addChart, 12, settings.bpm);
		sqlite3_bind_int(addChart, 13, diffIndex);
		sqlite3_bind_int(addChart, 14, settings.level);
		sqlite3_bind_int(addChart, 15, settings.previewOffset);
		sqlite3_bind_int(addChart, 16, settings.previewDuration);
		sqlite3_bind_int64(addChart, 17, 0);
		BindText(addChart, 18, Sha1(chartBytes.data(), chartBytes.size()));
		BindText(addChart, 19, settings.audioNoFX);
		sqlite3_bind_int(addChart, 20, 0);
		BindText(addChart, 21, bundleId);
		if (sqlite3_step(addChart) != SQLITE_DONE)
		{
			sqlite3_finalize(addChart);
			sqlite3_finalize(addFolder);
			sqlite3_finalize(statement);
			return fail("insert Backbeat chart");
		}
		sqlite3_reset(addChart);
		sqlite3_clear_bindings(addChart);
	}

	if (stepResult != SQLITE_DONE)
	{
		sqlite3_finalize(addChart);
		sqlite3_finalize(addFolder);
		sqlite3_finalize(statement);
		return fail("scan the Backbeat catalog");
	}

	sqlite3_finalize(addChart);
	sqlite3_finalize(addFolder);
	sqlite3_finalize(statement);

	CollectionListOwner tableList;
	const char* gamemodes[] = { "kshoot" };
	bkb_error_code tableListCode = bkb_store_list_tables(store, gamemodes, 1, &tableList.list);
	if (LogBackbeatError("table list", tableListCode))
	{
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		sqlite3_close(database);
		return false;
	}

	sqlite3_stmt* addTable = nullptr;
	sqlite3_stmt* addSection = nullptr;
	sqlite3_stmt* addSectionChart = nullptr;
	if (sqlite3_prepare_v2(database,
		"INSERT INTO main.Tables(url,name,symbol) VALUES(?,?,?)",
		-1, &addTable, nullptr) != SQLITE_OK)
		return fail("prepare table insert");
	if (sqlite3_prepare_v2(database,
		"INSERT INTO main.TableSections(table_url,section_index,kind,name) VALUES(?,?,?,?)",
		-1, &addSection, nullptr) != SQLITE_OK)
	{
		sqlite3_finalize(addTable);
		return fail("prepare table section insert");
	}
	if (sqlite3_prepare_v2(database,
		"INSERT INTO main.TableSectionCharts(table_url,section_index,kind,chart_index,chart_id) "
		"SELECT ?,?,?,?,rowid FROM main.Charts WHERE backbeat_bundle_id=?",
		-1, &addSectionChart, nullptr) != SQLITE_OK)
	{
		sqlite3_finalize(addSection);
		sqlite3_finalize(addTable);
		return fail("prepare table chart insert");
	}

	for (size_t tableIndex = 0; tableIndex < tableList.list->items_len; tableIndex++)
	{
		String url = CopyString(tableList.list->items[tableIndex].url);
		TableOwner table;
		bkb_error_code tableCode = bkb_store_get_table(store, url.c_str(), &table.table);
		if (LogBackbeatError("table load", tableCode))
		{
			sqlite3_finalize(addSectionChart);
			sqlite3_finalize(addSection);
			sqlite3_finalize(addTable);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			sqlite3_close(database);
			return false;
		}

		BindText(addTable, 1, url);
		BindText(addTable, 2, CopyString(table.table->name));
		BindText(addTable, 3, CopyString(table.table->symbol));
		if (sqlite3_step(addTable) != SQLITE_DONE)
		{
			sqlite3_finalize(addSectionChart);
			sqlite3_finalize(addSection);
			sqlite3_finalize(addTable);
			return fail("insert table");
		}
		sqlite3_reset(addTable);
		sqlite3_clear_bindings(addTable);

		auto insertSection = [&](int kind, size_t sectionIndex, bkb_str name,
			const bkb_table_chart* charts, size_t chartsLen)
		{
			BindText(addSection, 1, url);
			sqlite3_bind_int(addSection, 2, (int)sectionIndex);
			sqlite3_bind_int(addSection, 3, kind);
			BindText(addSection, 4, CopyString(name));
			if (sqlite3_step(addSection) != SQLITE_DONE)
				return false;
			sqlite3_reset(addSection);
			sqlite3_clear_bindings(addSection);

			for (size_t chartIndex = 0; chartIndex < chartsLen; chartIndex++)
			{
				if (!charts[chartIndex].bundle_id.ptr)
					continue;
				BindText(addSectionChart, 1, url);
				sqlite3_bind_int(addSectionChart, 2, (int)sectionIndex);
				sqlite3_bind_int(addSectionChart, 3, kind);
				sqlite3_bind_int(addSectionChart, 4, (int)chartIndex);
				BindText(addSectionChart, 5, CopyString(charts[chartIndex].bundle_id));
				if (sqlite3_step(addSectionChart) != SQLITE_DONE)
					return false;
				sqlite3_reset(addSectionChart);
				sqlite3_clear_bindings(addSectionChart);
			}
			return true;
		};

		for (size_t levelIndex = 0; levelIndex < table.table->levels_len; levelIndex++)
		{
			const bkb_table_level& level = table.table->levels[levelIndex];
			if (!insertSection(0, levelIndex, level.level, level.charts, level.charts_len))
			{
				sqlite3_finalize(addSectionChart);
				sqlite3_finalize(addSection);
				sqlite3_finalize(addTable);
				return fail("insert table level");
			}
		}
		for (size_t folderIndex = 0; folderIndex < table.table->folders_len; folderIndex++)
		{
			const bkb_table_folder& folder = table.table->folders[folderIndex];
			if (!insertSection(1, folderIndex, folder.name, folder.charts, folder.charts_len))
			{
				sqlite3_finalize(addSectionChart);
				sqlite3_finalize(addSection);
				sqlite3_finalize(addTable);
				return fail("insert table folder");
			}
		}
	}

	sqlite3_finalize(addSectionChart);
	sqlite3_finalize(addSection);
	sqlite3_finalize(addTable);

	const char* addPackCharts =
		"INSERT INTO main.PackCharts(pack_url,pack_name,chart_index,chart_id) "
		"SELECT p.url,p.name,e.entry,c.rowid FROM backbeat.pack p "
		"JOIN backbeat.pack_entry e ON e.url=p.url "
		"JOIN main.Charts c ON c.backbeat_bundle_id=e.bundle_id "
		"WHERE p.gamemode='kshoot'";
	if (sqlite3_exec(database, addPackCharts, nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("insert pack charts");

	CollectionListOwner courseList;
	bkb_error_code courseListCode = bkb_store_list_courses(store, gamemodes, 1, &courseList.list);
	if (LogBackbeatError("course list", courseListCode))
	{
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		sqlite3_close(database);
		return false;
	}

	std::unordered_map<std::string, String> chartHashes;
	sqlite3_stmt* chartHashScan = nullptr;
	if (sqlite3_prepare_v2(database,
		"SELECT backbeat_bundle_id,hash FROM main.Charts WHERE backbeat_bundle_id IS NOT NULL",
		-1, &chartHashScan, nullptr) != SQLITE_OK)
		return fail("prepare course chart lookup");
	int hashStep;
	while ((hashStep = sqlite3_step(chartHashScan)) == SQLITE_ROW)
	{
		String bundleId((const char*)sqlite3_column_text(chartHashScan, 0));
		String hash((const char*)sqlite3_column_text(chartHashScan, 1));
		chartHashes.emplace(std::move(bundleId), std::move(hash));
	}
	sqlite3_finalize(chartHashScan);
	if (hashStep != SQLITE_DONE)
		return fail("load course chart lookup");

	sqlite3_stmt* upsertCourse = nullptr;
	if (sqlite3_exec(database,
		"UPDATE main.Challenges SET settings=NULL WHERE source_key LIKE 'backbeat:course:%'",
		nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("mark courses as stale");
	const char* upsertCourseSql =
		"INSERT INTO main.Challenges("
		"title,charts,chart_meta,clear_mark,best_score,req_text,path,hash,level,lwt,source_key,settings) "
		"VALUES(?,?,?,0,0,'',?,?,?,?,?,?) "
		"ON CONFLICT(source_key) DO UPDATE SET "
		"title=excluded.title,charts=excluded.charts,chart_meta=excluded.chart_meta,"
		"req_text=excluded.req_text,path=excluded.path,hash=excluded.hash,level=excluded.level,"
		"lwt=excluded.lwt,settings=excluded.settings";
	if (sqlite3_prepare_v2(database, upsertCourseSql, -1, &upsertCourse, nullptr) != SQLITE_OK)
		return fail("prepare course upsert");

	for (size_t courseIndex = 0; courseIndex < courseList.list->items_len; courseIndex++)
	{
		String url = CopyString(courseList.list->items[courseIndex].url);
		CourseOwner course;
		bkb_error_code courseCode = bkb_store_get_course(store, url.c_str(), &course.course);
		if (LogBackbeatError("course load", courseCode))
		{
			sqlite3_finalize(upsertCourse);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			sqlite3_close(database);
			return false;
		}
		if (course.course->charts_len == 0)
		{
			Logf("Skipping empty Backbeat course %s", Logger::Severity::Warning, url);
			continue;
		}

		String sourceKey = "backbeat:course:" + url;
		constexpr int32 level = 0;
		nlohmann::json global = CourseSettings(*course.course, url);
		nlohmann::json settings = {
			{ "title", CopyString(course.course->name) },
			{ "level", level },
			{ "charts", nlohmann::json::array() },
		};
		if (!global.empty())
			settings["global"] = std::move(global);

		String chartMeta;
		for (size_t chartIndex = 0; chartIndex < course.course->charts_len; chartIndex++)
		{
			const bkb_course_chart& chart = course.course->charts[chartIndex];
			String chartReference = CopyString(chart.id);
			if (chart.bundle_id.ptr)
			{
				auto hash = chartHashes.find(CopyString(chart.bundle_id));
				if (hash != chartHashes.end())
					chartReference = hash->second;
			}
			settings["charts"].push_back(std::move(chartReference));
			String description = CopyString(chart.desc);
			if (!description.empty())
			{
				if (!chartMeta.empty())
					chartMeta += " ";
				chartMeta += description;
			}
		}

		String name = CopyString(course.course->name);
		String charts = settings["charts"].dump();
		String storedSettings = settings.dump();
		String path = "backbeat://course/" + url;
		BindText(upsertCourse, 1, name);
		BindText(upsertCourse, 2, charts);
		BindText(upsertCourse, 3, chartMeta);
		BindText(upsertCourse, 4, path);
		BindText(upsertCourse, 5, url);
		sqlite3_bind_int(upsertCourse, 6, level);
		sqlite3_bind_int64(upsertCourse, 7, course.course->updated.seconds);
		BindText(upsertCourse, 8, sourceKey);
		BindText(upsertCourse, 9, storedSettings);
		if (sqlite3_step(upsertCourse) != SQLITE_DONE)
		{
			sqlite3_finalize(upsertCourse);
			return fail("upsert course");
		}
		sqlite3_reset(upsertCourse);
		sqlite3_clear_bindings(upsertCourse);
	}

	sqlite3_finalize(upsertCourse);
	if (sqlite3_exec(database,
		"DELETE FROM main.Challenges WHERE source_key LIKE 'backbeat:course:%' "
		"AND settings IS NULL",
		nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("remove stale courses");
	if (sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
		return fail("commit Backbeat catalog sync");
	sqlite3_close(database);
	return true;
}
