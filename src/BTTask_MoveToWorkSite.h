// 行为树任务：把工作单元导航到工单指定的工位。只负责"移动"这一件事。
// 参照实现，不含任何专有源码。
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToWorkSite.generated.h"

/**
 * MoveTo 任务：Navigation 的接缝。
 * ！！！健壮地处理"到不了"是关键 —— 到不了就 fast-fail，让派工把工单重新入队，
 * 否则一条产线会被一个卡住的工作单元拖死。
 */
UCLASS()
class UBTTask_MoveToWorkSite : public UBTTaskNode
{
	GENERATED_BODY()

public:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	// 到达判定半径：进入这个范围就算"到工位了"
	UPROPERTY(EditAnywhere, Category = "Automation")
	float Val_WorkReachRadius = 120.0f;

	// 异步移动完成回调里 FinishLatentTask
	uint32 WaitingMoveId = 0;
};
