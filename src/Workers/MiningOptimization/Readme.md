# Worker mining optimization

This file describes some of the considerations put into optimizing worker mining in Stardust.

## Background information

### Order process timer

Execution of most unit orders in StarCraft, including most of the orders related to mining, is governed by the order process timer. This timer generally cycles between the values 0-8 inclusive (though it can be set to higher values in some cases), and unit orders are only processed when its value is 0. [This openbw snippet](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L7751-L7755) shows how the order timer cycle is managed.

When a command is sent to a unit, this will generally reset the unit's order process timer. This allows us to manipulate its value and, with some constraints, ensure it cycles to 0 when we want an order to be processed.

Starting on frame 8, every 150 frames the order process timer of all units [is reset to a value between 0-7 inclusive](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L12870-L12879). While this resetting does not involve RNG, it depends on information not known to the bot, so units get an unpredictable value.

### Mining order transitions and timings

When ordered to gather, the worker gets the MoveToMinerals order and moves towards the patch.

Whenever the worker's order process timer is 0, the MoveToMinerals order will be processed. If the edge-to-edge distance from the worker to the patch is [10 or less and the patch is being mined by another worker, the worker will switch to a different patch, if one is available](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4319-L4330). If the worker has arrived at the patch (edge-to-edge distance is 0) and the patch is free, the worker's order will transition to WaitForMinerals.

From WaitForMinerals the order transitions to MiningMinerals after one frame, at which point the patch is marked as being occupied.

An exception to the above is if the worker attempts to switch patches, but is unable to do so because no other patches are available. In this case the worker essentially locks onto the targeted patch: it will not try to find an alternate patch again while in MoveToMinerals and will transition to WaitForMinerals immediately upon reaching order process timer 0 after arrival. It will then remain in the WaitForMinerals state until the other worker finishes mining, at which point it will transition directly to MiningMinerals on the same frame regardless of the order process timer. This is therefore a very desirable behaviour, as it removes all wait times during mining takeover, and the patch remains marked as occupied without any gap.

In MiningMinerals, [if the worker is not pointing at the patch](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4377) (for example if it tried to switch to a different patch while waiting), it will use a full order process timer cycle to turn towards the patch. The worker will remain in the MiningMinerals order, and the patch will be marked as occupied, but the order timer will not start counting down the actual mining time.

Once the worker has the MiningMinerals order and is pointed at the patch, [the main order timer is set to 75](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4380) and the worker is now mining. The main order timer is decremented every frame until it reaches 0.

When both the main order timer and order process timer are 0, the order transitions from MiningMinerals to ResetHarvestCollision, the worker receives the minerals, and the patch is marked as free.

From ResetHarvestCollision the worker transitions to ReturnMinerals on the next frame and starts moving towards the depot.

When the worker arrives at the depot (edge-to-edge distance is 0) and its order process timer is 0, the minerals are delivered and the order transitions directly to MoveToMinerals.

### Order of unit order processing

The StarCraft engine processes unit orders [in the order they appear in its visible units list](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L12924-L12929).

