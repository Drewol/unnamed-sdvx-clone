#pragma once

class ShadedMesh {
public:
	ShadedMesh(const String& name);
	ShadedMesh(const String& name, const String& path);
	ShadedMesh();
	virtual ~ShadedMesh() = default; // Virtual so we can dynamiccast

	void Draw();
	void SetData(Vector<MeshGenerators::SimpleVertex>& data);
	void AddTexture(const String& name, const String& file);
	void AddSkinTexture(const String& name, const String& file);
	int AddSharedTexture(const String& name, const String& key);
	void SetBlendMode(const MaterialBlendMode& mode);
	void SetPrimitiveType(const PrimitiveType& type);
	void SetOpaque(bool opaque);

	void SetPos(float x, float y, float z) noexcept
	{
		m_pos = Vector3(x, y, z);
	}
	[[nodiscard]]
	Vector3& GetPos() noexcept { return m_pos; }
	void SetScale(float x, float y, float z) noexcept
	{
		m_scale = Vector3(x, y, z);
	}
	[[nodiscard]]
	Vector3& GetScale() noexcept { return m_scale; }
	void SetRotation(float x, float y, float z) noexcept
	{
		m_rotation = Vector3(x, y, z);
	}
	[[nodiscard]]
	Vector3& GetRotation() noexcept { return m_rotation; }
	[[nodiscard]]
	bool IsWireframe() const noexcept { return m_isWireframe; }
	void SetIsWireframe(bool b) noexcept { m_isWireframe = b; }

	template<typename T>
	void SetParam(const String& name, const T& value) {
		m_params.SetParameter(name, value);
	}

	static int lNew(struct lua_State* L);

	static Map<String, void*> FunctionMap;


protected:
	Mesh m_mesh;
	Material m_material;
	MaterialParameterSet m_params;
	Map<String, Texture> m_textures;
	Vector3 m_pos = Vector3(0.0,0.0,0.0);
	Vector3 m_scale = Vector3(1.0,1.0,1.0);
	Vector3 m_rotation = Vector3(0.0, 0.0, 0.0);
	bool m_isWireframe = false;
};

class ShadedMeshOnTrack : public ShadedMesh {
public:
	ShadedMeshOnTrack(class Game* game, const String& name) : ShadedMesh(name), m_game(game) { }
	ShadedMeshOnTrack(class Game* game, const String& name, const String& path) : ShadedMesh(name, path), m_game(game) { }
	ShadedMeshOnTrack(class Game* game) : ShadedMesh(), m_game(game) { }
	virtual ~ShadedMeshOnTrack() override = default;

	static int lNew(struct lua_State* L, class Game* game);
	void DrawOnTrack();
	void lUseGameMesh(struct lua_State* L);


	void SetLength(float l) noexcept { m_length = l; }
	[[nodiscard]]
	float GetLength() noexcept { return m_length; }
	void SetClipping(bool c) noexcept { m_clip = c; }

	[[nodiscard]]
	Game* GetGame() noexcept { return m_game; };

private:
	class Game* m_game = nullptr;
	float m_length = 1;
	bool m_clip = false;
};