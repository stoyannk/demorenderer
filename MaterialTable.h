// Copyright (c) 2013-2014, Stoyan Nikolov
// All rights reserved.
#pragma once

#ifndef MINIMAL_SIZE
// The textures are loaded from JSON descriptions
class MaterialTable 
{
public:
	typedef std::vector<std::string> TextureNames;

	struct TextureProperties
	{
		TextureProperties()
			: UVScale(1)
		{}
		float UVScale;
	};
	typedef std::vector<TextureProperties> TexturePropertiesVec;

	bool Load(const std::string& filename);

	const TextureNames& GetDiffuseTextureList() const;
	const TexturePropertiesVec& GetDiffuseTextureProperties() const;

	struct MaterialIds
	{
		unsigned char DiffuseIds0[3]; /* z, xy, -z*/
		unsigned char DiffuseIds1[3];
	};

	MaterialIds* GetMaterial(unsigned char id) const;
	unsigned GetMaterialsCount() const;

private:
	typedef std::vector<MaterialIds> MaterialsVec;

	TextureNames m_DiffuseTextures;
	TexturePropertiesVec m_TextureProperties;
	MaterialsVec m_Materials;
};

bool GenerateMaterialFromTable(
	const MaterialTable& table,
	const std::string& rootFolder,
class TextureManager& manager,
class Material& outMaterial);

#else
bool GenerateMaterialFromList(
	const std::string& rootFolder,
	const std::vector<std::string>& diffuseFiles,
	class TextureManager& manager,
	class Material& outMaterial);
#endif