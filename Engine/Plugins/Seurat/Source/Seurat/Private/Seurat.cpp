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

#include "Seurat.h"
#include "SeuratConfigWindow.h"
#include "SceneCaptureSeuratDetail.h"
#include "JsonManifest.h"

#include "Framework/SlateDelegates.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Misc/FileHelper.h"

#include "SeuratStyle.h"
#include "SeuratCommands.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Editor/EditorPerformanceSettings.h"
#include "LevelEditor.h"
#include "Kismet/GameplayStatics.h"
#endif // WITH_EDITOR

static const FString kSeuratOutputDir = FPaths::ConvertRelativePathToFull(FPaths::GameIntermediateDir() / "SeuratCapture");
static const int32 kTimerExpirationsPerCapture = 4;

#define LOCTEXT_NAMESPACE "FSeuratModule"

FSeuratModule::FSeuratModule() : InitialPosition(FVector::ZeroVector),
	InitialRotation(FRotator::ZeroRotator), bNeedRestoreRealtime(false),
	bNeedRestoreGamePaused(false), bNeedRestoreMonitorEditorPerformance(false),
	WorldFromReferenceCameraMatrixSeurat(FMatrix::Identity)
{
}

// Pause the time flow before capture, since Seurat only works with static scenes.
bool FSeuratModule::PauseTimeFlow(ASceneCaptureSeurat* InCaptureCamera)
{
	UWorld* World = InCaptureCamera->GetWorld();

	if (World->WorldType == EWorldType::Editor)
	{
		FEditorViewportClient* EditorViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());

		bool bRealTime = EditorViewportClient->IsRealtime();

		if (bRealTime)
		{
			bNeedRestoreRealtime = true;
			EditorViewportClient->SetRealtime(false);
		}
		return true;
	}
	else if (World->WorldType == EWorldType::PIE)
	{
		if (!UGameplayStatics::IsGamePaused(World))
		{
			bNeedRestoreGamePaused = true;
			if (!UGameplayStatics::SetGamePaused(World, true))
			{
				UE_LOG(Seurat, Error, TEXT("Capture process cannot pause the game. Please pause the game manually before capture."));
			}
		}
		return true;
	}
	// Seurat plugin only support Editor or PIE mode;
	else
	{
		return false;
	}
}

// Restore time flow to the initial state before capture.
void FSeuratModule::RestoreTimeFlow(ASceneCaptureSeurat* InCaptureCamera)
{
	UWorld* World = InCaptureCamera->GetWorld();
	if (bNeedRestoreRealtime && World->WorldType == EWorldType::Editor)
	{
		FEditorViewportClient* EditorViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
		EditorViewportClient->SetRealtime(true);
	}
	else if (bNeedRestoreGamePaused && World->WorldType == EWorldType::PIE)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
}

// Computes the radical inverse base |DigitBase| of the given value |A|.
static float RadicalInverse(uint64 A, uint64 DigitBase) {
	float InvBase = 1.0f / DigitBase;
	uint64 ReversedDigits = 0;
	float InvBaseN = 1.0f;
	// Compute the reversed digits in the base entirely in integer arithmetic.
	while (A != 0) {
		uint64 Next = A / DigitBase;
		uint64 Digit = A - Next * DigitBase;
		ReversedDigits = ReversedDigits * DigitBase + Digit;
		InvBaseN *= InvBase;
		A = Next;
	}
	// Only when done are the reversed digits divided by base^n.
	return FMath::Min(ReversedDigits * InvBaseN, 1.0f);
}

void FSeuratModule::StartupModule()
{
	//Register the tick function here.
	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FSeuratModule::Tick);

	// Initialize capturing parameters.
	CurrentSide = 0;
	CurrentSample = -1;

	// Bind the delegate for SceneCaptureCamera UI customization.
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("SceneCaptureSeurat", FOnGetDetailCustomizationInstance::CreateStatic(&FSceneCaptureSeuratDetail::MakeInstance));

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FSeuratStyle::Initialize();
	FSeuratStyle::ReloadTextures();

	FSeuratCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FSeuratModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}

	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FSeuratModule::AddToolbarExtension));

		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FSeuratModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FSeuratStyle::Shutdown();

	FSeuratCommands::Unregister();

	// Unbind the delegate for SceneCaptureCamera UI customization.
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("SceneCaptureSeurat");
}

