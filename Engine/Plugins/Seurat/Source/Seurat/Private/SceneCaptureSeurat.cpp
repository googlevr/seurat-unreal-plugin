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

#include "SceneCaptureSeurat.h"
#include "Seurat.h"
#include "Components/SceneCaptureComponent2D.h"


ASceneCaptureSeurat::ASceneCaptureSeurat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize capturing parameters.
	Resolution = ECaptureResolution::K1024;
	SamplesPerFace = EPositionSampleCount::K8;
	HeadboxSize = FVector(100, 100, 100);
	GetCaptureComponent2D()->bCaptureEveryFrame = false;
	GetCaptureComponent2D()->bCaptureOnMovement = false;
	PrimaryActorTick.bCanEverTick = true;
}
