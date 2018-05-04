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
#include "Engine/SceneCapture2D.h"
#include "GameFramework/Actor.h"
#include "SceneCaptureSeurat.generated.h"

UENUM()
enum class EPositionSampleCount : uint8
{
	K2 = 1,
	K4 = 2,
	K8 = 3,
	K16 = 4,
	K32 = 5,
	K64 = 6,
	K128 = 7,
	K256 = 8,
};

UENUM()
enum class ECaptureResolution : uint8
{
	K512 = 9,
	K1024 = 10,
	K2048 = 11,
	K4096 = 12,
	K1536 = 13,
};

UCLASS(hidecategories = (Collision, Material, Attachment, Actor), MinimalAPI)
class ASceneCaptureSeurat : public ASceneCapture2D
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SeuratSettings)
	FVector HeadboxSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SeuratSettings, meta = (DisplayName = "Samples Per Face"))
	EPositionSampleCount SamplesPerFace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SeuratSettings, meta = (DisplayName = "Resolution"))
	ECaptureResolution Resolution;
};
