#pragma once

/*
	Compiled operation on local database object
*/
class DBStatement : public Unique
{
public:
	DBStatement(DBStatement&& other);
	~DBStatement();
	bool Step();
	bool StepRow();
	void Rewind();
	void Finish();
	[[nodiscard]]
	int32 IntColumn(int32 index = 0) const;
	[[nodiscard]]
	int64 Int64Column(int32 index = 0) const;
	[[nodiscard]]
	double DoubleColumn(int32 index = 0) const;
	[[nodiscard]]
	String StringColumn(int32 index = 0) const;
	[[nodiscard]]
	String StringColumnEmptyOnNull(int32 index = 0) const;
	[[nodiscard]]
	Buffer BlobColumn(int32 index = 0) const;
	void BindInt(int32 index, const int32& value);
	void BindInt64(int32 index, const int64& value);
	void BindDouble(int32 index, const double& value);
	void BindString(int32 index, const String& value);
	void BindBlob(int32 index, const Buffer& value);
	int32 ColumnCount() const;
	operator bool();

private:
	DBStatement(const String& statement, class Database* db);
	friend class Database;

	Database& m_db;
	struct sqlite3_stmt* m_stmt = nullptr;
	int32 m_compileResult;
	int32 m_queryResult;
};

/*
	Local database object
*/
class Database : public Unique
{
public:
	~Database();
	void Close();
	bool Open(const String& path);
	DBStatement Query(const String& queryString);
	bool Exec(const String& queryString);
	bool ExecDirect(const String& queryString);

	struct sqlite3* db = nullptr;
};
