#pragma once

#include "CoreMinimal.h"

#include "ValueLadderSemanticRole.generated.h"

UENUM()
enum class EValueLadderSemanticRole : uint8
{
	GenericScalar,
	Translation,
	Rotation,
	Scale,
	Time,
	Mass,
	IntegerDiscrete
};
