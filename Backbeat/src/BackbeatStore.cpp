#include <Backbeat/BackbeatStore.hpp>

#include <Shared/Log.hpp>
#include <backbeat.h>
#include <cstring>
#include <utility>

namespace
{
	struct BundleOwner
	{
		bkb_bb* bundle = nullptr;
		~BundleOwner() { if (bundle) bkb_bb_free(bundle); }
	};

	struct AssetOwner
	{
		bkb_asset_data asset{};
		~AssetOwner() { bkb_asset_data_free(asset); }
	};

	Ref<Buffer> CopyBytes(const uint8* data, size_t size)
	{
		if (!data && size != 0)
			return {};
		auto bytes = std::make_shared<Buffer>(size);
		if (size != 0)
			memcpy(bytes->data(), data, size);
		return bytes;
	}

	bool LogBackbeatError(const char* operation, bkb_error_code code)
	{
		if (code == BKB_OK)
			return false;
		Logf("Backbeat %s failed: %s", Logger::Severity::Warning, operation, bkb_error_string(code));
		return true;
	}
}

struct BackbeatStore::Impl
{
	bkb_store* store = nullptr;
};

Ref<BackbeatStore> GetBackbeatStore()
{
	static Ref<BackbeatStore> store = std::make_shared<BackbeatStore>();
	return store;
}

BackbeatStore::BackbeatStore()
	: m_impl(std::make_unique<Impl>())
{
	bkb_error_code code = bkb_store_open(&m_impl->store);
	if (LogBackbeatError("store open", code))
		m_impl->store = nullptr;
}

BackbeatStore::~BackbeatStore()
{
	if (m_impl->store)
		bkb_store_free(m_impl->store);
}

bool BackbeatStore::IsOpen() const
{
	return m_impl->store != nullptr;
}

bkb_store* BackbeatStore::GetNativeHandle() const
{
	return m_impl->store;
}

Resource BackbeatStore::LoadChart(const String& bundleId, const String& filename) const
{
	if (!m_impl->store)
		return {};

	BundleOwner owner;
	bkb_error_code code = bkb_store_get_bundle(m_impl->store, bundleId.c_str(), &owner.bundle);
	if (LogBackbeatError("bundle load", code))
		return {};
	auto bytes = CopyBytes(owner.bundle->chart, owner.bundle->chart_len);
	if (!bytes)
		return {};
	return Resource::FromBytes(filename, std::move(bytes));
}

Resource BackbeatStore::ResolvePath(const String& bundleId, const String& relativePath) const
{
	if (!m_impl->store || relativePath.empty())
		return {};

	AssetOwner asset;
	bkb_error_code code = bkb_store_resolve_path(
		m_impl->store,
		bundleId.c_str(),
		relativePath.c_str(),
		&asset.asset);
	if (code != BKB_OK)
		return {};
	if (asset.asset.kind == BKB_ASSET_DATA_FILE)
		return Resource::FromPath(String(asset.asset.value.file.ptr, asset.asset.value.file.len), relativePath);
	if (asset.asset.kind == BKB_ASSET_DATA_BYTES)
	{
		const bkb_bytes& value = asset.asset.value.bytes;
		auto bytes = CopyBytes(value.ptr, value.len);
		if (!bytes)
			return {};
		return Resource::FromBytes(relativePath, std::move(bytes));
	}
	return {};
}

BackbeatChartSource::BackbeatChartSource(Ref<BackbeatStore> store, String bundleId, const String& chartPath)
	: m_store(std::move(store)), m_bundleId(std::move(bundleId))
{
	Path::RemoveLast(chartPath, &m_filename);
}

Resource BackbeatChartSource::LoadChart() const
{
	return m_store->LoadChart(m_bundleId, m_filename);
}

Resource BackbeatChartSource::ResolvePath(const String& relativePath) const
{
	return m_store->ResolvePath(m_bundleId, relativePath);
}
