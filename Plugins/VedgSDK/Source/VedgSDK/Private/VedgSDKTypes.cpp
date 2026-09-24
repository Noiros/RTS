// Fill out your copyright notice in the Description page of Project Settings.

#include "VedgSDKTypes.h"

FString VedgEnvToString(EVedgEnv Env)
{
	switch (Env)
	{
	case EVedgEnv::Local:
		return TEXT("local");
	case EVedgEnv::Prod:
		return TEXT("prod");
	case EVedgEnv::Dev:
	default:
		return TEXT("dev");
	}
}

bool VedgEnvFromString(const FString& Value, EVedgEnv& OutEnv)
{
	if (Value.Equals(TEXT("local"), ESearchCase::IgnoreCase))
	{
		OutEnv = EVedgEnv::Local;
		return true;
	}
	if (Value.Equals(TEXT("dev"), ESearchCase::IgnoreCase))
	{
		OutEnv = EVedgEnv::Dev;
		return true;
	}
	if (Value.Equals(TEXT("prod"), ESearchCase::IgnoreCase))
	{
		OutEnv = EVedgEnv::Prod;
		return true;
	}
	return false;
}
