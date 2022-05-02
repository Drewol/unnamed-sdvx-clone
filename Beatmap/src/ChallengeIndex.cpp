#include "stdafx.h"
#include "MapDatabase.hpp"
#include "json.hpp"

nlohmann::json ChallengeIndex::LoadJson(const Buffer& jsonBuf, const String& path)
{
	String jsonData((char*)jsonBuf.data(), jsonBuf.size());
	Logf("JSON loaded: %s", Logger::Severity::Debug, *jsonData);

	try
	{
		return nlohmann::json::parse(*jsonData);
	}
	catch (const std::exception& e)
	{
		Logf("Encountered JSON error with %s: %s", Logger::Severity::Warning, path, e.what());
	}
	return nlohmann::json();
}

nlohmann::json ChallengeIndex::LoadJson(const String& path)
{
	File chalFile;
	if (!chalFile.OpenRead(path))
		return false;

	Buffer jsonBuf;
	jsonBuf.resize(chalFile.GetSize());
	chalFile.Read(jsonBuf.data(), jsonBuf.size());
	return ChallengeIndex::LoadJson(jsonBuf, path);
}

int32 jsonIntValue(nlohmann::json j, const String& k, int32 def)
{
	if (j[k].is_string())
	{
	}
	return j.value(k, def);
}

// Each of these has a string key and then a pair of strings to use
// The first is for global reqs and the second is for overriden reqs
const Map<String, std::pair<String, String>> ChallengeIndex::ChallengeDescriptionStrings = {
	// Overrideable reqs
	{"clear", {
		"Clear all charts",
		"Clear this chart"
	}},
	{"excessive clear", {
		"Clear all charts on Excessive Rate",
		"Clear this chart on Excessive Rate"
	}},
	{"permissive clear", {
		"Clear all charts on Permissive Rate",
		"Clear this chart on Permissive Rate"
	}},
	{"blastive clear", {
		"Clear all charts on Blastive %.1f* Rate",
		"Clear this chart on Blastive %.1f* Rate"
	}},
	{"permissive clear", {
		"Play on Permissive Rate",
		""
	}},
	{"min_percentage", {
		"Get %d%% completion on each chart",
		"Get %d%% completion"
	}},
	{"min_gauge",{
		"Get %d%% gauge on each chart",
		"Get %d%% gauge"
	}},
	{"max_errors",{
		"Get less than %d errors per chart",
		"Get less than %d errors"
	}},
	{"max_nears",{
		"Get less than %d nears per chart",
		"Get less than %d nears"
	}},
	{"min_crits",{
		"Get at least %d crits per chart",
		"Get at least %d crits"
	}},
	{"min_chain",{
		"Get a chain at least %d long each chart",
		"Get a chain at least %d long"
	}},

	// Global reqs
	{"min_average_percentage", {
		"Get %d%% completion overall", ""
	}},
	{"min_average_gauge", {
		"Get %d%% gauge overall", ""
	}},
	{"max_overall_errors", {
		"Get less than %d errors total", ""
	}},
	{"max_overall_nears", {
		"Get less than %d nears total", ""
	}},
	{"min_overall_crits", {
		"Get at least %d crits total", ""
	}},
	{"gauge_carry_over", {
		"Gauge does not reset", ""
	}},
};

