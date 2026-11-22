// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/VedgText.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Column header -> key -> text.
	TMap<FString, TMap<FString, FString>> GTables;
	bool bGLoaded = false;

	// RFC 4180: quoted fields may hold commas, newlines and "" escapes.
	TArray<TArray<FString>> ParseCsv(const FString& Text)
	{
		TArray<TArray<FString>> Rows;
		TArray<FString> Row;
		FString Field;
		bool bQuoted = false;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR C = Text[i];
			if (bQuoted)
			{
				if (C == TEXT('"') && i + 1 < Text.Len() && Text[i + 1] == TEXT('"'))
				{
					Field.AppendChar(TEXT('"'));
					++i;
				}
				else if (C == TEXT('"'))
				{
					bQuoted = false;
				}
				else
				{
					Field.AppendChar(C);
				}
			}
			else if (C == TEXT('"'))
			{
				bQuoted = true;
			}
			else if (C == TEXT(','))
			{
				Row.Add(MoveTemp(Field));
				Field.Reset();
			}
			else if (C == TEXT('\n'))
			{
				Field.TrimEndInline();
				Row.Add(MoveTemp(Field));
				Field.Reset();
				Rows.Add(MoveTemp(Row));
				Row.Reset();
			}
			else
			{
				Field.AppendChar(C);
			}
		}
		if (!Field.IsEmpty() || Row.Num() > 0)
		{
			Row.Add(Field);
			Rows.Add(Row);
		}
		return Rows;
	}

	void Load()
	{
		bGLoaded = true;
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VedgSDK"));
		if (!Plugin.IsValid())
		{
			return;
		}

		for (const TCHAR* Name : { TEXT("login"), TEXT("social") })
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Resources/Translations") / (FString(Name) + TEXT(".csv")))))
			{
				continue;
			}
			const TArray<TArray<FString>> Rows = ParseCsv(Text);
			if (Rows.Num() == 0)
			{
				continue;
			}
			const TArray<FString>& Header = Rows[0];
			for (int32 R = 1; R < Rows.Num(); ++R)
			{
				if (Rows[R].Num() == 0 || Rows[R][0].IsEmpty())
				{
					continue;
				}
				for (int32 Col = 1; Col < Header.Num() && Col < Rows[R].Num(); ++Col)
				{
					GTables.FindOrAdd(Header[Col]).Add(Rows[R][0], Rows[R][Col].Replace(TEXT("%s"), TEXT("{0}")));
				}
			}
		}
	}

	const FString* Find(const FString& Column, const FString& Key)
	{
		const TMap<FString, FString>* Table = GTables.Find(Column);
		const FString* Text = Table ? Table->Find(Key) : nullptr;
		return Text && !Text->IsEmpty() ? Text : nullptr;
	}
}

FString FVedgText::GetString(const FString& Key, const FString& Fallback)
{
	if (!bGLoaded)
	{
		Load();
	}

	// Headers are "fr_FR", "en_US", ... - match on the language part.
	const FString Language = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
	for (const TPair<FString, TMap<FString, FString>>& Table : GTables)
	{
		if (Table.Key.StartsWith(Language + TEXT("_")))
		{
			if (const FString* Text = Find(Table.Key, Key))
			{
				return *Text;
			}
		}
	}
	if (const FString* Text = Find(TEXT("en_US"), Key))
	{
		return *Text;
	}
	return Fallback;
}

FText FVedgText::Get(const FString& Key)
{
	return FText::FromString(GetString(Key, Key));
}
