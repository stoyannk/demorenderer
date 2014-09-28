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

[numthreads(1, 1, 1)]
void FinalizeCounters()
{
	IndirectInfo.Store(0 * 4, Counters[0].IndicesCount);
	IndirectInfo.Store(1 * 4, 1);
	IndirectInfo.Store(2 * 4, 0);
	IndirectInfo.Store(3 * 4, 0);
	IndirectInfo.Store(4 * 4, 0);
}