// Generate a description for a challenge requirement
// j is the json object to read req from
// keyName is the name of the json key to read
// stringName is the name of the strings to use (see ChallengeDescriptionStrings)
// isOverride chooses which string to use (either global or override)
// mult is a value multiplier
// add is a value addition
String ChallengeIndex::ChallengeDescriptionVal(
	const nlohmann::json& j,
	const String& keyName,
	const String& stringName,
	bool isOverride,
	int mult,
	int add)
{
	// This function requires a %d or %u formater, so just sanity check in debug
	assert(ChallengeDescriptionStrings.Find(stringName));
	assert(ChallengeDescriptionStrings.Find(stringName)->first.find("%d")!=String::npos
		|| ChallengeDescriptionStrings.Find(stringName)->first.find("%u")!=String::npos);
	assert(!isOverride || ChallengeDescriptionStrings.Find(stringName)->first.find("%d")!=String::npos
		|| ChallengeDescriptionStrings.Find(stringName)->first.find("%u")!=String::npos);

	if (!j.contains(keyName))
		return "";

	const auto* str = ChallengeDescriptionStrings.Find(stringName);

	const auto& k = j[keyName];
	int32 value = 0;
	if (isOverride && k.is_null())
	{
		String name = keyName;
		// TODO(itszn) better way to insert name into string
		std::replace(name.begin(), name.end(), '_', ' ');
		return "  > Ignore " + name + " requirement for this chart\n";
	}

	if (k.is_number_float())
		value = static_cast<uint32>(j.value(keyName, 0.0f) * mult + add);
	else if (k.is_number_integer())
		value = j.value(keyName, 0) * mult + add;
	else if (k.is_string())
	{
		String str;
		k.get_to(str);
		value = atoi(*str) * mult + add;
	}
	else
	{
		// Unknown type, skip
		return "";
	}
	return (isOverride? "  - ":"- ") + Utility::Sprintf(*(isOverride ? str->second : str->first), value) + "\n";
}

// Use this if the keyName is the same as the stringName
String ChallengeIndex::ChallengeDescriptionVal(
	const nlohmann::json& j,
	const String& keyName,
	bool isOverride,
	int mult,
	int add)
{
	return ChallengeIndex::ChallengeDescriptionVal(j, keyName, keyName, isOverride, mult, add);
}

