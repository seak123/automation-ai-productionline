// 生产能力（GAS）：激活即"干活"，结束时提交产出/消耗的 GameplayEffect。
// 产量、耗材、冷却全部作为数据交给策划调。参照实现，不含任何专有源码。
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Produce.generated.h"

/**
 * 生产的实际效果是一条 GameplayAbility：
 *   激活 -> 播放/驱动干活表现 -> 服务器权威结算 -> ApplyGameplayEffect（+产物 / -耗材 / 起冷却）
 *
 * 因为生产由 GAS 驱动，同一套工作单元 AI 只要换工单上的 AbilityTag，
 * 就能驱动磨坊、熔炉、孵化器……任意站点 —— 这就是四系统解耦的收益。
 */
UCLASS()
class UGA_Produce : public UGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

protected:
	// 产出/消耗/冷却效果，由策划在数据里配
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	TSubclassOf<class UGameplayEffect> ProduceEffect;

	// 干活时长由动画蒙太奇驱动；结束回调里提交结算并 EndAbility
	void OnWorkMontageFinished();
};
