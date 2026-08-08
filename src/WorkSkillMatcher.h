// 派工匹配 / Work assignment matching
//
// 生产线的核心 gameplay：工作按"技能类型"分类，每个工作有"需求人数"，
// 从空闲工作单元池中按技能匹配指派；部分工作可连续进行。
// The heart of the production line: work is typed by SKILL, each job carries a
// HEADCOUNT, and idle workers are matched from a per-skill pool. Some work is continuous.
//
// 参照实现，说明玩法机制，不含任何专有源码。
// Reference implementation illustrating the mechanic; no proprietary source.
#pragma once

#include "CoreMinimal.h"
#include "WorkSkillMatcher.generated.h"

/**
 * 工作技能类型 / Work skill types.
 * 一个工作单元只会做它拥有技能的工作——这是玩家"养什么生物、能自动化什么"的核心策略点。
 * A worker only takes work matching a skill it possesses — this is the core strategic
 * hook of "which creatures you raise determines what you can automate".
 */
UENUM()
enum class EWorkSkill : uint8
{
	Fire,       // 点火 / 熔炼 / lighting fires, smelting
	Handwork,   // 手工制作 / crafting at benches
	Water,      // 浇水 / watering crops
	Harvest,    // 采集（如采蜜）/ gathering, e.g. collecting honey
	Haul,       // 搬运 / hauling items
	Planting,   // 种植 / planting
	Logging,    // 伐木 / logging
	Mining,     // 采矿 / mining
};

/**
 * 工作目标类型 / Work target types.
 *
 * ！！！这是这套系统里最容易被低估的复杂点：目标根本不是同一种东西。
 * The most underestimated source of complexity here: targets are not one kind of thing.
 * 有的是 Actor（储物箱、矿石实体），有的是植被实例（野树、树桩），
 * Some are Actors (storage boxes, mineral entities), some are foliage instances
 * (wild trees, stumps),
 * 有的是纯数据的全局作物，有的甚至是"虚拟目标"（浇水任务并没有一个实体可走过去）。
 * some are pure-data global crops, and some are purely VIRTUAL — a watering job has
 * no physical entity to walk up to at all.
 * 因此目标必须抽象成统一的"可定位 + 可校验"接口，AI 才能一视同仁地处理。
 * Targets therefore have to be abstracted behind one "locatable + validatable"
 * interface so the AI can treat them uniformly.
 */
UENUM()
enum class EWorkTargetType : uint8
{
	None = 0,
	NewPlant,          // 待种植的坑位 / a planting slot
	DroppedItem,       // 地上的掉落物 / a dropped item entity
	VirtualDrop,       // 虚拟掉落物（未实体化）/ a virtual drop, not yet spawned
	WorkBuild,         // 生产线部件 / a production-line station
	FoliageInstance,   // 植被实例（野树等）/ a foliage instance such as a wild tree
	GlobalCrop,        // 全局作物（纯数据）/ a global crop, pure data
	BuildPiece,        // 建筑物件 / a build piece
	ContainerBox,      // 储物箱 / a storage container
	WateringVirtual,   // 浇水专用虚拟目标 / virtual target used only by watering jobs
	RollingLog,        // 滚木 / a rolling log
	Stump,             // 树桩 / a tree stump
	Mineral,           // 矿石 / a mineral node
};

/**
 * 需求人数与已指派情况 / Headcount requirement and current assignment.
 */
USTRUCT()
struct FHeadCount
{
	GENERATED_BODY()

	// 这个工作需要几名工作单元 / How many workers this job needs
	UPROPERTY() int32 Need = 1;

	// 已指派的工作单元 / Workers already assigned
	UPROPERTY() TArray<FGuid> AssignedWorkers;

	bool FullyAssigned() const { return AssignedWorkers.Num() >= Need; }
};

/**
 * 一条工作 / One unit of work.
 */
USTRUCT()
struct FWorkInfo
{
	GENERATED_BODY()