void ChallengeIndex::GenerateDescription()
{
	String desc = "";
	if (!settings.contains("global"))
	{
		reqText = "No requirements";
		return;
	}

	const auto& j = settings["global"];
	if (j.value("clear", false))
	{
		if (j.value("excessive_gauge", false))
			desc += "- " + ChallengeDescriptionStrings.Find("excessive clear")->first;
		else if (j.value("permissive_gauge", false))
			desc += "- " + ChallengeDescriptionStrings.Find("permissive clear")->first;
		else if (j.value("blastive_gauge", false))
		{
			float level = j.value("gauge_level", 0.5f);
			desc += "- " + Utility::Sprintf(*ChallengeDescriptionStrings.Find("blastive clear")->first, level);
		}
		else
			desc += "- " + ChallengeDescriptionStrings.Find("clear")->first;

		if (j.value("gauge_carry_over", false))
			desc += " (" + ChallengeDescriptionStrings.Find("gauge_carry_over")->first + ")";

		desc += "\n";
	}

	desc += ChallengeDescriptionVal(j, String("min_percentage"),         false);
	desc += ChallengeDescriptionVal(j, String("min_average_percentage"), false);
	desc += ChallengeDescriptionVal(j, String("min_gauge"),              false, /*mult=*/100);
	desc += ChallengeDescriptionVal(j, String("min_average_gauge"),      false, /*mult=*/100);
	desc += ChallengeDescriptionVal(j, String("max_errors"),             false, /*mult=*/1,              /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("max_overall_errors"),     false, /*mult=*/1,              /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("max_average_errors"),     // Reuse overall string ^
									       String("max_overall_errors"), false, /*mult=*/totalNumCharts, /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("max_nears"),              false, /*mult=*/1,              /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("max_overall_nears"),      false, /*mult=*/1,              /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("max_average_nears"),      // Reuse overall string ^
	                                       String("max_overall_nears"),  false, /*mult=*/totalNumCharts, /*add=*/1);
	desc += ChallengeDescriptionVal(j, String("min_crits"),              false);
	desc += ChallengeDescriptionVal(j, String("min_overall_crits"),      false);
	desc += ChallengeDescriptionVal(j, String("min_average_crits"),      // Reuse overall string ^
	                                       String("min_overall_crits"),  false, /*mult=*/totalNumCharts, /*add=*/0);
	desc += ChallengeDescriptionVal(j, String("min_chain"),              false);

	// If no overrides don't do anything
	if (!settings.contains("overrides"))
	{
		this->reqText = desc;
		return;
	}

	desc += "\n";

	const auto& o = settings["overrides"];
	unsigned int maxNum = static_cast<unsigned int>(std::min(std::min((size_t)totalNumCharts, charts.size()), o.size()));

	for (unsigned int i = 0; i < maxNum; i++)
	{
		String overdesc = "";
		const auto& j = o[i];

		// Special case for clear overriding global clear/excessive
		if (j.contains("clear") || j.contains("excessive_gauge") ||
			j.contains("blastive_gauge") || j.contains("permissive_gauge"))
		{
			if (j.contains("clear") && !j.value("clear", false))
			{
				// If we are disabling the global clear req
				if (settings["global"].value("clear", false))
					overdesc += "  > Ignore clear requirement for this chart\n";
			}
			else
			{
				bool isHard = j.value("excessive_gauge", false) || settings["global"].value("excessive_gauge", false);
				bool isPermissive = j.value("permissive_gauge", false) || settings["global"].value("permissive_gauge", false);
				bool isBlastive = j.value("blastive_gauge", false) || settings["global"].value("blastive_gauge", false);
				if (isBlastive)
				{
					float level = 0.5;
					if (j.contains("gauge_level"))
						level = j.value("gauge_level", 0.5f);
					else
						level = settings["global"].value("gauge_level", 0.5f);

				}
				else if (isPermissive)
					overdesc += "  - " + ChallengeDescriptionStrings.Find("permissive clear")->second + "\n";
				else if (isHard)
					overdesc += "  - " + ChallengeDescriptionStrings.Find("excessive clear")->second + "\n";
				else
					overdesc += "  - " + ChallengeDescriptionStrings.Find("clear")->second + "\n";
			}
		}

		overdesc += ChallengeDescriptionVal(j, String("min_percentage"), true);
		overdesc += ChallengeDescriptionVal(j, String("min_gauge"),      true, /*mult=*/100);
		overdesc += ChallengeDescriptionVal(j, String("max_errors"),     true, /*mult=*/1, /*add=*/1);
		overdesc += ChallengeDescriptionVal(j, String("max_nears"),      true, /*mult=*/1, /*add=*/1);
		overdesc += ChallengeDescriptionVal(j, String("min_crits"),      true);
		overdesc += ChallengeDescriptionVal(j, String("min_chain"),      true);

		if (!overdesc.empty())
			desc += charts[i]->title + ":\n" + overdesc;
	}

	if (desc.empty())
		desc = "No requirements";

	this->reqText = desc;
}

