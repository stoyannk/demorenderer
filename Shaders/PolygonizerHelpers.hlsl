struct GenerateCounters
{
	uint VerticesCount;
	uint IndicesCount;
};

RWByteAddressBuffer IndirectInfo : register(u2);
RWStructuredBuffer<GenerateCounters> Counters : register(u3);

[numthreads(1, 1, 1)]
void InitCounters()
{
	Counters[0].VerticesCount = 0;
	Counters[0].IndicesCount = 0;
}
