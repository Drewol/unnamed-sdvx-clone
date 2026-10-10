#pragma once

#include <Shared/ChartSource.hpp>
#include <memory>

struct bkb_store;

class BackbeatStore
{
public:
	BackbeatStore();
	~BackbeatStore();

	[[nodiscard]] bool IsOpen() const;
	[[nodiscard]] Resource LoadChart(const String& bundleId, const String& filename) const;
	[[nodiscard]] Resource ResolvePath(const String& bundleId, const String& relativePath) const;
	[[nodiscard]] bkb_store* GetNativeHandle() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

class BackbeatChartSource final : public ChartSource
{
public:
	BackbeatChartSource(Ref<BackbeatStore> store, String bundleId, const String& chartPath);

	[[nodiscard]] Resource LoadChart() const override;
	[[nodiscard]] Resource ResolvePath(const String& relativePath) const override;

private:
	Ref<BackbeatStore> m_store;
	String m_bundleId;
	String m_filename;
};

Ref<BackbeatStore> GetBackbeatStore();
