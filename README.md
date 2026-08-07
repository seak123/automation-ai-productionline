# Automation AI — Creature Production Line

A reference design for a **creature-driven automation/production line**: assignable AI workers
(mechanical-beast "pets") that pick up work orders, path to work sites, and perform
production actions — built on **GAS (Gameplay Ability System)**, **Behavior Trees**, and
**Navigation**. This is the classic "assign a creature to a station and it works the line on
its own" loop.

> **About this repository.** Original *clean-room* write-up of a feature I built in a
> commercial title. **No proprietary source** — code below is illustrative reference code
> explaining the design and the module boundaries.

---

## 1. Problem

Players place production stations and assign creatures to them. Each creature must, on its
own: find the next unit of work, **navigate** to the right spot, play/drive the correct
**ability** (mine, haul, craft, deposit), and loop — reliably, for many creatures at once,
and correctly across the server/client boundary.

The interesting engineering is not any single action; it's the **clean seam between four
systems** (work dispatch, AI, abilities, navigation) so each can change independently.

---

## 2. Architecture

```
   +------------------+     assigns      +-------------------+
   |  Work Dispatch   |----------------->|  Worker (Pet AI)  |
   |  (station-side)  |<--- reports ----|  Behavior Tree     |
   +------------------+                  +---------+---------+
        | produces                                 |
        v                                           v  runs tasks
   FWorkOrder queue                        +--------+---------+
                                           |  MoveTo task      | -> Navigation
                                           |  PerformWork task | -> GAS ability
                                           +-------------------+
                                                    |
                                                    v  ability applies
                                             Gameplay Effect (produce / consume / cooldown)
```

- **Work dispatch component** (on the station/manager) owns a queue of `FWorkOrder`s and
  assigns them to idle workers. It knows *what* needs doing, not *how*.
- **Worker AI** (Behavior Tree) owns *how*: it pulls an order, runs a **MoveTo** task, then a
  **PerformWork** task, then reports completion and loops.
- **GAS** owns the *actual effect* of work — the produce/consume/cooldown is a
  `GameplayAbility` + `GameplayEffect`, so designers tune yields and timings as data.
- **Navigation** is consumed only by the MoveTo task, keeping pathfinding concerns isolated.

Each arrow is an interface, so (e.g.) the ability can be re-tuned without touching dispatch,
and the BT can change without touching GAS.

---

## 3. Work dispatch — assign orders to idle workers

```cpp
// Station-side: hand the next pending order to whichever worker is free.
// Dispatch knows WHAT to do; it never drives the worker's movement or animation.
void UWorkDispatchComponent::TickDispatch()
{
    for (TWeakObjectPtr<AWorkerPawn> Worker : RegisteredWorkers)
    {
        if (!Worker.IsValid() || !Worker->IsIdle()) continue;
        if (PendingOrders.IsEmpty()) break;

        FWorkOrder Order = PendingOrders.Dequeue();
        Order.State      = EWorkState::Assigned;

        Worker->AssignOrder(Order);          // fire-and-track; worker reports back on done/fail
        ActiveOrders.Add(Order.Id, Order);
    }
}

// Worker reports the outcome; dispatch re-queues on failure so nothing is silently dropped.
void UWorkDispatchComponent::OnWorkerReport(FGuid OrderId, EWorkResult Result)
{
    if (FWorkOrder* Order = ActiveOrders.Find(OrderId))
    {
        if (Result == EWorkResult::Failed)   // path blocked, resource gone, etc.
            PendingOrders.Enqueue(Order->Reset());
        ActiveOrders.Remove(OrderId);
    }
}
```

---

## 4. MoveTo task — the Navigation seam

A Behavior-Tree task that drives the worker to the order's work site and *only* concerns
itself with navigation. Robust handling of "can't reach" is what keeps a line from stalling.

```cpp
EBTNodeResult::Type UBTTask_MoveToWorkSite::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    AWorkerController* C = Cast<AWorkerController>(OwnerComp.GetAIOwner());
    const FVector Goal   = C->GetActiveOrder().WorkSiteLocation;

    FAIMoveRequest Req(Goal);
    Req.SetAcceptanceRadius(WorkReachRadius);
    Req.SetUsePathfinding(true);

    const FPathFollowingRequestResult R = C->MoveTo(Req);

    switch (R.Code)
    {
    case EPathFollowingRequestResult::AlreadyAtGoal: return EBTNodeResult::Succeeded;
    case EPathFollowingRequestResult::RequestSuccessful:
        WaitingMoveId = R.MoveId;                 // finish async in OnMoveCompleted
        return EBTNodeResult::InProgress;
    default:                                       // unreachable -> fail fast, dispatch re-queues
        return EBTNodeResult::Failed;
    }
}
```

> **Note on dynamic navmesh.** In an open world that changes at runtime (players build and
> demolish), the navmesh under these paths must regenerate *cheaply* or pathfinding stalls
> every time someone places a wall. I worked on generating/updating navmesh dynamically and
> keeping that cost bounded — see the companion write-up; here the MoveTo task simply assumes
> a valid, up-to-date navmesh.

---

## 5. PerformWork — the GAS seam

The actual production is a **GameplayAbility**: activating it plays the work, and a
**GameplayEffect** applies the yield/cost. Designers own the numbers as data.

```cpp
// BT task: activate the worker's production ability and wait for it to end.
EBTNodeResult::Type UBTTask_PerformWork::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    UAbilitySystemComponent* ASC = GetWorkerASC(OwnerComp);
    const FGameplayTag WorkTag   = OwnerComp.GetAIOwner<AWorkerController>()
                                    ->GetActiveOrder().AbilityTag;   // e.g. Work.Mine

    return ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(WorkTag))
        ? EBTNodeResult::InProgress          // ability's OnEnd finishes the task
        : EBTNodeResult::Failed;
}

// The ability itself: do the work, then commit the produce/consume effect on the server.
void UGA_Produce::ActivateAbility(/*...*/)
{
    PlayWorkMontageAndWait();                // cosmetic; drives timing
    // On the authoritative end-of-work tick:
    ApplyGameplayEffectToOwner(ProduceEffect);   // +output items, -input items, start cooldown
    EndAbility(/*...*/);
}
```

Because production is GAS-driven, the same worker AI powers *any* station — a mill, a smelter,
an incubator — just by swapping the ability tag on the work order. That reuse is the payoff of
the four-way separation.

---

## 6. Results

- One worker-AI + dispatch design **drives many different production stations** by data
  (ability tag + work site), instead of a bespoke script per station.
- Failure handling (unreachable sites, vanished resources) keeps lines **self-healing**
  instead of silently stalling.
- Clean seams meant designers tuned yields/timings in GAS and BT behaviour independently,
  without engineering round-trips.

## 7. What this demonstrates

Comfort across **GAS, Behavior Trees, and Navigation** at once; designing a feature as
**cooperating systems with clean interfaces** rather than one monolith; and building for
**data-driven reuse** so one framework serves many gameplay objects.
