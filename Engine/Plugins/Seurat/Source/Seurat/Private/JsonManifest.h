/* Copyright 2017 Google Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

static TArray<TSharedPtr<FJsonValue>> MatrixToJsonArray(FMatrix Matrix)
{
	TArray<TSharedPtr<FJsonValue>> Ret;
	for (int32 i = 0; i<4; i++)
		for (int32 j = 0; j < 4; j++)
		{
			Ret.Add(MakeShareable(new FJsonValueNumber(Matrix.M[j][i])));
		}
	return Ret;
}

class ProjectiveCamera {
public:
	int32 ImageWidth;
	int32 ImageHeight;
	FMatrix ClipFromEyeMatrix;
	FMatrix WorldFromEyeMatrix;
	FString DepthType;
	TSharedPtr<FJsonObject> ToJson()
	{
		TSharedPtr<FJsonObject> Ret = MakeShareable(new FJsonObject());
		Ret->SetNumberField("image_width", ImageWidth);
		Ret->SetNumberField("image_height", ImageHeight);
		Ret->SetArrayField("clip_from_eye_matrix", MatrixToJsonArray(ClipFromEyeMatrix));
		Ret->SetArrayField("world_from_eye_matrix", MatrixToJsonArray(WorldFromEyeMatrix));
		Ret->SetStringField("depth_type", DepthType);
		return Ret;
	}
};

class Image4File {
public:
	FString Path;
	FString Channel0;
	FString Channel1;
	FString Channel2;
	FString ChannelAlpha;
	TSharedPtr<FJsonObject> ToJson()
	{
		TSharedPtr<FJsonObject> Ret = MakeShareable(new FJsonObject());
		Ret->SetStringField("path", Path);
		Ret->SetStringField("channel_0", Channel0);
		Ret->SetStringField("channel_1", Channel1);
		Ret->SetStringField("channel_2", Channel2);
		Ret->SetStringField("channel_alpha", ChannelAlpha);
		return Ret;
	}
};

class Image1File {
public:
	FString Path;
	FString Channel0;
	TSharedPtr<FJsonObject> ToJson()
	{
		TSharedPtr<FJsonObject> Ret = MakeShareable(new FJsonObject());
		Ret->SetStringField("path", Path);
		Ret->SetStringField("channel_0", Channel0);
		return Ret;
	}
};

class DepthImageFile {
public:
	Image4File Color;
	Image1File Depth;
	TSharedPtr<FJsonObject> ToJson()
	{
		TSharedPtr<FJsonObject> Ret = MakeShareable(new FJsonObject());
		Ret->SetObjectField("color", Color.ToJson());
		Ret->SetObjectField("depth", Depth.ToJson());
		return Ret;
	}
};

class SeuratView {
public:
	ProjectiveCamera ProjectiveCamera;
	DepthImageFile DepthImageFile;
	TSharedPtr<FJsonObject> ToJson()
	{
		TSharedPtr<FJsonObject> Ret = MakeShareable(new FJsonObject());
		Ret->SetObjectField("projective_camera", ProjectiveCamera.ToJson());
		Ret->SetObjectField("depth_image_file", DepthImageFile.ToJson());
		return Ret;
	}
};

