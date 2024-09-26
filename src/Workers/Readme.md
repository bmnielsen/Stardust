# Worker mining optimization

This file describes some of the considerations put into optimizing worker mining in Stardust.

## Background information

### Order process timer

Execution of most unit orders in StarCraft, including most of the orders related to mining, is governed by the order process timer. This timer generally cycles between the values 0-8 inclusive (though it can be set to higher values in some cases), and unit orders are only processed when its value is 0. [This openbw snippet](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L7751-L7755) shows how the order timer cycle is managed.

When a command is sent to a unit, this will generally reset the unit's order process timer. This allows us to manipulate its value and, with some constraints, ensure it cycles to 0 when we want an order to be processed.

Starting on frame 8, every 150 frames the order process timer of all units [is reset to a value between 0-7 inclusive](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L12870-L12879). While this resetting does not involve RNG, it depends on information not known to the bot, so units get an unpredictable value.

### Mining order transitions and timings

When ordered to gather, the worker gets the MoveToMinerals order and moves towards the patch.

Whenever the worker's order process timer is 0, the MoveToMinerals order will be processed. If the edge-to-edge distance from the worker to the patch is [10 or less and the patch is being mined by another worker, the worker will switch to a different patch](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4319-L4330). If the worker has arrived at the patch (edge-to-edge distance is 0) and the patch is free, the worker's order will transition to WaitForMinerals.

From WaitForMinerals the order transitions to MiningMinerals after one frame.

In MiningMinerals, [if the worker is not pointing at the patch](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4377) (for example if it tried to switch to a different patch while waiting), it will wait, essentially adding a full order process timer cycle of delay.

Once the worker has the MiningMinerals order and is pointed at the patch, [the main order timer is set to 75](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4380) and the worker is now mining. The main order timer is decremented every frame until it reaches 0.

When both the main order timer and order process timer are 0, the order transitions from MiningMinerals to ResetHarvestCollision, the worker receives the minerals, and the patch is marked as free.

From ResetHarvestCollision the worker transitions to ReturnMinerals on the next frame and starts moving towards the depot.

When the worker arrives at the depot (edge-to-edge distance is 0) and its order process timer is 0, the minerals are delivered and the order transitions directly to MoveToMinerals.

### Order of unit order processing

The StarCraft engine processes unit orders [in the order they appear in its visible units list](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L12924-L12929).

When units are added to the visible units list, they are added [at (or near) the head of the list](https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/game_types.h#L41-L45).

This means that units have their orders processed in reverse order to when they last became visible.

## Start of gathering - no worker currently mining patch

When the patch is free, we want the approaching worker's order process timer to reach 0 on the exact frame it arrives at the patch.

Gather commands have the following delay before they can transition into mining:

- Latency frames
- 3 frames where the worker goes through a ResetCollision cycle
- 8 frames for the order process timer to cycle back to 0

So we want to reissue a gather command exactly 11+LF frames before the worker arrives at the patch.

Issuing a new gather command to a worker targeting the same patch *usually* does not affect its movement: it continues along the same path and maintains its speed. So by tracking the position history of a mining worker, we can build a database of the optimal positions for resending the gather command for each patch.

When executing the command, the game engine does treat this as a new move target for the unit, however. The exact behaviour of the unit therefore depends on how the game engine chooses to recalculate the path. If it changes the next move waypoint for the unit, this may result in it arriving at the patch earlier or later than it would otherwise.

To make somewhat intelligent decisions about which gather positions to use, we track the observed results and compare this to the expected result if we do not resend the command at all. 

If an order process timer reset is to occur between the optimal resend position and reaching the patch, we cannot achieve optimal timing. What we do in this situation depends on the timing of the reset. If the reset happens just after the optimal command would have kicked in, we send the gather command to take effect at the reset frame, which on average is a benefit. Otherwise we allow the worker to gather without resending the command and accept that it may take longer to begin mining.

## Start of gathering - other worker currently mining patch

Optimizing taking over mining from another worker is somewhat more complicated, as we can't manipulate the mining worker's order process timer while it is mining.

### Possible mining timings

If there is no order process timer reset during mining, the worker's order process timer will always be 6 on the frame when the main order timer reaches 0. This means the total mining time from when the worker's main order timer is initialized to 75 until the worker finishes mining is 81 frames.

If there is an order process timer reset while the main order timer is still decrementing, the worker's order process timer can have any value between 0-8 inclusive when the main order timer expires. This means the overall mining time can vary between 75-83 frames inclusive.

If there is an order process timer reset after the main order timer has finished decrementing, the mining time can be extended even further. In the worst case, the worker's order process timer is reset to 7 on the frame where it otherwise would have finished, extending mining time to 88 frames.

### Order of unit order processing

The mineral patch is marked as available as part of the mining worker's order processing. This means that on the frame where mining finishes, another worker can start mining the patch immediately only if its orders are processed after the mining worker. Otherwise it needs to wait an extra frame, as it would try to switch patches if timed to take over on the same frame.

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

However, unlike the gather command, reissuing the return cargo command (or anything equivalent, like right-clicking the depot) does affect the worker's movement: the worker stops moving completely for three frames and may take a different path back to the depot (which may be shorter).

Despite the potentially longer path back to the depot, optimizing the order process timer can still give a benefit, especially as in some cases the worker will maintain some of its speed and therefore reach the mineral patch again more quickly.

Since reissuing the return cargo command changes the path, we cannot simply observe normal mining patterns and compute the optimal reissue positions. Instead, a test infrastructure is needed that simulates returns from all possible mining locations and finds the best reissue position for each through trial-and-error.