	UPROPERTY() FGuid           Id;
	UPROPERTY() EWorkSkill      Skill  = EWorkSkill::Handwork;
	UPROPERTY() EWorkTargetType Target = EWorkTargetType::None;

	// 工位；虚拟目标由系统算出一个可站立点 / Work site; virtual targets resolve to a standable point
	UPROPERTY() FVector SiteLocation = FVector::ZeroVector;

	UPROPERTY() FHeadCount HeadCount;

	/**
	 * 连续型工作 / Continuous work.
	 * 搬运、采集这类活干完不回到空闲，而是直接找同类的下一份——
	 * Hauling and gathering don't drop the worker back to idle; the worker immediately
	 * looks for the next job of the same kind —
	 * 否则一个搬运工会在每两箱货之间来回"空闲->重新寻路"，观感和效率都很差。
	 * otherwise a hauler would idle and re-path between every two crates, which looks
	 * bad and wastes throughput.
	 */
	UPROPERTY() bool bContinuous = false;

	/**
	 * 有些工作没有"完成"的概念 / Some work never completes.
	 * 例如采蜜是一份无限长的活，直到不再满足工作条件（蜂巢空了/天黑了）才结束。
	 * Collecting honey, for instance, is an endless job that ends only when its
	 * conditions stop holding — the hive empties, or night falls.
	 */
	UPROPERTY() bool bEndless = false;
};

/**
 * 派工匹配器 / The matcher.
 *
 * ！！！匹配以"工作"为主循环，而不是以"工作单元"为主循环。
 * The loop is driven by JOBS, not by workers.
 * 若以单元为主循环，先被遍历到的单元会挑走最近的活，导致需要多人的工作长期配不满、
 * With a worker-driven loop, whichever worker is visited first grabs the nearest job,
 * leaving multi-worker jobs perpetually understaffed
 * 玩家看到的现象就是"一堆生物在小活上打转，大工程没人干"。
 * — the player sees a swarm of creatures fussing over trivial tasks while the big
 * project sits untouched.
 */
UCLASS()
class UWorkSkillMatcher : public UObject
{
	GENERATED_BODY()

public:
	// 每轮匹配 / Run one matching pass
	void MatchOnce();

	void AddWork(const FWorkInfo& Work);
	void RegisterIdleWorker(const FGuid& WorkerGuid, EWorkSkill Skill);

protected:
	void AssignWorkerToWork(FWorkInfo& Work, const FGuid& WorkerGuid);

	/**
	 * 取该技能下一个可用单元 / Pop the next available worker for a skill.
	 * 实际项目里此处还会按"距离工位远近 + 单元的该项技能等级"排序，
	 * In practice this also ranks by distance to the site and the worker's proficiency
	 * in that skill,
	 * 让高手去干重活、近的去干近的活。
	 * so specialists take the demanding jobs and nearby workers take nearby ones.
	 */
	bool PopAvailableWorker(EWorkSkill Skill, const FVector& SiteLocation, FGuid& OutWorker);

private:
	// 尚未配满人的工作，按技能归类 / Understaffed jobs, bucketed by skill
	TMap<EWorkSkill, TArray<FWorkInfo>> PendingBySkill;

	// 空闲工作单元池，按技能归类 / Idle worker pool, bucketed by skill
	TMap<EWorkSkill, TArray<FGuid>> IdleBySkill;
};

inline void UWorkSkillMatcher::MatchOnce()
{
	for (auto& Pair : PendingBySkill)
	{
		const EWorkSkill Skill = Pair.Key;
		for (FWorkInfo& Work : Pair.Value)
		{
			// 把这个工作的人数补满，或直到该技能没有空闲单元为止
			// Staff this job to its headcount, or until this skill's pool runs dry
			FGuid Worker;
			while (!Work.HeadCount.FullyAssigned()
			       && PopAvailableWorker(Skill, Work.SiteLocation, Worker))
			{
				AssignWorkerToWork(Work, Worker);
			}
		}
	}
}
