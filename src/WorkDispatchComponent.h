// Automation AI — illustrative clean-room reference (no proprietary source).
// Station-side work dispatch described in the README.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WorkDispatchComponent.generated.h"

UENUM() enum class EWorkState  : uint8 { Pending, Assigned, Done };
UENUM() enum class EWorkResult : uint8 { Succeeded, Failed };

// A single unit of work. Carries WHERE and WHAT (as a GAS tag) — never HOW.
USTRUCT()
struct FWorkOrder
{
    GENERATED_BODY()

    UPROPERTY() FGuid            Id;
    UPROPERTY() FVector          WorkSiteLocation = FVector::ZeroVector;
    UPROPERTY() FGameplayTag     AbilityTag;          // e.g. Work.Mine / Work.Smelt
    UPROPERTY() EWorkState       State = EWorkState::Pending;

    FWorkOrder& Reset() { State = EWorkState::Pending; return *this; }
};

// Owns the queue and assigns orders to idle workers. Knows what needs doing, not how.
UCLASS()
class UWorkDispatchComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void TickDispatch();                                   // assign pending -> idle workers
    void OnWorkerReport(FGuid OrderId, EWorkResult Result);// re-queue on failure

private:
    TArray<TWeakObjectPtr<class AWorkerPawn>> RegisteredWorkers;
    TQueue<FWorkOrder>                        PendingOrders;
    TMap<FGuid, FWorkOrder>                   ActiveOrders;
};
