#include "Input/ValueLadderUnitMetadata.h"

namespace
{
	FName ResolveSingleMetadataValue(const FString& UnitValue)
	{
		const FString NormalizedUnitValue = UnitValue.TrimStartAndEnd();
		return NormalizedUnitValue.IsEmpty() ? NAME_None : FName(*NormalizedUnitValue);
	}
}

FName ValueLadder::Units::ResolveUnitKey(const TArray<FValueLadderUnitMetadata>& MetadataChain)
{
	for (const FValueLadderUnitMetadata& Metadata : MetadataChain)
	{
		const FName ForceUnits = ResolveSingleMetadataValue(Metadata.ForceUnits);
		if (!ForceUnits.IsNone())
		{
			return ForceUnits;
		}
	}

	for (const FValueLadderUnitMetadata& Metadata : MetadataChain)
	{
		const FName Units = ResolveSingleMetadataValue(Metadata.Units);
		if (!Units.IsNone())
		{
			return Units;
		}
	}

	return NAME_None;
}
