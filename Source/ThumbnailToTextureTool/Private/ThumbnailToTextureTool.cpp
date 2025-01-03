// Credits please, open source from NanceDevDiaries. Game on!

#include "CustomBlueprintRenderer.h"
#include "CanvasTypes.h"
#include "IThumbnailToTextureTool.h"

#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ObjectTools.h"
#include "ThumbnailToTextureSettings.h"
#include "UnrealEdGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CustomSkeletalMeshThumbnailRenderer.h"
#include "CustomStaticMeshThumbnailRenderer.h"
#include "ThumbnailRendering/BlueprintThumbnailRenderer.h"
#include "ThumbnailRendering/SkeletalMeshThumbnailRenderer.h"
#include "ThumbnailRendering/StaticMeshThumbnailRenderer.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"

#define LOCTEXT_NAMESPACE "FThumbnailToTextureToolModule"

/**
 * Implements the FThumbnailToTextureToolModule module.
 */
class FThumbnailToTextureToolModule
	: public IThumbnailToTextureToolModule
{
public:
	FThumbnailToTextureToolModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	/** Returns whether the Header View supports the given class */
	static bool DoesAssetSupportExportToThumbnail(const FAssetData& AssetData);

	UCustomBlueprintRenderer* BlueprintThumbnailRenderer;
	UCustomStaticMeshThumbnailRenderer* StaticMeshThumbnailRenderer;
	UCustomSkeletalMeshThumbnailRenderer* SkeletalMeshThumbnailRenderer;

protected:
	virtual UThumbnailToTextureSettings* GetEditorSettingsInstance() const override;
	virtual UCustomBlueprintRenderer* GetCustomBlueprintThumbnailRendererInstance() override;
	virtual UCustomStaticMeshThumbnailRenderer* GetCustomStaticMeshThumbnailRendererInstance() override;
	virtual UCustomSkeletalMeshThumbnailRenderer* GetCustomSkeletalMeshThumbnailRendererInstance() override;

private:
	void AddContentBrowserContextMenuExtender();
	void RemoveContentBrowserContextMenuExtender();

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	static void ExecuteSaveThumbnailAsTexture(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);

	FDelegateHandle ContentBrowserExtenderDelegateHandle;

	void CreateThumbnailSettings();

private:
	UThumbnailToTextureSettings* ThumbnailToTextureEditorSettings;
};


FThumbnailToTextureToolModule::FThumbnailToTextureToolModule(): BlueprintThumbnailRenderer(nullptr),
                                                                StaticMeshThumbnailRenderer(nullptr),
                                                                SkeletalMeshThumbnailRenderer(nullptr)
{
	ThumbnailToTextureEditorSettings = nullptr;
}

void FThumbnailToTextureToolModule::StartupModule()
{
	BlueprintThumbnailRenderer = NewObject<UCustomBlueprintRenderer>(GetTransientPackage(),
	                                                                 UCustomBlueprintRenderer::StaticClass());
	check(BlueprintThumbnailRenderer);
	BlueprintThumbnailRenderer->AddToRoot();

	StaticMeshThumbnailRenderer = NewObject<UCustomStaticMeshThumbnailRenderer>(
		GetTransientPackage(), UCustomStaticMeshThumbnailRenderer::StaticClass());
	check(StaticMeshThumbnailRenderer);
	StaticMeshThumbnailRenderer->AddToRoot();

	SkeletalMeshThumbnailRenderer = NewObject<UCustomSkeletalMeshThumbnailRenderer>(
		GetTransientPackage(), UCustomSkeletalMeshThumbnailRenderer::StaticClass());
	check(SkeletalMeshThumbnailRenderer);
	SkeletalMeshThumbnailRenderer->AddToRoot();

	CreateThumbnailSettings();
	AddContentBrowserContextMenuExtender();
}

void FThumbnailToTextureToolModule::ShutdownModule()
{
	RemoveContentBrowserContextMenuExtender();

	// Unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "TextureToTextureTool");
	}

	if (!GExitPurge) // If GExitPurge Object is already gone
	{
		ThumbnailToTextureEditorSettings->RemoveFromRoot();
		BlueprintThumbnailRenderer->RemoveFromRoot();
		StaticMeshThumbnailRenderer->RemoveFromRoot();
		SkeletalMeshThumbnailRenderer->RemoveFromRoot();
	}
	ThumbnailToTextureEditorSettings = nullptr;
	BlueprintThumbnailRenderer = nullptr;
	StaticMeshThumbnailRenderer = nullptr;
	SkeletalMeshThumbnailRenderer = nullptr;
}

