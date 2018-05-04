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

#include "SceneCaptureSeuratDetail.h"
#include "SeuratConfigWindow.h"
#include "PropertyEditorModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SceneCaptureSeuratDetails"

TSharedRef<IDetailCustomization> FSceneCaptureSeuratDetail::MakeInstance()
{
	return MakeShareable(new FSceneCaptureSeuratDetail);
}

FSceneCaptureSeuratDetail::~FSceneCaptureSeuratDetail()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FSceneCaptureSeuratDetail::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray< TWeakObjectPtr<UObject> > OutObjects;
	DetailLayout.GetObjectsBeingCustomized(OutObjects);

	if (OutObjects.Num() == 0)
	{
		return;
	}

	ASceneCaptureSeurat* CaptureCameraActor = Cast<ASceneCaptureSeurat>(OutObjects[0].Get());

	if (CaptureCameraActor == nullptr)
	{
		UE_LOG(Seurat, Error, TEXT("Detail panel cannot find the corresponding actor of SceneCaptureSeurat."));
		return;
	}

	IDetailCategoryBuilder& BrushBuilderCategory = DetailLayout.EditCategory("SeuratSettings", FText::GetEmpty(), ECategoryPriority::Important);
	BrushBuilderCategory.AddCustomRow(FText::GetEmpty(), true)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(1.0f)
		[
			SNew(SSeuratConfigWindow, CaptureCameraActor)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE