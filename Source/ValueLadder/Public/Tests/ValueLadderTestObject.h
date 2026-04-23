#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ValueLadderTestObject.generated.h"

UCLASS()
class VALUELADDER_API UValueLadderTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	int8 Int8Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	int16 Int16Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	int64 Int64Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder", meta = (ClampMin = "0", UIMin = "0", UIMax = "7"))
	uint8 UInt8ClampedValue = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	uint16 UInt16Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	uint32 UInt32Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	uint64 UInt64Value = 0;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "ValueLadder")
	FIntPoint IntPointValue = FIntPoint(0, 0);
};
