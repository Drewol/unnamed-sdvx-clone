#include "stdafx.h"
#include "Database.hpp"
#include "sqlite3.h"

DBStatement::DBStatement(const String& statement, Database* db) : m_db(*db)
{
	m_queryResult = 0;
	m_compileResult = sqlite3_prepare(m_db.db, *statement, (int)statement.size() + 1, &m_stmt, nullptr);
	if (m_compileResult != SQLITE_OK)
	{
		Logf("Failed to compile statement:\n%s\n-> %s", Logger::Severity::Error, statement, sqlite3_errmsg(m_db.db));
	}
}
DBStatement::DBStatement(DBStatement&& other) : m_db(other.m_db)
{
	m_stmt = other.m_stmt;
	m_compileResult = other.m_compileResult;
	m_queryResult = other.m_queryResult;
	other.m_stmt = nullptr;
}
DBStatement::~DBStatement()
{
	Finish();
}
bool DBStatement::Step()
{
	assert(m_stmt);
	m_queryResult = sqlite3_step(m_stmt);
	bool res = m_queryResult >= SQLITE_ROW; // Row or Done
	if (m_queryResult < SQLITE_ROW)
	{
		Logf("Query Failed -> %s", Logger::Severity::Warning, sqlite3_errmsg(m_db.db));
	}
	return res;
}
bool DBStatement::StepRow()
{
	assert(m_stmt);
	m_queryResult = sqlite3_step(m_stmt);
	bool res = m_queryResult == SQLITE_ROW; // Row only
	if (m_queryResult < SQLITE_ROW)
	{
		Logf("Query Failed -> %s", Logger::Severity::Warning, sqlite3_errmsg(m_db.db));
	}
	return res;
}
void DBStatement::Rewind()
{
	sqlite3_reset(m_stmt);
}
void DBStatement::Finish()
{
	if (m_stmt)
	{
		sqlite3_finalize(m_stmt);
		m_stmt = nullptr;
	}
}
int32 DBStatement::IntColumn(int32 index) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	return sqlite3_column_int(m_stmt, index);
}
int64 DBStatement::Int64Column(int32 index /*= 0*/) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	return sqlite3_column_int64(m_stmt, index);
}
double DBStatement::DoubleColumn(int32 index) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	return sqlite3_column_double(m_stmt, index);
}
String DBStatement::StringColumn(int32 index /*= 0*/) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	return String((char*)sqlite3_column_text(m_stmt, index));
}
String DBStatement::StringColumnEmptyOnNull(int32 index /*= 0*/) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	if (sqlite3_column_type(m_stmt, index) == SQLITE_NULL) {
		return "";
	}
	return String((char*)sqlite3_column_text(m_stmt, index));
}
Buffer DBStatement::BlobColumn(int32 index /*= 0*/) const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	int32 blobLen = sqlite3_column_bytes(m_stmt, index);
	uint8* data = (uint8*)sqlite3_column_blob(m_stmt, index);
	return Buffer(data, data + blobLen);
}
void DBStatement::BindInt(int32 index, const int32& value)
{
	assert(m_stmt);
	sqlite3_bind_int(m_stmt, index, value);
}
void DBStatement::BindDouble(int32 index, const double& value)
{
	assert(m_stmt);
	sqlite3_bind_double(m_stmt, index, value);
}
void DBStatement::BindInt64(int32 index, const int64& value)
{
	assert(m_stmt);
	sqlite3_bind_int64(m_stmt, index, value);
}

static void FreeData(char* data)
{
	delete[] data;
}
void DBStatement::BindString(int32 index, const String& value)
{
	assert(m_stmt);
	char* copy = new char[value.size()];
	memcpy(copy, *value, value.size());
	sqlite3_bind_text(m_stmt, index, copy, (int32)value.size(), (void(*)(void*)) & FreeData);
}
void DBStatement::BindBlob(int32 index, const Buffer& value)
{
	assert(m_stmt);
	char* copy = new char[value.size()];
	memcpy(copy, value.data(), value.size());
	sqlite3_bind_blob(m_stmt, index, copy, (int32)value.size(), (void(*)(void*)) & FreeData);
}
int32 DBStatement::ColumnCount() const
{
	assert(m_stmt && m_queryResult == SQLITE_ROW);
	return sqlite3_column_count(m_stmt);
}
DBStatement::operator bool()
{
	return m_stmt != nullptr;
}

Database::~Database()
{
	Close();
}
void Database::Close()
{
	if (db)
	{
		sqlite3_close(db);
	}
	db = nullptr;
}
bool Database::Open(const String& path)
{
	Close();
	int32 r = sqlite3_open(*path, &db);
	if (r != 0)
	{
		return false;
	}
	return true;
}
DBStatement Database::Query(const String& queryString)
{
	DBStatement statement(queryString, this);
	return std::move(statement);
}
bool Database::Exec(const String& queryString)
{
	DBStatement stmt = Query(queryString);
	if (!stmt)
		return false;
	return stmt.Step();
}

bool Database::ExecDirect(const String& queryString)
{
	char* err;
	if (sqlite3_exec(db, *queryString, nullptr, nullptr, &err) != SQLITE_OK)
	{
		Logf("sqlite3_exec failed -> %s", Logger::Severity::Error, err);
		return false;
	}
	return true;
}