void ChallengeIndex::FindCharts(MapDatabase* db, const nlohmann::json& chartsToFind)
{
	totalNumCharts = 0;

	if (chartsToFind.is_discarded() || !chartsToFind.is_array())
	{
		Logf("Unable to understand charts for challenge %s", Logger::Severity::Warning, path);
		return;
	}

	totalNumCharts = static_cast<int32>(chartsToFind.size());
	for (auto& el : chartsToFind.items())
	{
		ChartIndex* chart = nullptr;
		if (el.value().is_string())
		{
			String val;
			String kshMatch = ".ksh";
			el.value().get_to(val);

			// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c/876704#876704
			if (std::mismatch(kshMatch.rbegin(), kshMatch.rend(), val.rbegin()).first == kshMatch.rend())
			{
				// Look up as path
				chart = db->FindFirstChartByPath(val);
				if (chart == nullptr)
					Logf("Could not find chart by path %s for challenge %s", Logger::Severity::Warning, *val, *path);
			}
			else
			{
				chart = db->FindFirstChartByHash(val);
				if (chart == nullptr)
					Logf("Could not find chart by *hash* %s for challenge %s. If you are using a path, make sure it ends with `.ksh`", Logger::Severity::Warning, *val, *path);
			}
		}
		else if (el.value().is_object())
		{
			const auto& o = el.value();
			if (!o.contains("name") || !o["name"].is_string() || !o.contains("level") || !o["level"].is_number_integer())
			{
				Logf("Found invalid name+level `%s` for challenge %s in database", Logger::Severity::Warning, o.dump().c_str(), *path);
				missingChart = true;
				continue;
			}
			String name;
			o["name"].get_to(name);
			int32 level = o.value("level", 0);
			chart = db->FindFirstChartByNameAndLevel(name, level);
			if (chart == nullptr)
				Logf("Could not find chart %s for challenge %s", Logger::Severity::Warning, o.dump().c_str(), *path);
		}
		else
		{
			if (!el.value().is_discarded())
				Logf("Found non string/object chart entry `%s` for challenge %s in database", Logger::Severity::Warning, el.value().dump().c_str(), *path);
			else
				Logf("Found invalid json chart entry for challenge %s in database", Logger::Severity::Warning, *path);
			missingChart = true;
			continue;
		}

		if (chart)
			charts.push_back(chart);
		else
			missingChart = true;
	}

}

bool ChallengeIndex::BasicValidate(const nlohmann::json& settings, const String& path)
{
	if (settings.is_discarded() || !settings.is_object())
		return false;

	if (!settings.contains("title") || !settings["title"].is_string())
	{
		Logf("Encountered error loading challenge %s: missing or invalid title", Logger::Severity::Warning, *path);
		return false;
	}
	if (!settings.contains("level") || !settings["level"].is_number_integer())
	{
		Logf("Encountered error loading challenge %s: missing or invalid level", Logger::Severity::Warning, *path);
		return false;
	}

	if (!settings.contains("charts") || !settings["charts"].is_array())
	{
		Logf("Encountered error loading challenge %s: missing or invalid chart array", Logger::Severity::Warning, *path);
		return false;
	}
	if (settings["charts"].size() == 0)
	{
		Logf("Encountered error loading challenge %s: Must have at least one chart", Logger::Severity::Warning, *path);
		return false;
	}
	for (auto& el : settings["charts"].items())
	{
		if (el.value().is_string()) {}
		else if (el.value().is_object())
		{
			const auto& v = el.value();
			if (!v.contains("name") || !v["name"].is_string())
			{
				Logf("Encountered error loading challenge %s: Chart entry `%s`: \"name\" must be a string", Logger::Severity::Warning, *path, v.dump().c_str());
				return false;
			}
			if (!v.contains("level") || !v["level"].is_number_integer())
			{
				Logf("Encountered error loading challenge %s: Chart entry `%s`: \"level\" must be an integer", Logger::Severity::Warning, *path, v.dump().c_str());
				return false;
			}
		}
		else
		{
			Logf("Encountered error loading challenge %s: Chart entry `%s`: must be string for hash/path or object for name+level", Logger::Severity::Warning, *path, el.value().dump().c_str());
			return false;
		}
	}

	if (settings.contains("global") && !settings["global"].is_object())
	{
		Logf("Encountered error loading challenge %s: global must be an object", Logger::Severity::Warning, *path);
		return false;
	}

	if (settings.contains("overrides"))
	{
		if (!settings["overrides"].is_array())
		{
			Logf("Encountered error loading challenge %s: overrides must be an array", Logger::Severity::Warning, *path);
			return false;
		}
		for (auto& el : settings["overrides"].items())
		{
			if (!el.value().is_object())
			{
				Logf("Encountered error loading challenge %s: Override entries must be objects", Logger::Severity::Warning, *path);
				return false;
			}
		}
	}
	return true;

}
