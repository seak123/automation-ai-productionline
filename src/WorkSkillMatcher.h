// 生产线派工的核心 gameplay：工作按"技能类型"分类，每个工作需要一定"人数"，
// 从空闲工作单元池按技能匹配指派；部分工作（搬运/采集）可连续进行。
// 参照实现，说明匹配玩法，不含任何专有源码。
#pragma once

#include "CoreMinimal.h"
#include "WorkSkillMatcher.generated.h"

// 工作技能类型：一个工作单元只能做它会的技能对应的工作
UENUM()
enum class EWorkSkill : uint8
{
	Fire,      // 点火 / 熔炼
	Handwork,  // 手工制作
	Water,     // 浇水
	Harvest,   // 采集（如采蜜）——通常是连续工作
	Haul,      // 搬运——连续工作
};

// 一个工作的需求人数与已指派情况
USTRUCT()
struct FHeadCount
{
	GENERATED_BODY()

	UPROPERTY() int32        Need = 1;         // 需要几名工作单元
	UPROPERTY() TArray<FGuid> AssignedWorkers;  // 已指派的

	bool FullyAssigned() const { return AssignedWorkers.Num() >= Need; }
};

// 一条待处理的工作
USTRUCT()
struct FWorkInfo
{
	GENERATED_BODY()

	UPROPERTY() FGuid       Id;
	UPROPERTY() EWorkSkill  Skill = EWorkSkill::Handwork;
	UPROPERTY() FVector     SiteLocation = FVector::ZeroVector;
	UPROPERTY() FHeadCount  HeadCount;
	UPROPERTY() bool        bContinuous = false; // 搬运/采集为 true：干完不空闲，接着找同类活
};

/**
 * 把空闲工作单元按技能匹配给待处理工作。
 * ！！！匹配以"工作"为主体循环：优先把还没配满人数的工作补齐；
 * 连续型工作（搬运/采集）允许同一单元干完接着干，减少反复寻路。
 */
UCLASS()
class UWorkSkillMatcher : public UObject
{
	GENERATED_BODY()

public:
	// 每轮匹配：遍历待处理工作，从对应技能的空闲池里补人
	void MatchOnce();

	void AddWork(const FWorkInfo& Work);
	void RegisterIdleWorker(const FGuid& WorkerGuid, EWorkSkill Skill);

protected:
	// 把某个空闲单元指派到某工作
	void AssignWorkerToWork(FWorkInfo& Work, const FGuid& WorkerGuid);

	// 取出某技能下一个可用的空闲单元
	bool PopAvailableWorker(EWorkSkill Skill, FGuid& OutWorker);

private:
	// 尚未配满人的工作，按技能归类
	TMap<EWorkSkill, TArray<FWorkInfo>> PendingBySkill;
	// 空闲工作单元池，按技能归类
	TMap<EWorkSkill, TArray<FGuid>>     IdleBySkill;
};

inline void UWorkSkillMatcher::MatchOnce()
{
	for (auto& Pair : PendingBySkill)
	{
		const EWorkSkill Skill = Pair.Key;
		for (FWorkInfo& Work : Pair.Value)
		{
			// 把这个工作的人数补满，或直到该技能没有空闲单元
			FGuid Worker;
			while (!Work.HeadCount.FullyAssigned() && PopAvailableWorker(Skill, Worker))
			{
				AssignWorkerToWork(Work, Worker);
			}
		}
	}
}
