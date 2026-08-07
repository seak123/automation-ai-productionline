// 派工逻辑实现。参照实现，不含任何专有源码。
#include "WorkDispatchComponent.h"
#include "WorkerPawn.h"

void UWorkDispatchComponent::TickDispatch()
{
	for (TWeakObjectPtr<AWorkerPawn> Worker : RegisteredWorkers)
	{
		if (!Worker.IsValid() || !Worker->IsIdle())
		{
			continue;
		}
		if (PendingOrders.IsEmpty())
		{
			break;
		}

		FWorkOrder Order;
		PendingOrders.Dequeue(Order);
		Order.State = EWorkState::Assigned;

		// 发出去就交给工作单元自己跑（寻路 + 干活），完成/失败时回报
		Worker->AssignOrder(Order);
		ActiveOrders.Add(Order.Id, Order);
	}
}

void UWorkDispatchComponent::OnWorkerReport(FGuid OrderId, EWorkResult Result)
{
	if (FWorkOrder* Order = ActiveOrders.Find(OrderId))
	{
		// 失败（路被堵、资源没了……）就重新入队，让生产线自愈而不是静默卡死
		if (Result == EWorkResult::Failed)
		{
			PendingOrders.Enqueue(Order->Reset());
		}
		ActiveOrders.Remove(OrderId);
	}
}
