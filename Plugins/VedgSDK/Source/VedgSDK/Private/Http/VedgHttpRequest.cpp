// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/VedgHttpRequest.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "VedgSDKModule.h"

void FVedgHttpRequest::Request(const FString& Url, const FString& Verb, const FString& JsonBody, TFunction<void(FVedgHttpResult)> OnComplete, float Timeout)
{
	const auto HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(Verb);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!JsonBody.IsEmpty())
	{
		HttpRequest->SetContentAsString(JsonBody);
	}
	HttpRequest->SetTimeout(Timeout);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[Url, OnComplete](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bConnectedSuccessfully)
		{
			FVedgHttpResult Result;
			if (bConnectedSuccessfully && Response.IsValid())
			{
				Result.StatusCode = Response->GetResponseCode();
				Result.bValid = Result.StatusCode >= 200 && Result.StatusCode < 300;

				TSharedPtr<FJsonObject> JsonObject;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
				{
					Result.ResponseObject = JsonObject;
				}

				if (!Result.bValid)
				{
					UE_LOG(LogVedgSDK, Warning, TEXT("Request to %s failed with status %d"), *Url, Result.StatusCode);
				}
			}
			else
			{
				UE_LOG(LogVedgSDK, Warning, TEXT("Request to %s failed: no connection"), *Url);
			}

			OnComplete(Result);
		});

	HttpRequest->ProcessRequest();
}
