#include "MCTables.hlsl"

struct GenerateCounters
{
	uint VerticesCount;
	uint IndicesCount;
};

RWByteAddressBuffer BufferOut : register(u0);
RWByteAddressBuffer IndexBufferOut : register(u1);
RWByteAddressBuffer IndirectInfo : register(u2);
RWStructuredBuffer<GenerateCounters> Counters : register(u3);

StructuredBuffer<RegularCellData> CellData : register(t0);
StructuredBuffer<uint> VertexData : register(t1);

float3 calcNormal(float3 position)
{
	return normalize(float3(
		sceneDistance(position + float3(NORM_DELTA, 0, 0)) - sceneDistance(position - float3(NORM_DELTA, 0, 0)),
		sceneDistance(position + float3(0, NORM_DELTA, 0)) - sceneDistance(position - float3(0, NORM_DELTA, 0)),
		sceneDistance(position + float3(0, 0, NORM_DELTA)) - sceneDistance(position - float3(0, 0, NORM_DELTA))
		));
}

#define SHARED_DIST_SIZE 512

groupshared float groupDists[SHARED_DIST_SIZE];

[numthreads(8, 8, 8)]
void PolygonizerCS(uint3 DTid : SV_DispatchThreadID,
	uint gtid : SV_GroupIndex,
	uint3 groupIds : SV_GroupThreadID)
{
	const uint3 groupDims = float3(8, 8, 8);
	float3 origin = DTid * Step + InitialCoords;

	// eash thread computes its dist
	groupDists[gtid] = sceneDistance(origin);

	GroupMemoryBarrierWithGroupSync();
	uint3 offsets[8];
	offsets[0] = uint3(0, 0, 0);
	offsets[1] = uint3(1, 0, 0);
	offsets[2] = uint3(0, 0, 1);
	offsets[3] = uint3(1, 0, 1);

	offsets[4] = uint3(0, 1, 0);
	offsets[5] = uint3(1, 1, 0);
	offsets[6] = uint3(0, 1, 1);
	offsets[7] = uint3(1, 1, 1);

	const uint3 groupCoeff = uint3(1, 8, 64);
	float3 positions[8];
	float distances[8];
	int corner[8];
	for (int i = 0; i < 8; ++i)
	{
		positions[i] = origin + offsets[i] * Step;

		uint3 distId = groupIds + offsets[i];
		if (any(distId >= groupDims)) {
			distances[i] = sceneDistance(positions[i]);
		}
		else {
			distances[i] = groupDists[dot(distId, groupCoeff)];
		}
		corner[i] = distances[i] >= 0 ? 1 : -1;
	}

	uint caseCode = ((corner[0] >> 7) & 0x01)
				  | ((corner[1] >> 6) & 0x02)
				  | ((corner[2] >> 5) & 0x04)
				  | ((corner[3] >> 4) & 0x08)
				  | ((corner[4] >> 3) & 0x10)
				  | ((corner[5] >> 2) & 0x20)
				  | ((corner[6] >> 1) & 0x40)
				  | (corner[7] & 0x80);
	
	if ((caseCode ^ ((corner[7] >> 7) & 0xFF)) != 0)
	{
		uint caseIndex = regularCellClass[caseCode];

		RegularCellData cellData = CellData[caseIndex];

		int vertexCount = GetVertexCount(cellData);
		int triangleCount = GetTriangleCount(cellData);
		
		// Reserve memory
		uint myVertexSlot = 0;
		InterlockedAdd(Counters[0].VerticesCount, vertexCount, myVertexSlot);
		uint myIndexSlot = 0;
		InterlockedAdd(Counters[0].IndicesCount, triangleCount * 3, myIndexSlot);
		
		for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			uint edgeIndex = VertexData[caseCode * 12 + vertexIndex] & 0xFF;

			int v0 = (edgeIndex >> 4) & 0x0F;
			int v1 = edgeIndex & 0x0F;

			float diff = distances[v1] - distances[v0];
			float t = (abs(diff) > 0.0) ? (distances[v1] / (diff)) : 0.0;

			float3 vertexPosition = lerp(positions[v1], positions[v0], t);

			uint address = (myVertexSlot + vertexIndex) * 32;
			BufferOut.Store3(address,
				uint3(asuint(vertexPosition.x),
				asuint(vertexPosition.y),
				asuint(vertexPosition.z)));

			const float3 normal = calcNormal(vertexPosition);
			BufferOut.Store3(address + 12,
				uint3(asuint(normal.x),
				asuint(normal.y),
				asuint(normal.z)));

			uint2 textureInds = textureIndices(vertexPosition, lerp(distances[v1], distances[v0], t));
			BufferOut.Store2(address + 24, textureInds);
		}

		for (int index = 0; index < triangleCount * 3; ++index)
		{
			IndexBufferOut.Store((myIndexSlot + index) * 4, myVertexSlot + cellData.vertexIndex[index]);
		}
	}
}
