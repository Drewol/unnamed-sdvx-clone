#pragma once

#include <Backbeat/BackbeatStore.hpp>
#include <memory>

class BackbeatCatalog
{
public:
	BackbeatCatalog();
	~BackbeatCatalog();

	[[nodiscard]] bool IsOpen() const;
	[[nodiscard]] bool Prepare();
	[[nodiscard]] bool PullCatalog();

private:
	[[nodiscard]] bool m_PullCatalog();

	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

Ref<BackbeatCatalog> GetBackbeatCatalog();