void FSeuratModule::AddMenuExtension(FMenuBuilder& Builder)
{
}

void FSeuratModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
}

void FSeuratModule::Tick(ELevelTick TickType, float DeltaSeconds)
{
	if (CurrentSample < 0)
	{
		return;
	}

	// Lose Camera reference. End the capture.
	if (ColorCameraActor == nullptr || ColorCameraActor->IsPendingKill())
	{
		CancelCapture();
		return;
	}

	--CaptureTimer;
	if (CaptureTimer==0)
	{
		// Write out color data.
		FString ColorImageName = BaseImageName + "_ColorDepth.exr";
		WriteImage(ColorCamera->TextureTarget, kSeuratOutputDir / ColorImageName, true);

		if (CurrentSample == Samples.Num())
		{
			EndCapture();
		}

		CaptureTimer = kTimerExpirationsPerCapture;
	}
	else if (CaptureTimer == kTimerExpirationsPerCapture - 1)
	{
		CaptureSeurat();
	}
}

// Convert a transformation matrix in Unreal coordinate system to Seurat's
// coordinates.
FMatrix SeuratMatrixFromUnrealMatrix(FMatrix UnrealTransform) {
	// Use a change of basis to transform the matrix from Unreal's coordinate
	// system into Seurat's. The matrix SeuratFromUnrealCoordinates encodes
	// the following change of coordinates:
	//
	// Unreal +X is forward and maps to Seurat -Z.
	// Unreal +Y is right and maps to Seurat +X.
	// Unreal +Z is up and maps to Seurat +Y.
	const FMatrix SeuratFromUnrealCoordinates(
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 1, 0),
		FPlane(-1, 0, 0, 0),
		FPlane(0, 0, 0, 1));

	// Apply the change of basis, S = P * U * P^-1, to convert Unreal matrix U to
	// Seurat matrix S. Note that some authors define the change of basis with
	// the form S = P^-1 * U * P, and in this case P would be the transformation
	// from Seurat to Unreal coordinates.
	return SeuratFromUnrealCoordinates * UnrealTransform * SeuratFromUnrealCoordinates.Inverse();
}

