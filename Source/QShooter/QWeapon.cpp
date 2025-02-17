// Fill out your copyright notice in the Description page of Project Settings.


#include "QWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "QShooterCharacter.h"

AQWeapon::AQWeapon()
{

}

void AQWeapon::IncreaseAmmo(int32 ammoDelta)
{
	ensureMsgf(ammoDelta >= 0 && ammoDelta <= MagazineCapcity, TEXT("ammoDelta is not legal"));
	AmmoAmount += ammoDelta;
}

void AQWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#pragma region ReadParamFromDataTable
	const FString tablePath(TEXT("DataTable'/Game/_Game/DataTable/DT_Weapon.DT_Weapon'"));
	UDataTable* weaponTable = LoadObject<UDataTable>(nullptr, *tablePath);
	if (weaponTable)
	{
		FWeaponDataTableRow* tableRow = nullptr;
		switch (WeaponType)
		{
		case EWeaponType::EWT_SubmachineGun:
			tableRow = weaponTable->FindRow<FWeaponDataTableRow>(TEXT("SubmachineGun"), TEXT(""));
			break;
		case EWeaponType::EWT_AssaultRifle:
			tableRow = weaponTable->FindRow<FWeaponDataTableRow>(TEXT("AssaultRifle"), TEXT(""));
			break;
		case EWeaponType::EWT_Pistol:
			tableRow = weaponTable->FindRow<FWeaponDataTableRow>(TEXT("Pistol"), TEXT(""));
			break;
		default:
			UE_LOG(LogTemp, Error, TEXT("weapon %s has a illegal type"), *GetName());
			break;
		}

		if (tableRow)
		{
			AmmoType = tableRow->AmmoType;
			AmmoAmount = tableRow->WeaponAmmo;
			MagazineCapcity = tableRow->MagazineCapcity;
			SetPickupSound(tableRow->PickupSound);
			SetEquipSound(tableRow->EquipSound);
			ItemMeshComponent->SetSkeletalMesh(tableRow->ItemMesh);
			SetItemInventoryIcon(tableRow->InventoryIcon);
			AmmoIcon = tableRow->AmmoIcon;
			SetItemName(tableRow->ItemName);
			ClipName = tableRow->ClipName;
			Reload_AM_SectionName = tableRow->ReloadAnimMontageSection;

			ItemMeshComponent->SetAnimInstanceClass(tableRow->AnimBlueprintClass);

			//Crosshair textures
			CrosshairMiddleTex = tableRow->CrosshairMiddleTex;
			CrosshairRightTex = tableRow->CrosshairRightTex;
			CrosshairLeftTex = tableRow->CrosshairLeftTex;
			CrosshairUpTex = tableRow->CrosshairUpTex;
			CrosshairBottomTex = tableRow->CrosshairBottomTex;

			AutoFireRate = tableRow->AutoFireRate;
			MuzzleFlash = tableRow->MuzzleFlash;
			FireSound = tableRow->FireSound;
			BoneToHide = tableRow->BoneToHide;

			SetItemMaterial(tableRow->MaterialInstance);
			SetItemMaterialIndex(tableRow->MaterialIndex);

			bIsAutomatic = tableRow->IsAutomatic;

			Damage = tableRow->Damage;
			HeadshotDamage = tableRow->HeadshotDamage;
		}
	}
#pragma endregion

}

void AQWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateSlideDisplacement();
}

void AQWeapon::FireOneBullet()
{
	if (AmmoAmount <= 0)
	{
		return;
	}
	SetAmmoAmount(AmmoAmount- 1);
	//bIsBulletFiring = true;
	if (IsSlideMovable())
	{
		bIsMovingSlide = true;
		GetWorldTimerManager().SetTimer(MoveSlideTimerHandle, this, &AQWeapon::FinishSlide, SlideMoveDuration, false);
	}
}

bool AQWeapon::IsClipFull() const
{
	return AmmoAmount == MagazineCapcity;
}

UTexture2D* AQWeapon::GetAmmoIconTexture()
{
	return AmmoIcon;
}

void AQWeapon::BeginPlay()
{
	Super::BeginPlay();

	ensureMsgf(AmmoAmount <= MagazineCapcity, TEXT("AmmoAmount must be <= MagazineCapcity"));

	//NAME_None
	if (BoneToHide != FName(""))
	{
		ItemMeshComponent->HideBoneByName(BoneToHide, EPhysBodyOp::PBO_None);
	}
}

void AQWeapon::SetToEquipped(AQShooterCharacter* player)
{
	ensureMsgf(player, TEXT("Playe should not be null "));
	PlayerEuipThis = player;

	ConfigItemState(EQItemState::EIS_Equipped);

	if (USoundCue* euipSound = GetEquipSound())
	{
		UGameplayStatics::PlaySound2D(this, euipSound);
	}
}

void AQWeapon::ThrowItem()
{
	// 1. 计算一个impulse direction, 预期方向是武器前向，左右偏一点
	FVector impulseDirection = FVector::ForwardVector;
#pragma region CalImpulseDirection
	FRotator weaponRoation(0.0f, ItemMeshComponent->GetComponentRotation().Yaw, 0.0f);
	//FVector weaponFoward = 
	ItemMeshComponent->SetWorldRotation(weaponRoation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector itemRightDir = ItemMeshComponent->GetRightVector();
	const FVector itemForward = ItemMeshComponent->GetForwardVector();

	//这里用mesh 的right Dir 是因为当前的枪坐标有些不对，不是枪的前向对着X轴
	impulseDirection = itemRightDir.RotateAngleAxis(FMath::RandRange(10.0f, 30.f), ItemMeshComponent->GetUpVector());
	//impulseDirection = itemRightDir;
	impulseDirection *= 20000.0f;
#pragma endregion

	// 2. addimpulse
	ItemMeshComponent->AddImpulse(impulseDirection);
	bIsFalling = true;

	// 3. 设置timer 以stop
	GetWorld()->GetTimerManager().SetTimer(FallingStopTimerHandle, this, &AQWeapon::StopFalling, FallingDuration);
}

bool AQWeapon::HasAmmo()
{
	return AmmoAmount > 0;
}

void AQWeapon::StopFalling()
{
	bIsFalling = false;
	ConfigItemState(EQItemState::EIS_ToPickUp);
}

void AQWeapon::FinishSlide()
{
	UE_LOG(LogTemp, Warning, TEXT("Finish move slide"));
	bIsMovingSlide = false;
}

bool AQWeapon::IsSlideMovable() const
{
	return WeaponType == EWeaponType::EWT_Pistol;
}

void AQWeapon::UpdateSlideDisplacement()
{
	if (IsSlideMovable() && bIsMovingSlide && SlideDisplacmentCurve)
	{
		const float elapsedTime = GetWorldTimerManager().GetTimerElapsed(MoveSlideTimerHandle);
		const float curveValue = SlideDisplacmentCurve->GetFloatValue(elapsedTime);
		SlideDisplacement = curveValue * SlideDisplacementMAX;
		RecoilRotation = curveValue * RecoilRotationMAX;
	}
}
