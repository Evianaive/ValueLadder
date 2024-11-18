// Fill out your copyright notice in the Description page of Project Settings.


#include "ValueLadderSettings.h"


template<> const TArray<float>& UValueLadderSettings::GetLadderValues() const{return FloatLadders;};
template<> const TArray<int32>& UValueLadderSettings::GetLadderValues() const{return IntLadders;};

template<> int32 UValueLadderSettings::GetDefaultLadderValuesIndex<float>() const{return DefaultFloatLadderIndex;};
template<> int32 UValueLadderSettings::GetDefaultLadderValuesIndex<int32>() const{return DefaultIntLadderIndex;};
