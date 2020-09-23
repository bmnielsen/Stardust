# Stardust
AI for StarCraft: Brood War

## About
Stardust is written in C++ and uses [BWAPI](https://github.com/bwapi/bwapi) to play 1v1 Melee games of StarCraft: Brood War. Stardust plays as the Protoss race.

To see Stardust play, check out the [SSCAIT ladder and live stream](https://sscaitournament.com/) and the [BASIL ladder](https://basil.bytekeeper.org/).

Stardust is mostly optimized for AI vs. AI tournaments, but if you want to play against it, check out [SCHNAIL](https://schnail.com/).

## Libraries

Stardust uses [BWEM](http://bwem.sourceforge.net/) for terrain analysis and a modified version of [FAP](https://github.com/N00byEdge/FAP) for combat simulation.

## License

The license is MIT with an added condition that forks may not be submitted to StarCraft AI competitions without written consent of the author. The rationale behind this is to prevent a flood of minimally-altered forks popping up in tournaments, which happened with my previous bot Locutus in 2018.

So if you are interested in forking Stardust and submitting it to a tournament, create an issue here and we'll discuss it. I will likely accept any Zerg or Terran forks, but be much more strict with Protoss forks. 
