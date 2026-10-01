#include "EnginePCH.h"
#include "StaticMeshData.h"

#include <format>

bool FStaticMeshData::Validate(FString& OutError) const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = std::format("invalid mesh (vertices {}, indices {})", VertexCount, IndexCount);
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= VertexCount)
		{
			OutError = std::format("index {} out of range (vertices {})", Index, VertexCount);
			return false;
		}
	}

	uint32 SectionIndexSum = 0;
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.StartIndex + Section.IndexCount > IndexCount)
		{
			OutError = std::format("section range {}+{} exceeds indices {}", Section.StartIndex, Section.IndexCount, IndexCount);
			return false;
		}
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
		{
			OutError = std::format("material slot {} out of range (slots {})", Section.MaterialSlotIndex, MaterialSlots.Num());
			return false;
		}
		SectionIndexSum += Section.IndexCount;
	}
	if (SectionIndexSum != IndexCount)
	{
		OutError = std::format("section index sum {} != indices {}", SectionIndexSum, IndexCount);
		return false;
	}

	return true;
}