void FSeuratModule::BeginCapture(ASceneCaptureSeurat* InCaptureCamera)
{
	// Disable Monitor Editor Performance before capture, so it won't reduce graphic settings and ruin the capture.
	UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();
	bNeedRestoreMonitorEditorPerformance = EditorPerformanceSettings->bMonitorEditorPerformance;
	EditorPerformanceSettings->bMonitorEditorPerformance = false;

	UEditorPerProjectUserSettings* EditorUserSettings = GetMutableDefault<UEditorPerProjectUserSettings>();
	EditorUserSettings->PostEditChange();
	EditorUserSettings->SaveConfig();

	// Pause the time since Seurat only works with static scenes.
	if (!PauseTimeFlow(InCaptureCamera))
	{
		UE_LOG(Seurat, Error, TEXT("Seurat plugin only runs in Editor or PIE mode!"));
		return;
	}

	ColorCameraActor = InCaptureCamera;

	// The Capture already began, do nothing.
	if (CurrentSample >= 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Capture in Progress", "Please wait for current capture progress before start another!"));
		return;
	}

	// Save initial camera state.
	InitialPosition = ColorCameraActor->GetActorLocation();
	InitialRotation = ColorCameraActor->GetActorRotation();

	// Calculate this matrix before changing capture camera postion.
	WorldFromReferenceCameraMatrixSeurat = SeuratMatrixFromUnrealMatrix(
		ColorCameraActor->GetTransform().ToMatrixNoScale());

	ColorCamera = ColorCameraActor->GetCaptureComponent2D();
	ColorCamera->TextureTarget = NewObject<UTextureRenderTarget2D>();
	int32 InResolution = static_cast<int32>(ColorCameraActor->Resolution);
	int32 Resolution = InResolution == 13 ? 1536 : FGenericPlatformMath::Pow(2, InResolution);
	ColorCamera->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
	ColorCamera->TextureTarget->InitCustomFormat(Resolution, Resolution, PF_FloatRGBA, true);

	Samples.Empty();
	FVector HeadboxSize = ColorCameraActor->HeadboxSize;
	int32 SamplesPerFace = FGenericPlatformMath::Pow(2, static_cast<int32>(ColorCameraActor->SamplesPerFace));
	// Use Hammersly sampling for reproduciblity.
	for (int32 SampleIndex = 0; SampleIndex < SamplesPerFace; ++SampleIndex)
	{
		FVector HeadboxPosition = FVector(
			(float)SampleIndex / (float)(SamplesPerFace - 1),
			RadicalInverse((uint64)SampleIndex, 2),
			RadicalInverse((uint64)SampleIndex, 3));
		HeadboxPosition.X *= HeadboxSize.X;
		HeadboxPosition.Y *= HeadboxSize.Y;
		HeadboxPosition.Z *= HeadboxSize.Z;
		HeadboxPosition -= HeadboxSize * 0.5f;
		// Headbox samples are in camera space; transform to world space.
		HeadboxPosition = ColorCameraActor->GetTransform().TransformPosition(HeadboxPosition);
		Samples.Add(HeadboxPosition);
	}

	FVector CameraLocation = ColorCameraActor->GetActorLocation();

	// Sort samples by distance from center of the headbox.
	Samples.Sort([&CameraLocation](const FVector& V1, const FVector& V2) {
		return (V1 - CameraLocation).Size() < (V2 - CameraLocation).Size();
	});

	// Replace the sample closest to the center of the headbox with a sample at
	// exactly the center. This is important because Seurat requires
	// sampling information at the center of the headbox.
	Samples[0] = CameraLocation;

	ViewGroups.Empty();
	CurrentSample = 0;
	CurrentSide = 0;
	CaptureTimer = kTimerExpirationsPerCapture;
}

void FSeuratModule::EndCapture()
{
	// From Json array to a string.
	TSharedPtr<FJsonObject> SeuratManifest = MakeShareable(new FJsonObject());
	SeuratManifest->SetArrayField("view_groups", ViewGroups);
	GenerateJson(SeuratManifest, kSeuratOutputDir);
	Samples.Empty();
	ViewGroups.Empty();
	CurrentSample = -1;

	// Restore camera state.
	ColorCameraActor->SetActorLocation(InitialPosition);
	ColorCameraActor->SetActorRotation(InitialRotation);

	RestoreTimeFlow(ColorCameraActor.Get());

	// Restore Monitor Editor Performance as it is before the capture.
	UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();
	EditorPerformanceSettings->bMonitorEditorPerformance = bNeedRestoreMonitorEditorPerformance;

	UEditorPerProjectUserSettings* EditorUserSettings = GetMutableDefault<UEditorPerProjectUserSettings>();
	EditorUserSettings->PostEditChange();
	EditorUserSettings->SaveConfig();

	ColorCamera->TextureTarget = nullptr;
	ColorCamera = nullptr;
	ColorCameraActor = nullptr;

	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Scene Captured!", "Scene Captured!"));
}

void FSeuratModule::CancelCapture()
{
	Samples.Empty();
	ViewGroups.Empty();
	CurrentSample = -1;

	ColorCamera = nullptr;
	ColorCameraActor = nullptr;

	UE_LOG(Seurat, Error, TEXT("Lost Capture Camera reference. Don't modify the scene while capturing."));
}

