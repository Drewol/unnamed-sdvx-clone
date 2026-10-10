#pragma once

#include "Buffer.hpp"
#include "Ref.hpp"
#include "String.hpp"
#include <utility>

/**
 * Effectively, either a path to a file or an inline slice.
 *
 * Because backbeat inlines small files, we need to pass this around instead of file paths in
 * a lot of places.
 *
 * IsPath -> use the path, otherwise GetBytes.
 */
class Resource
{
public:
	static Resource FromPath(String path, String name = "")
	{
		Resource resource;
		resource.m_path = std::move(path);
		resource.m_name = name.empty() ? resource.m_path : std::move(name);
		return resource;
	}

	static Resource FromBytes(String name, Ref<Buffer> data)
	{
		Resource resource;
		resource.m_name = std::move(name);
		resource.m_data = std::move(data);
		return resource;
	}

	[[nodiscard]] bool IsValid() const { return !m_path.empty() || m_data != nullptr; }
	[[nodiscard]] bool IsPath() const { return !m_path.empty(); }
	[[nodiscard]] const String& GetPath() const { return m_path; }
	[[nodiscard]] const String& GetName() const { return m_name; }
	[[nodiscard]] const Ref<Buffer>& GetBytes() const { return m_data; }

private:
	String m_path;
	String m_name;
	Ref<Buffer> m_data;
};
