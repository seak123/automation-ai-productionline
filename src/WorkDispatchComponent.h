// 生产线派工组件（站点侧）：维护工单队列，把工单派给空闲的 AI 工作单元。
// 参照实现，不含任何专有源码。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WorkDispatchComponent.generated.h"

UENUM() enum class EWorkState  : uint8 { Pending, Assigned, Done };
UENUM() enum class EWorkResult : uint8 { Succeeded, Failed };

/**
 * 一个工单：只携带"在哪"和"干什么"（用 GAS Tag 表达），不关心"怎么干"。
 * 这条边界让派工与工作单元的移动/动画彻底解耦。
 */
USTRUCT()
struct FWorkOrder
{
	GENERATED_BODY()

	UPROPERTY() FGuid        Id;
	UPROPERTY() FVector      WorkSiteLocation = FVector::ZeroVector; // 工位
	UPROPERTY() FGameplayTag AbilityTag;                            // 如 Work.Mine / Work.Smelt
	UPROPERTY() EWorkState   State = EWorkState::Pending;

	FWorkOrder& Reset() { State = EWorkState::Pending; return *this; }
};

/**
 * 派工组件：拥有工单队列，把 pending 工单发给空闲工作单元。
 * 它只知道"要做什么"，从不驱动工作单元的移动或表现。
 */
UCLASS(ClassGroup = (Automation), meta = (BlueprintSpawnableComponent))
class UWorkDispatchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 把 pending 工单派给空闲工作单元；每帧调用
	void TickDispatch();

	// 工作单元回报结果：失败则重新入队，保证没有工单被静默丢弃
	void OnWorkerReport(FGuid OrderId, EWorkResult Result);

	void EnqueueOrder(const FWorkOrder& Order) { PendingOrders.Enqueue(Order); }

private:
	TArray<TWeakObjectPtr<class AWorkerPawn>> RegisteredWorkers;
	TQueue<FWorkOrder>                        PendingOrders;
	TMap<FGuid, FWorkOrder>                    ActiveOrders;
};
