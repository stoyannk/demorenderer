// Copyright (c) 2013-2014, Stoyan Nikolov
// All rights reserved.
#include "precompiled.h"

#include "MaterialTable.h"

#include <Dx11/Rendering/TextureManager.h>
#include <Dx11/Rendering/Material.h>
#ifndef MINIMAL_SIZE

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

bool MaterialTable::Load(const std::string& filename)
{
	using boost::property_tree::ptree;
    ptree jsonTree;

	try {
		read_json(filename, jsonTree);
	} catch(std::exception& ex) {
		SLLOG(Sev_Error, Fac_Rendering, "Error reading materials file: ", ex.what());
	}

	std::unordered_map<std::string, TextureProperties> propertiesSet;
	try {
		auto properties = jsonTree.get_child("properties");
		
		std::for_each(properties.begin(), properties.end(), [&](ptree::value_type& property) {
			auto texObj = property.second.begin();
			auto texName = texObj->first.data();
			TextureProperties props;
			props.UVScale = texObj->second.get_child("UVScale").get_value<float>();
			propertiesSet.insert(std::make_pair(texName, props));
		});

	} catch(std::exception& ex) {
		SLLOG(Sev_Error, Fac_Rendering, "Error parsing texture properties: ", ex.what());
		return false;
	}

	TextureNames localTextures;
	TexturePropertiesVec localProperties;
	std::unordered_map<std::string, unsigned> texturesSet;
	auto addTexture = [&](const std::string& texture)->unsigned {
		auto it = texturesSet.find(texture);
		if(it != texturesSet.end())
			return it->second;

		localTextures.push_back(texture);
		auto id = localTextures.size()-1;
		texturesSet.insert(std::make_pair(texture, id));

		TextureProperties properties;
		auto props = propertiesSet.find(texture);
		if(props != propertiesSet.end()) {
			properties = props->second;
		}

		localProperties.push_back(properties);

		return id;
	};

	MaterialsVec localMeterials;
	try {
		auto materials = jsonTree.get_child("materials");
		const auto count = materials.count("");
		
		if(!count) {
			SLOG(Sev_Error, Fac_Rendering, "No materials specified in the table!");
			return false;
		}
				
		localMeterials.reserve(count);

		std::for_each(materials.begin(), materials.end(), [&](ptree::value_type& material) {
			MaterialIds output;
			auto tex0 = material.second.get_child("diffuse0").begin();
			auto tex1 = material.second.get_child("diffuse1").begin();
			for(auto i = 0; i < 3; ++i, ++tex0, ++tex1) {
				output.DiffuseIds0[i] = addTexture(tex0->second.data());
				output.DiffuseIds1[i] = addTexture(tex1->second.data());
			}
			localMeterials.emplace_back(output);
		});
	} catch(std::exception& ex) {
		SLLOG(Sev_Error, Fac_Rendering, "Error parsing materials: ", ex.what());
		return false;
	}
	
	std::swap(localMeterials, m_Materials);
	std::swap(localTextures, m_DiffuseTextures);
	std::swap(localProperties, m_TextureProperties);

	return true;
}

const MaterialTable::TextureNames& MaterialTable::GetDiffuseTextureList() const
{
	return m_DiffuseTextures;
}

const MaterialTable::TexturePropertiesVec& MaterialTable::GetDiffuseTextureProperties() const
{
	return m_TextureProperties;
}

MaterialTable::MaterialIds* MaterialTable::GetMaterial(unsigned char id) const
{
	if(m_Materials.size() <= id)
		return nullptr;

	return const_cast<MaterialIds*>(&(m_Materials[id]));
}

unsigned MaterialTable::GetMaterialsCount() const
{
	return m_Materials.size();
}

bool GenerateMaterialFromTable(
	const MaterialTable& table,
	const std::string& rootFolder,
	TextureManager& manager,
	Material& outMaterial)
{
	const auto& diffuseFiles = table.GetDiffuseTextureList();
	auto diffuseTextures = manager.LoadTexture2DArray(diffuseFiles, rootFolder, true);
	if (!diffuseTextures)
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load diffuse material table textures!");
		return false;
	}

	MaterialTable::TextureNames normalFiles;
	normalFiles.reserve(diffuseFiles.size());
	std::transform(diffuseFiles.cbegin(), diffuseFiles.cend(), std::back_inserter(normalFiles), [](const std::string& filename) {
		std::string name(filename.begin(), filename.begin() + filename.find_first_of('.'));
		return name + "_normal.dds";
	});
	auto normalTextures = manager.LoadTexture2DArray(normalFiles, rootFolder, false);
	if (!normalTextures)
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load normal material table textures!");
		return false;
	}

	outMaterial.SetDiffuse(diffuseTextures);
	outMaterial.SetNormalMap(normalTextures);
	outMaterial.SetProperty(MP_TextureArrays);

	return true;
}
#else
bool GenerateMaterialFromList(
	const std::string& rootFolder,
	const std::vector<std::string>& diffuseFiles,
	TextureManager& manager,
	Material& outMaterial)
{
	auto diffuseTextures = manager.LoadTexture2DArray(diffuseFiles, rootFolder, true);
	if (!diffuseTextures)
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load diffuse material table textures!");
		return false;
	}

	std::vector<std::string> normalFiles;
	normalFiles.reserve(diffuseFiles.size());
	std::transform(diffuseFiles.cbegin(), diffuseFiles.cend(), std::back_inserter(normalFiles), [](const std::string& filename) {
		std::string name(filename.begin(), filename.begin() + filename.find_first_of('.'));
		return name + "_normal.dds";
	});
	auto normalTextures = manager.LoadTexture2DArray(normalFiles, rootFolder, false);
	if (!normalTextures)
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load normal material table textures!");
		return false;
	}

	outMaterial.SetDiffuse(diffuseTextures);
	outMaterial.SetNormalMap(normalTextures);
	outMaterial.SetProperty(MP_TextureArrays);

	return true;
}
#endif