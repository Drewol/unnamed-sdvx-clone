#include <string>

/// Simple class for downloading a file using libcpr
class Downloader
{
public:
	Downloader() = default;
	void Download(const std::string& url);

	inline const std::string& GetContent() const { return m_content; }

private:
	class ProgressBar
	{
	public:
		void Start();
		void Update(size_t total, size_t current);
		void Finish();

	private:
		void Render(size_t total, size_t current);
		const size_t m_width = 50;
	};

	ProgressBar m_progressBar;

	std::string m_content;
};
