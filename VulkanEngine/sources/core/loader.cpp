#include "loader.h"

// Due to forward declaration...
#include "engine.h"

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(Engine* engine, std::filesystem::path filePath)
{
	fmt::println("Loading glTF from \"{}\".", filePath.string());

	auto data = fastgltf::GltfDataBuffer::FromPath(filePath);

	if (data.error() != fastgltf::Error::None)
	{
		fmt::println("Failed to load glTF data buffer from path \"{}\".", filePath.string());

		return {};
	}

	fastgltf::Parser parser;
	constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;
	auto expected = parser.loadGltf(data.get(), filePath.parent_path(), gltfOptions);

	if (auto error = expected.error(); error != fastgltf::Error::None)
	{
		fmt::println("Failed to load glTF: {}.", fastgltf::to_underlying(expected.error()));

		return {};
	}

	fastgltf::Asset asset = std::move(expected.get());
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (fastgltf::Mesh& mesh : asset.meshes)
	{
		MeshAsset newMeshAsset;

		newMeshAsset.name = mesh.name;

		vertices.clear();
		indices.clear();

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;

			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)asset.accessors[p.indicesAccessor.value()].count;

			size_t startVertex = vertices.size();

			// Load indexes.
			{
				fastgltf::Accessor& indexAccessor = asset.accessors[p.indicesAccessor.value()];

				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(asset, indexAccessor, [&](std::uint32_t index)
				{
					indices.push_back(index + (uint32_t)startVertex);
				});
			}

			// Load vertex positions.
			{
				fastgltf::Attribute* positionAttribute = p.findAttribute("POSITION");
				fastgltf::Accessor& positionAccessor = asset.accessors[positionAttribute->accessorIndex];

				vertices.resize(vertices.size() + positionAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&](glm::vec3 position, size_t index)
				{
					Vertex newVertex;

					newVertex.position = position;
					newVertex.normal = { 1.0f, 0.0f, 0.0f };
					newVertex.color = glm::vec4{ 1.0f };
					newVertex.uvX = 0;
					newVertex.uvY = 0;

					vertices[startVertex + index] = newVertex;
				});
			}

			// Load vertex normals.
			fastgltf::Attribute* normalAttribute = p.findAttribute("NORMAL");

			if (normalAttribute != p.attributes.end())
			{
				fastgltf::Accessor& normalAccessor = asset.accessors[normalAttribute->accessorIndex];

				fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normalAccessor, [&](glm::vec3 normal, size_t index)
				{
					vertices[startVertex + index].normal = normal;
				});
			}

			// Load UVs.
			fastgltf::Attribute* uvAttribute = p.findAttribute("TEXCOORD_0");

			if (uvAttribute != p.attributes.end())
			{
				fastgltf::Accessor& uvAccessor = asset.accessors[uvAttribute->accessorIndex];

				fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uvAccessor, [&](glm::vec2 uv, size_t index)
				{
					vertices[startVertex + index].uvX = uv.x;
					vertices[startVertex + index].uvY = uv.y;
				});
			}

			// Load vertex colors.
			fastgltf::Attribute* colorAttribute = p.findAttribute("COLOR_0");

			if (colorAttribute != p.attributes.end())
			{
				fastgltf::Accessor& colorAccessor = asset.accessors[colorAttribute->accessorIndex];

				fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colorAccessor, [&](glm::vec4 color, size_t index)
				{
					vertices[startVertex + index].color = color;
				});
			}

			newMeshAsset.surfaces.push_back(newSurface);
		}

		// Display the vertex normals.
		constexpr bool overrideColors = true;

		if (overrideColors)
		{
			for (Vertex& vertex : vertices)
			{
				vertex.color = glm::vec4(vertex.normal, 1.0f);
			}
		}

		newMeshAsset.meshBuffers = engine->uploadMesh(vertices, indices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMeshAsset)));
	}

	return meshes;
}
