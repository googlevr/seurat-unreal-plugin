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

#include "SeuratConfigWindow.h"

#define LOCTEXT_NAMESPACE "FSeuratModule"

float ProcessInput(const FText& InText, TSharedPtr<SEditableTextBox> TextBox, float PrevValue)
{
	FString InputText = InText.ToString();
	if (!InputText.IsNumeric())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Invalid input", "Input must be numeric"));
		TextBox->SetText(FText::AsNumber(PrevValue));
		return PrevValue;
	}
	return FCString::Atof(*InputText);;
}

void SSeuratConfigWindow::Construct(const FArguments& InArgs, ASceneCaptureSeurat* InOwner)
{
	Owner = InOwner;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(LOCTEXT("Capture", "Capture"))
			.ToolTipText(LOCTEXT("Capture", "Capture"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SSeuratConfigWindow::Capture)
		]
	];
}

FReply SSeuratConfigWindow::Capture()
{
	FSeuratModule* SeuratModule = FModuleManager::GetModulePtr<FSeuratModule>("Seurat");
	if (SeuratModule != nullptr)
	{
		SeuratModule->BeginCapture(Owner);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