void FSeuratModule::CaptureSeurat()
{
	FString BaseName = "Cube";

	TArray<FString> Sides = {
		"Front",
		"Back",
		"Right",
		"Left",
		"Top",
		"Bottom",
	};

	if (CurrentSide == 0)
	{
		ViewGroup = MakeShareable(new FJsonObject());
		Views.Empty();
	}

	int32 NumSides = Sides.Num();
	int32 Side = CurrentSide;
	FString SideName = Sides[Side];
	FRotator FaceRotation = FRotator::ZeroRotator;
	switch (Side)
	{
	case 0: // Front
		break;
	case 1: // Back
		FaceRotation.Yaw += 180.0f;
		break;
	case 2: // Right
		FaceRotation.Yaw += 90.0f;
		break;
	case 3: // Left
		FaceRotation.Yaw += 270.0f;
		break;
	case 4: // Top
		FaceRotation.Pitch += 90.0f;
		break;
	case 5: // Bottom
		FaceRotation.Pitch += 270.0f;
		break;
	default:
		break;
	}

	BaseImageName = BaseName + "_" + SideName + "_" + FString::FromInt(CurrentSample);
	TSharedPtr<FJsonObject> View = Capture(FaceRotation, Samples[CurrentSample]);
	Views.Add(MakeShareable(new FJsonValueObject(View.ToSharedRef())));

	++CurrentSide;
	if (CurrentSide == NumSides)
	{
		CurrentSide = 0;
		++CurrentSample;
		ViewGroup->SetArrayField("views", Views);
		ViewGroups.Add(MakeShareable(new FJsonValueObject(ViewGroup)));
		ViewGroup.Reset();
	}
}

TSharedPtr<FJsonObject> FSeuratModule::Capture(FRotator Orientation, FVector Position)
{
	// Setup the camera.
	ColorCameraActor->SetActorLocation(Position);
	ColorCameraActor->SetActorRotation(Orientation);

	// Note that if bCaptureEveryFrame is true and the game is not paused by any means,
	// then this function call is redundant. However this is intentional since there are
	// several ways by which you can pause the game time, thus "Capture Every Frame" won't
	// work and it would rely on these calls to capture properly. Also these calls are
	// considered thread safe since they would resolve any CaptureSceneDeferred() before
	// enqueue this CaptureScene() command.
	ColorCamera->CaptureScene();

	int32 InResolution = static_cast<int32>(ColorCameraActor->Resolution);
	int32 Resolution = InResolution == 13 ? 1536 : FGenericPlatformMath::Pow(2, InResolution);

	// Note, this matrix won't necessarily match Unreal's projection matrix. It
	// doesn't matter in this case, because the Unreal plug captures eye space Z,
	// so it doesn't need to decode from window space. However, the matrix informs
	// Seurat of the near clip plane, and that the far clip is infinite. This also
	// allows Seurat to do some window space operations on the captured points.
	// This matrix is for a 90 degree frustum, infinite Z.
	float C = -1.0f;
	float D = -2 * GNearClippingPlane;
	FMatrix ClipFromEye = FMatrix(
		FPlane{ 1, 0, 0, 0 },
		FPlane{ 0, 1, 0, 0 },
		FPlane{ 0, 0, C, C },
		FPlane{ 0, 0, D, 0 });
	// This camera matrix stores this sample location's transformation, as opposed
	// to the reference camera transform stored in
	// WorldFromReferenceCameraMatrixSeurat.
	FMatrix WorldFromEyeSampleCameraUnreal = ColorCameraActor->GetTransform().ToMatrixNoScale();

	const FMatrix WorldFromEyeSeurat = SeuratMatrixFromUnrealMatrix(
		WorldFromEyeSampleCameraUnreal);
	// This line has two operations. First, inverting the world-from-eye matrix
	// converts the Seurat camera pose matrix to the eye-from-world matrix.
	//
	// Second, the pre-concatentation of WorldFromReferenceCameraMatrixSeurat's
	// matrix shifts all the camera sample matrices to have the eye coordinates of
	// the reference camera at the origin. That is, points in world coordinates
	// (GetTransform returns actor transforms in world coordinates) near the
	// origin transform to around the reference camera position under this matrix.
	//
	// Finally, regarding the order of the matrices, note that Unreal's
	// math library orders transformations left-to-right, so the first
	// transformation is on the left, followed by the second transformation on the
	// right.
	const FMatrix EyeFromWorldSeurat = WorldFromReferenceCameraMatrixSeurat * WorldFromEyeSeurat.Inverse();

	SeuratView MyView;

	MyView.ProjectiveCamera.ImageWidth = Resolution;
	MyView.ProjectiveCamera.ImageHeight = Resolution;
	MyView.ProjectiveCamera.ClipFromEyeMatrix = ClipFromEye;
	MyView.ProjectiveCamera.WorldFromEyeMatrix = EyeFromWorldSeurat.Inverse();
	MyView.ProjectiveCamera.DepthType = "EYE_Z";
	MyView.DepthImageFile.Color.Path = BaseImageName + "_ColorDepth.exr";
	MyView.DepthImageFile.Color.Channel0 = "R";
	MyView.DepthImageFile.Color.Channel1 = "G";
	MyView.DepthImageFile.Color.Channel2 = "B";
	MyView.DepthImageFile.Color.ChannelAlpha = "CONSTANT_ONE";
	MyView.DepthImageFile.Depth.Path = BaseImageName + "_ColorDepth.exr";
	MyView.DepthImageFile.Depth.Channel0 = "A";

	return MyView.ToJson();
}

