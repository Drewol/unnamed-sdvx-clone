#include <iterator>
#include <string_view>

/// Simple class for extracting an archive file to a specific path
class Extractor
{
public:
	Extractor() = default;
	void Extract(const std::string_view data);

private:
	struct archive* CreateRead(const std::string_view data);
	struct archive* CreateDiskWrite();

	static void CopyArchive(struct archive* src, struct archive* dst);
	static void CopyArchiveData(struct archive* src, struct archive* dst);
};