bool FThumbnailToTextureToolModule::DoesAssetSupportExportToThumbnail(const FAssetData& AssetData)
{
	// #TODO (NanceDevDiaries) add more support as it comes. Example, SkeletalMesh once it's figured
	// The logic of its saved render data
	const FName AssetName = AssetData.AssetClassPath.GetAssetName();
	return AssetName == TEXT("StaticMesh")
		|| AssetName == TEXT("Blueprint");
}

UThumbnailToTextureSettings* FThumbnailToTextureToolModule::GetEditorSettingsInstance() const
{
	return ThumbnailToTextureEditorSettings;
}

UCustomBlueprintRenderer* FThumbnailToTextureToolModule::GetCustomBlueprintThumbnailRendererInstance()
{
	return BlueprintThumbnailRenderer;
}

UCustomStaticMeshThumbnailRenderer* FThumbnailToTextureToolModule::GetCustomStaticMeshThumbnailRendererInstance()
{
	return StaticMeshThumbnailRenderer;
}

UCustomSkeletalMeshThumbnailRenderer* FThumbnailToTextureToolModule::GetCustomSkeletalMeshThumbnailRendererInstance()
{
	return SkeletalMeshThumbnailRenderer;
}

void FThumbnailToTextureToolModule::AddContentBrowserContextMenuExtender()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(
		"ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.
		GetAllAssetViewContextMenuExtenders();

	CBMenuExtenderDelegates.Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateStatic(
			&FThumbnailToTextureToolModule::OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FThumbnailToTextureToolModule::RemoveContentBrowserContextMenuExtender()
{
	if (ContentBrowserExtenderDelegateHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(
			"ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.
			GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
		{
			return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle;
		});
	}
}

TSharedRef<FExtender> FThumbnailToTextureToolModule::OnExtendContentBrowserAssetSelectionMenu(
	const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	// #todo filter out to only have assets that have a thumbnail already (Actors, not dataTables/dataAssets etc)

	Extender = MakeShared<FExtender>();

	if (DoesAssetSupportExportToThumbnail(SelectedAssets[0]))
	{
		Extender->AddMenuExtension(
			"CommonAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&ExecuteSaveThumbnailAsTexture, SelectedAssets)
		);
	}

	return Extender;
}

void FThumbnailToTextureToolModule::ExecuteSaveThumbnailAsTexture(FMenuBuilder& MenuBuilder,
                                                                  const TArray<FAssetData> SelectedAssets)
{
	// developed from engine code and help from a mix of https://forums.unrealengine.com/t/copy-asset-thumbnail-to-new-texture2d/138054/4
	// and https://isaratech.com/save-a-procedurally-generated-texture-as-a-new-asset/
	// and https://arrowinmyknee.com/2020/08/28/asset-right-click-menu-in-ue4/
	// and https://dev.epicgames.com/community/snippets/lw1/procedural-texture-with-c
	// and https://forums.unrealengine.com/t/programatically-created-asset-fails-to-save-or-crashes-the-editor/724517
	MenuBuilder.BeginSection("CreateTextureOffThumbnail", LOCTEXT("CreateTextureOffThumbnailMenuHeading", "Thumbnail"));
	{
		// Add Menu Entry Here
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Thumbnail_NewTexture", "Export to Texture"),
			LOCTEXT("Thumbnail_NewTextureTooltip",
			        "Will export asset's thumbnail and put it in a folder defined in the project settings"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
			{
				for (const FAssetData& AssetData : SelectedAssets)
				{
					if (!DoesAssetSupportExportToThumbnail(AssetData))
					{
						// Skip unsupported class
						continue;
					}
					
					FString GamePath = AssetData.GetAsset()->GetPathName();
					FString AssetName;
					if (int32 PathEnd; GamePath.FindLastChar('/', PathEnd))
					{
						++PathEnd;
						AssetName += GamePath;
						AssetName.RightChopInline(PathEnd);
						int32 extensionIdx;
						if (AssetName.FindChar('.', extensionIdx))
						{
							AssetName.LeftInline(extensionIdx);
						}
						GamePath.LeftInline(PathEnd);
						FString Prefix = GetEditorSettings().ThumbnailPrefix;
						FString NameWithPrefix = Prefix + AssetName;
						AssetName = NameWithPrefix;
					}
					else
					{
						AssetName = "T_Thumbnail";
					}

					if (int32 PathSeparatorIdx; AssetName.FindChar('/', PathSeparatorIdx))
					{
						// TextureName should not have any path separators in it
						return;
					}

					// Create the new texture 2D and save it on disk
					FString PackageFilename;
					const FName ObjectFullName = FName(*AssetData.GetFullName());
					TSet<FName> ObjectFullNames;
					ObjectFullNames.Add(ObjectFullName);

					FString PackageName = GetEditorSettings().RootTexture2DSaveDir.Path;
					if (!PackageName.EndsWith("/"))
					{
						PackageName += "/";
					}
					PackageName += AssetName;

					UPackage* Package = CreatePackage(*PackageName);
					Package->FullyLoad();

					UTexture2D* NewTexture = NewObject<UTexture2D>(Package, *AssetName,
					                                               RF_Public | RF_Standalone);
					//NewTexture->AddToRoot();
					NewTexture->MarkPackageDirty();

					// Load the image from the asset's loaded Thumbnail
					if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &PackageFilename))
					{
						// TODO find out more why this might happen for skeletalMeshes
						bool FailedToFindRenderInfo = false;
						
						if (GetEditorSettings().UseTransparentBackground || GetEditorSettings().
							UseCustomBackgroundMaterial)
						{
							// Set the size of cached thumbnails
							constexpr int32 ImageWidth = ThumbnailTools::DefaultThumbnailSize;
							constexpr int32 ImageHeight = ThumbnailTools::DefaultThumbnailSize;

							FObjectThumbnail ObjectThumbnail;
							FObjectThumbnail* ObjectThumnailPtr = &ObjectThumbnail;

							UObject* Object = AssetData.FastGetAsset();
							if (Object && !IsValidChecked(Object))
							{
								Object = nullptr;
							}

							// Get the rendering info for this object
							FThumbnailRenderingInfo* RenderInfo = GUnrealEd
																	  ? GUnrealEd->GetThumbnailManager()->
																	  GetRenderingInfo(Object)
																	  : nullptr;
							if (RenderInfo)
							{
								ObjectThumbnail.SetImageSize(ImageWidth, ImageHeight);

								UTextureRenderTarget2D* RenderTargetTexture = NewObject<UTextureRenderTarget2D>();
								RenderTargetTexture->AddToRoot();
								check(RenderTargetTexture);
								RenderTargetTexture->ClearColor = FLinearColor::White;
								RenderTargetTexture->SRGB = 1;
								RenderTargetTexture->RenderTargetFormat = RTF_RGBA8;
								constexpr bool bForceLinearGamma = false;
								RenderTargetTexture->InitCustomFormat(ThumbnailTools::DefaultThumbnailSize,
																	  ThumbnailTools::DefaultThumbnailSize, PF_FloatRGBA,
																	  bForceLinearGamma);
								FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->
									GameThread_GetRenderTargetResource()->GetTextureRenderTarget2DResource();
								RenderTargetTexture->UpdateResourceImmediate(true);

								// Create a canvas for the render target and clear it to black
								FCanvas Canvas(RenderTargetResource, nullptr, FGameTime::GetTimeSinceAppStart(),
											   GMaxRHIFeatureLevel);
								Canvas.Clear(FLinearColor::Black);
								
								constexpr int32 XPos = 0;
								constexpr int32 YPos = 0;
								constexpr bool bAdditionalViewFamily = false;

								if (RenderInfo->Renderer->IsA(UBlueprintThumbnailRenderer::StaticClass()))
								{
									// Draw the thumbnail
									GetCustomBlueprintThumbnailRenderer().Draw(Object,
																			   XPos,
																			   YPos,
																			   ImageWidth,
																			   ImageHeight,
																			   RenderTargetResource,
																			   &Canvas,
																			   bAdditionalViewFamily
									);
								}
								else if (RenderInfo->Renderer->IsA(UStaticMeshThumbnailRenderer::StaticClass()))
								{
									GetCustomStaticMeshThumbnailRenderer().Draw(Object,
										XPos,
										YPos,
										ImageWidth,
										ImageHeight,
										RenderTargetResource,
										&Canvas,
										bAdditionalViewFamily
									);
								}
								else if (RenderInfo->Renderer->IsA(USkeletalMeshThumbnailRenderer::StaticClass()))
								{
									GetCustomSkeletalMeshThumbnailRenderer().Draw(Object,
																				  XPos,
																				  YPos,
																				  ImageWidth,
																				  ImageHeight,
																				  RenderTargetResource,
																				  &Canvas,
																				  bAdditionalViewFamily
									);
								}
								else
								{
									RenderInfo->Renderer->Draw(
										Object,
										XPos,
										YPos,
										ImageWidth,
										ImageHeight,
										RenderTargetResource,
										&Canvas,
										bAdditionalViewFamily
									);
								}

								// If this object's thumbnail will be rendered to a texture on the GPU.
								bool bUseGPUGeneratedThumbnail = true;

								// GPU based thumbnail rendering only
								if (bUseGPUGeneratedThumbnail)
								{
									// Tell the rendering thread to draw any remaining batched elements
									Canvas.Flush_GameThread();
									{
										ENQUEUE_RENDER_COMMAND(UpdateThumbnailRTCommand)(
											[RenderTargetResource](FRHICommandListImmediate& RHICmdList)
											{
												TransitionAndCopyTexture(
													RHICmdList, RenderTargetResource->GetRenderTargetTexture(),
													RenderTargetResource->TextureRHI, {});
											});

										if (ObjectThumnailPtr)
										{
											const FIntRect InSrcRect(0, 0, ObjectThumnailPtr->GetImageWidth(),
																	 ObjectThumnailPtr->GetImageHeight());

											TArray<uint8>& OutData = ObjectThumnailPtr->AccessImageData();

											OutData.Empty();
											OutData.AddUninitialized(
												ObjectThumnailPtr->GetImageWidth() * ObjectThumnailPtr->GetImageHeight() *
												sizeof(FColor));

											// Copy the contents of the remote texture to system memory
											// NOTE: OutRawImageData must be a preallocated buffer!
											RenderTargetResource->ReadPixelsPtr(
												(FColor*)OutData.GetData(), FReadSurfaceDataFlags(), InSrcRect);
										}
									}
								}

								int32 SizeX = ObjectThumbnail.GetImageWidth();
								int32 SizeY = ObjectThumbnail.GetImageHeight();

								FTexturePlatformData* platformData = new FTexturePlatformData();
								platformData->SizeX = SizeX;
								platformData->SizeY = SizeY;
								platformData->SetNumSlices(1);
								platformData->PixelFormat = PF_FloatRGBA;
								NewTexture->SetPlatformData(platformData);
								NewTexture->MipGenSettings = TMGS_NoMipmaps;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 2
								FTexture2DMipMap* Mip = new FTexture2DMipMap();
								Mip->SizeX = SizeX;
								Mip->SizeY = SizeY;
								Mip->SizeZ = 1;
#else
								FTexture2DMipMap* Mip = new FTexture2DMipMap(SizeX, SizeY, 1);
#endif
								NewTexture->GetPlatformData()->Mips.Add(Mip);

								// Lock the texture so it can be modified
								Mip->BulkData.Lock(LOCK_READ_WRITE);
								uint8* TextureData = Mip->BulkData.Realloc(ImageWidth * ImageHeight * sizeof(FColor));

								const TArray<uint8>& OldBytes = ObjectThumnailPtr->GetUncompressedImageData();

								TArray<FColor> OldColors;
								OldColors.SetNumUninitialized(OldBytes.Num() / sizeof(FColor));
								FMemory::Memcpy(OldColors.GetData(), OldBytes.GetData(), OldBytes.Num());

								if (GetEditorSettings().UseTransparentBackground)
								{
									FLinearColor TransparentColor;
									GetEditorSettings().TranslucentMaterial->GetVectorParameterValue(
										TEXT("Color"), TransparentColor);

									for (FColor& OldColor : OldColors)
									{
										FLinearColor OldLinearColor = OldColor.ReinterpretAsLinear();
										if (OldLinearColor.Equals(TransparentColor,
																  GetEditorSettings().BackgroundCutoffThreshold))
										{
											OldColor.A = 0;
										}
									}
								}

								for (uint32 Y = 0; Y < ImageHeight; Y++)
								{
									uint64 Index = (ImageHeight - 1 - Y) * ImageWidth * sizeof(FColor);
									uint8* DestPtr = &TextureData[Index];

									FMemory::Memcpy(DestPtr, reinterpret_cast<const uint8*>(OldColors.GetData()) + Index,
													ImageWidth * sizeof(FColor));
								}
								Mip->BulkData.Unlock();

								NewTexture->Source.Init(SizeX, SizeY, 1,
														1,
														TSF_BGRA8, reinterpret_cast<const uint8*>(OldColors.GetData()));

								NewTexture->SRGB = false;
								NewTexture->MipGenSettings = TMGS_FromTextureGroup;
								NewTexture->LODGroup = TEXTUREGROUP_UI; // Prepare the asset for UI use
								NewTexture->CompressionSettings = TC_EditorIcon; // UI setting
								NewTexture->DeferCompression = true;
								NewTexture->PostEditChange();
								NewTexture->UpdateResource();
								//Package->MarkPackageDirty();

								Package->FullyLoad();
								Package->SetDirtyFlag(true);
								FEditorFileUtils::PromptForCheckoutAndSave({Package}, false, false);
								FAssetRegistryModule::AssetCreated(NewTexture);

								RenderTargetTexture->RemoveFromRoot();
							}
							else
							{
								FailedToFindRenderInfo = true;
							}
						}
						
						if ((!GetEditorSettings().UseTransparentBackground && !GetEditorSettings().
                             							UseCustomBackgroundMaterial ) || FailedToFindRenderInfo) // use the existing thumbnail
						{
							FThumbnailMap ThumbnailMap;
							ThumbnailTools::LoadThumbnailsFromPackage(PackageFilename, ObjectFullNames,
							                                          ThumbnailMap);

							FObjectThumbnail* ObjectThumbnail = ThumbnailMap.Find(ObjectFullName);


							if (!ObjectThumbnail)
							{
								return;
							}

							int32 SizeX = ObjectThumbnail->GetImageWidth();
							int32 SizeY = ObjectThumbnail->GetImageHeight();

							FTexturePlatformData* platformData = new FTexturePlatformData();
							platformData->SizeX = SizeX;
							platformData->SizeY = SizeY;
							platformData->SetNumSlices(1);
							platformData->PixelFormat = PF_B8G8R8A8;
							NewTexture->SetPlatformData(platformData);
							NewTexture->MipGenSettings = TMGS_NoMipmaps;

							int32 NumBlocksX = ObjectThumbnail->GetImageWidth() / GPixelFormats[PF_B8G8R8A8].BlockSizeX;
							int32 NumBlocksY = ObjectThumbnail->GetImageHeight() / GPixelFormats[PF_B8G8R8A8].
								BlockSizeY;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 2
							FTexture2DMipMap* Mip = new FTexture2DMipMap();
							Mip->SizeX = SizeX;
							Mip->SizeY = SizeY;
							Mip->SizeZ = 1;
#else
							FTexture2DMipMap* Mip = new FTexture2DMipMap(SizeX, SizeY, 1);
#endif
							NewTexture->GetPlatformData()->Mips.Add(Mip);
							Mip->BulkData.Lock(LOCK_READ_WRITE);
							Mip->BulkData.Realloc(
								static_cast<int64>(NumBlocksX) * NumBlocksY * GPixelFormats[PF_B8G8R8A8].BlockBytes);
							Mip->BulkData.Unlock();

							NewTexture->UpdateResource();

							NewTexture->Source.Init(SizeX, SizeY, 1,
							                        1,
							                        TSF_BGRA8, ObjectThumbnail->GetUncompressedImageData().GetData());
							NewTexture->LODGroup = TEXTUREGROUP_UI; // Prepare the asset for UI use
							NewTexture->CompressionSettings = TC_Default;
							// No need for "UserInterface2D", no need for alpha, it was also having issues making the asset have a thumbnail itself
							NewTexture->NeverStream = true;
							NewTexture->CompressionNoAlpha = true;

							NewTexture->UpdateResource();
							Package->FullyLoad();
							Package->SetDirtyFlag(true);
							FEditorFileUtils::PromptForCheckoutAndSave({Package}, false, false);
							FAssetRegistryModule::AssetCreated(NewTexture);
						}
					}
				}
			})),
			NAME_None,
			EUserInterfaceActionType::Button);
	}
	MenuBuilder.EndSection();
}

void FThumbnailToTextureToolModule::CreateThumbnailSettings()
{
	ThumbnailToTextureEditorSettings = NewObject<UThumbnailToTextureSettings>(
		GetTransientPackage(), UThumbnailToTextureSettings::StaticClass());
	check(ThumbnailToTextureEditorSettings);
	ThumbnailToTextureEditorSettings->AddToRoot();

	// Register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr EditorSettingsSection = SettingsModule->RegisterSettings(
			"Project", "Plugins", "TextureToTextureEditor",
			NSLOCTEXT("ThumbnailToTextureTool", "ThumbnailToTextureSettingsName",
			          "Thumbnail To Texture Editor defaults"),
			NSLOCTEXT("ThumbnailToTextureTool", "ThumbnailToTextureSettingsDescription",
			          "Configure Thumbnail To Texture Editor defaults."),
			ThumbnailToTextureEditorSettings);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FThumbnailToTextureToolModule, ThumbnailToTextureTool)
