#pragma once

/*
	Compiled operation on local database object
*/
class DBStatement : public NonCopyable
{
public:
	DBStatement(DBStatement&& other);
	~DBStatement();
	bool Step();
	bool StepRow();
	void Rewind();
	void Finish();
	int32 IntColumn(int32 index = 0) const;
	int64 Int64Column(int32 index = 0) const;
	double DoubleColumn(int32 index = 0) const;
	String StringColumn(int32 index = 0) const;
	String StringColumnEmptyOnNull(int32 index = 0) const;
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
class Database : public NonCopyable
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
