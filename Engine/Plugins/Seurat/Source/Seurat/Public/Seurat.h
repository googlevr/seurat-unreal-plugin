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
#include "Engine/EngineBaseTypes.h"
#include "Framework/Commands/UICommandList.h"
#include "ModuleManager.h"
#include "HighResScreenshot.h"
#include "TextureResource.h"
#include "SceneCaptureSeurat.h"

class FToolBarBuilder;
class FMenuBuilder;

class FSeuratModule : public IModuleInterface
{
public:
	FSeuratModule();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void BeginCapture(ASceneCaptureSeurat* InCaptureCamera);
	void EndCapture();
	void CancelCapture();
	void Tick(ELevelTick TickType, float DeltaSeconds);

	// Fields related to capture process.
	TArray<FVector> Samples;
	TArray<TSharedPtr<FJsonValue>> ViewGroups;
	TSharedPtr<FJsonObject> ViewGroup;
	TArray<TSharedPtr<FJsonValue>> Views;
	int32 CurrentSide;
	int32 CurrentSample;
	int32 CaptureTimer;

private:
	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	bool PauseTimeFlow(ASceneCaptureSeurat* InCaptureCamera);
	void RestoreTimeFlow(ASceneCaptureSeurat* InCaptureCamera);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TWeakObjectPtr<ASceneCaptureSeurat> ColorCameraActor;
	USceneCaptureComponent2D* ColorCamera;

	// Saves and restores camera actor transformation.
	FVector InitialPosition;
	FRotator InitialRotation;

	// Tracks editor and in-editor time update state to allow capture to operate
	// over multiple frames without time changing.
	bool bNeedRestoreRealtime;
	bool bNeedRestoreGamePaused;
	// Saves and restores editor performance monitoring feature as capture
	// automatically disables the feature to capture with precisely the rendering
	// features enabled at the start of capture.
	bool bNeedRestoreMonitorEditorPerformance;

	// Transforms capture camera in world space to origin.
	FMatrix WorldFromReferenceCameraMatrixSeurat;
	// Stores the prefix of all capture output files.
	FString BaseImageName;

	void WriteImage(UTextureRenderTarget2D* InRenderTarget, FString Filename, bool bClearAlpha);
	bool SaveStringTextToFile(FString SaveDirectory, FString FileName, FString SaveText, bool AllowOverWriting);
	void CaptureSeurat();
	TSharedPtr<FJsonObject> Capture(FRotator Orientation, FVector Position);
	void GenerateJson(TSharedPtr<FJsonObject> JsonObject, FString ExportPath);
};

DECLARE_LOG_CATEGORY_EXTERN(Seurat, Log, All);