void FSeuratModule::WriteImage(UTextureRenderTarget2D* InRenderTarget, FString Filename, bool bClearAlpha)
{
	FTextureRenderTargetResource* RTResource = InRenderTarget->GameThread_GetRenderTargetResource();

	// We're reading back depth in centimeters from alpha, and linear lighting.
	const ERangeCompressionMode kDontRangeCompress = RCM_MinMax;
	FReadSurfaceDataFlags ReadPixelFlags(kDontRangeCompress);
	// We always want linear output.
	ReadPixelFlags.SetLinearToGamma(false);

	TArray<FLinearColor> OutBMP;
	RTResource->ReadLinearColorPixels(OutBMP, ReadPixelFlags);

	FIntRect SourceRect;

	FIntPoint DestSize(InRenderTarget->GetSurfaceWidth(), InRenderTarget->GetSurfaceHeight());

	FString ResultPath;
	FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
	HighResScreenshotConfig.bCaptureHDR = true;
	HighResScreenshotConfig.SaveImage(Filename, OutBMP, DestSize);
}

bool FSeuratModule::SaveStringTextToFile(FString SaveDirectory, FString FileName, FString SaveText, bool AllowOverWriting)
{
	IFileManager* FileManager = &IFileManager::Get();

	if (!FileManager) return false;

	// Dir Exists?
	if (!FileManager->DirectoryExists(*SaveDirectory))
	{
		// Create directory if it not exist.
		FileManager->MakeDirectory(*SaveDirectory);

		// Still could not make directory?
		if (!FileManager->DirectoryExists(*SaveDirectory))
		{
			// Could not make the specified directory.
			return false;
		}
	}

	// Get complete file path.
	SaveDirectory /= FileName;

	// No over-writing?
	if (!AllowOverWriting)
	{
		// Check if file exists already.
		if (FileManager->GetFileAgeSeconds(*SaveDirectory) > 0)
		{
			// No overwriting.
			return false;
		}
	}

	return FFileHelper::SaveStringToFile(SaveText, *SaveDirectory);
}

void FSeuratModule::GenerateJson(TSharedPtr<FJsonObject> JsonObject, FString ExportPath)
{
	FString OutputString;
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > Writer = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	if (!SaveStringTextToFile(ExportPath, "manifest.json", OutputString, true))
		UE_LOG(Seurat, Error, TEXT("Saving json to file failed"));
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(Seurat);
IMPLEMENT_MODULE(FSeuratModule, Seurat)