When units are added to the visible units list, they are added [at (or near) the head of the list](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/game_types.h#L41-L45).

This means that units have their orders processed in reverse order to when they last became visible.

Note that BWAPI's `isVisible` does not reflect whether a unit is in the visible units list or not. Rather, it appears to be tied to whether the units position is in the fog of war or not. Two examples are workers harvesting gas and workers loaded in a transport; in both cases they are not in the engine's visible units list, but BWAPI's `isVisible` will return true.

### Subpixels

Internally, the StarCraft engine generally stores positions in units of 1/256th of a pixel, allowing for smooth movement at speeds that are not exact multiples of pixels per frame. All of this is abstracted away at the BWAPI layer, where we just see the pixel locations.

When a unit finishes mining or delivering a resource, it needs to start moving along a new path that will generally be in the opposite direction of where it is currently facing. This means the unit first needs to turn before actually moving towards the new target.

At a subpixel level, at least on the first frame when leaving a patch or depot, the worker moves very slightly forward while executing the turn. This can be a problem, as the worker is always right next to the patch or depot and facing it when the turn begins. If its subpixel position was very close to the pixel boundary of the patch or depot, the worker will be pushed inside it and be detected as colliding with the patch or depot.

The resulting collision reconciliation state takes a full order process timer cycle to resolve, so unlucky subpixel placement of the worker can seriously impact mining efficiency.

## Start of gathering - no worker currently mining patch

When the patch is free, we ideally want the approaching worker's order process timer to reach 0 on the exact frame it arrives at the patch, while of course ensuring the worker arrives at the patch as early as possible.

Gather commands have the following delay before they can transition into mining:

- Latency frames
- One frame where the unit replans its movement and resets the mining status
- 9 frames for order process timer to cycle back to 0
- 1 frame in WaitForMinerals (once the worker is at the patch)

So we want to reissue a gather command exactly 10+LF frames before the worker arrives at the patch.

Issuing a new gather command to a worker targeting the same patch *usually* does not affect its movement: it continues along the same path and maintains its speed. So by tracking the position history of a mining worker, we can build a database of the optimal positions for resending the gather command for each patch.

When executing the command, the game engine does treat this as a new move target for the unit, however. The exact behaviour of the unit therefore depends on how the game engine chooses to recalculate the path. If it changes the next move waypoint for the unit, this may result in it arriving at the patch earlier or later than it would otherwise.

To make somewhat intelligent decisions about which gather positions to use, we track the observed results and compare this to the expected result if we do not resend the command at all. 

If an order process timer reset is to occur between the optimal resend position and reaching the patch, we cannot achieve optimal timing. What we do in this situation depends on the timing of the reset and what possibilies we have for reaching the patch faster. If the reset happens just after the optimal command would have kicked in, we can often send the gather command to take effect at the reset frame, which on average is a benefit. Otherwise we allow the worker to gather without resending the command and accept that it may take longer to begin mining.

One special case with order timer resets is when the order timer reset occurs on the frame LF+1 after issuing a mining command. Because it has just processed the gather command, normally the worker's order process timer will always be 0 on this frame, causing it to process the MoveToMinerals order. If there is an order timer reset, however, the worker will most likely be left in an in-between state until the order process timer reaches 0 again, where movement has been replanned but the mining state is at its default value. The result of this is that the worker needs an additional order process timer cycle before it can actually transition to mining, so we want to avoid this situation.

## Start of gathering - other worker currently mining patch

Optimizing taking over mining from another worker is somewhat more complicated, as we can't manipulate the mining worker's order process timer while it is mining.

### Possible mining timings

If there is no order process timer reset during mining, the worker's order process timer will always be 6 on the frame when the main order timer reaches 0. This means the total mining time from when the worker's main order timer is initialized to 75 until the worker finishes mining is 81 frames.

If there is an order process timer reset while the main order timer is still decrementing, the worker's order process timer can have any value in 0-8 inclusive when the main order timer expires. This means the overall mining time can vary between 75-83 frames inclusive.

If there is an order process timer reset after the main order timer has finished decrementing, the mining time can be extended even further. In the worst case, the worker's order process timer is reset to 7 on the frame where it otherwise would have finished, extending mining time to 88 frames.

### Order of unit order processing

The mineral patch is marked as available as part of the mining worker's order processing. This means that on the frame where mining finishes, another worker can start mining the patch immediately only if its orders are processed after the mining worker. Otherwise it needs to wait an extra frame, as it would try to switch patches if timed to take over on the same frame.

An exception to this is if the worker taking over is locked to the patch via trying to switch patches and not finding one available. Once the worker has entered this state, it doesn't actually transition to mining during its own order processing. Instead, the order processing of the worker finishing mining both updates its own state and transitions the other worker to mining. The order of unit processing is therefore irrelevant - the worker taking over will start mining immediately regardless of their relative positions in the visible units list, so long as it was "patch locked" prior to completion of mining by the mining worker.

### Effect of order process timer resets on the worker taking over

For the cases where there is an order process timer reset late in the mining cycle, we also need to consider that the worker taking over will also have their order timer reset. If this reset is to a low number, there is a good chance the patch will still be occupied and cause the worker to try to change patches.

### Optimal frame for takeover

Putting all of the above together, we have the following three main cases: no order process timer reset during mining, order process timer reset during mining (but not near the end), and order process timer reset near the end of mining.

If there is no order process timer reset during mining, the worker taking over knows the mining worker will take 81 frames to mine and can time its takeover accordingly.

If there is an order process timer reset during mining (but not near the end), we don't know the exact frame when the mining worker will finish mining, but know it will at worst be 83 frames. The consequence of timing too early is worse than timing too late, as being early causes the worker to switch patches, incurring a full latency frames + order process timer cycle to get it back on target. So we assume the worst and time the takeover to happen after 83 frames.

If there is an order process timer reset near the end of mining, we are limited by the fact that the worker taking over also has their order process timer reset. The best we can do is time it so the mining command kicks in on the frame of the reset, causing the order process timer cycle to start over. The other worker will always be finished mining by the time the order is processed.

For all of the above cases, we add an extra frame of delay if the worker taking over has its orders processed before the worker mining.

### Worker taking over arrives after the optimal takeover frame

If the worker taking over mining doesn't arrive at the mineral patch before the optimal takeover frame, we can fall back to the single-worker case, where we resend the gather order at a suitable position to allow mining immediately on arrival.

However, it can be difficult to identify whether the worker will arrive at the mineral patch before or after the optimal takeover frame. While a worker approaches the patch that another worker is mining, we must resend the gather command regularly to prevent it from switching patches. As noted earlier, however, resending the gather command can change the path the worker takes towards the patch. Our logic to maintain mineral locking is therefore effectively poisoning the observations we would like to rely on to know whether the worker will arrive at the patch on time.

To work around this, we rely as much as possible on the observations we make in the single-worker case, as the paths taken by the workers should be the same. Besides this, we make note of positions that have had suboptimal results in order to know when we need to change behaviour.

## Return of minerals

Similar to when approaching a free mineral patch, the optimal timing for returning minerals is to reach the depot at the same frame when the order process timer reaches 0.

However, unlike the gather command, reissuing the return cargo command (or anything equivalent, like right-clicking the depot) always affects the worker's movement: the worker stops moving completely for three frames and may take a different path back to the depot (which may be shorter).

Despite the potentially longer path back to the depot, optimizing the order process timer can still give a benefit, especially as in some cases the worker will maintain some of its speed and therefore reach the mineral patch again more quickly.

Since reissuing the return cargo command changes the path, we cannot simply observe normal mining patterns and compute the optimal reissue positions. Instead, a test infrastructure is needed that simulates returns from all possible mining locations and finds the best reissue position for each through trial-and-error.

## A note of caution about frame timing

A frame in BW with BWAPI is executed in this order:

1. BW starts a new frame; BWAPI takes over the process via a DLL hook
2. BWAPI reads the game memory to update all of its data about units, bullets, etc.
3. BWAPI allows the AI to do its thing (by signalling a client bot to process the frame or invoking the AIModule event callbacks in a module bot)
4. BWAPI issues any remaining buffered commands and does some bookkeeping
5. BWAPI returns from its hook, allowing BW to do its processing of the frame
6. BW's engine runs and updates the game state

This can cause some confusion when viewing a replay in CherryVis and comparing it to data logged by the bot, since they are seeing the game data at different points in the above cycle: CherryVis will show the state after step 6, while the bot on the same frame saw the engine data from the end of the previous frame.

This makes it very easy to make off-by-one frame timing errors when working on things like mining timings.

Stardust already keeps its own frame timer in an attempt to not get too confused by pauses, so to hopefully simplify things, it is initialized to run one tick behind BWAPI's frame timer. So when looking at a frame in CherryVis, the frame labels from the bot will match the frame with the engine data the bot's behaviour was based on. Frame numbers in the CherryVis data files are likewise decremented. This breaks down if there is pausing, but as this doesn't happen in games with CherryVis instrumentation, this isn't an issue.

## A note of caution about order process timer timing

Similar to the above, the details of how the order process timer cycle works are important to how they should be interpreted when viewing the values in CherryVis.

When a unit is being processed, the first check is whether the order process timer is nonzero. If this is true, the value is decremented and all further unit processing is skipped.

If the order process timer is zero during the above check, the timer is set to 8 and the rest of the unit processing is allowed to happen. Some orders then set the order process timer to different values than 8 (for example, starting an attack will set the order process timer to the same value as the weapon cooldown).

When we are paused on a frame in CherryVis, we see the order process timer values from the end of the frame. So if a worker has had its unit processing occur on that frame, its order process timer value will be 8 (ignoring orders that set another value during processing). An order process timer value of 0 indicates that the unit will have its processing occur on the next frame, unless something interrupts this (like an order process timer reset or a new command kicking in).

When an order process timer reset occurs every 150 frames, it happens before any of the units are updated and go through the above logic. So if you pause in CherryVis on the order process timer reset frame, you will see the value after the original reset value was updated according to the above logic. Since the order process timer resets use the values 0-7, you will therefore see units with values of 0-6 and 8 (ignoring orders that set another value during processing).
