#pragma once

#include "CoreMinimal.h"

struct FValueLadderUnitMetadata
{
	FString ForceUnits;
	FString Units;
};

namespace ValueLadder::Units
{
	VALUELADDER_API FName ResolveUnitKey(const TArray<FValueLadderUnitMetadata>& MetadataChain);
}
