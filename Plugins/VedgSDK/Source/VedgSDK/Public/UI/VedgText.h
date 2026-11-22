// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// Panel/error strings, read from the Godot addon's own translation CSVs
// (Resources/Translations/*.csv, copied as-is: key,fr_FR,en_US,...). The column
// follows the current culture's language, falling back to en_US.
class VEDGSDK_API FVedgText
{
public:
	static FText Get(const FString& Key);
	// Raw string, Fallback when the key is unknown.
	static FString GetString(const FString& Key, const FString& Fallback);
};
