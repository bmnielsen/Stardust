/*
 * Copyright (c) 2019-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

"use strict";


// =============== ACCESS TO OPENBW
function _cvis_current_frame() {
  return _replay_get_value(2);
}

function _cvis_selected_units() {
  return Module.get_selected_units();
}

function _cvis_select_unit_bw_id(bw_id) {
  Module.clear_selection();
  var unit = Module.lookup_unit(bw_id);
  if (unit) {
    Module.select_unit_by_bw_id(bw_id);
    Module.set_screen_center_position(unit['x'], unit['y']);
  }
  else {
    console.log('BW Unit with id', bw_id, 'not found.');
  }
}

var _cvis_all_states = [];

$(document).ready(function(){
  // Expand & collapse
  const EXPAND_WIDTH = '80vw';
  $('.cvis-collapse-button').hide();
  $('.cvis-expand-collapse').click(function() {
    $('.cvis-expand-collapse').toggle();
    $('.infobar').toggle();
    if ($('.sidebar-cdb').attr('style') === undefined || !$('.sidebar-cdb').attr('style').includes(EXPAND_WIDTH))
      $('.sidebar-cdb').css('min-width', EXPAND_WIDTH);
    else
      $('.sidebar-cdb').css('min-width', '');
  });

  // Back to list of replays
  $(window).on('popstate', function(e){
  	var new_state = e.originalEvent.state;
    if (new_state.page == 'replay') {
      return; // Will be handled in available_replays.js
    }
    cvis_stop_all().then(cvis_reset_dom_state);
  });
});

function cvis_reset_dom_state() {
  $('.cvis-dbg-show-when-loading-failed').show();
  $('.cvis-dbg-show-when-loading').hide();
  $('.cvis-dbg-show-when-running').hide();
  $('.cvis-dbg-show-when-loading').hide();
  $('.cvis-dbg-show-when-loading-failed').hide();
  $('.cvis-dbg-show-when-matching-units').hide();
  $('.list-of-replays-main-page').show();
  $('.hide-when-game-on').show();
  $('.show-when-game-on').addClass('collapse');
  $('.sidebar-cdb').show();  // But has collapse class
  $('.cvis-matching-units-progress-bar').width('0%');
  $('.delete-on-reset-cvis').remove();
}

function cvis_stop_all() {
  return new Promise(function(resolve, reject) {
    function _check_ready() {
      var all_stopped = true;
      $.each(_cvis_all_states, function(_, s) {
        all_stopped = all_stopped && s['killed'];
      });
      if (all_stopped) {
        Module.reset_replay();
        resolve();
      }
      else
        window.setTimeout(_check_ready, 300);
    }
    var all_stopped = true;
    $.each(_cvis_all_states, function(_, s) {
      all_stopped = all_stopped && s['killed'];
    });
    if (all_stopped) {
      resolve();
      return;
    }
    $.each(_cvis_all_states, function(_, s) {
      s['kill'] = true;
    });
    _check_ready();
  });
}

function cvis_dbg_init(rep, cvis_path) {
  return new Promise(function(resolve, reject) {
    $.get("get/replay_info?rep=" + encodeURIComponent(rep), function(replay_info) {
        // Load the multi dumps
        var proms = [];
        $.each(replay_info.multi, function(multi_name, multi_cvis_file) {
          proms.push(new Promise(function(res, rej) {
            $.get("get/cvis/?cvis=" + encodeURIComponent(multi_cvis_file), d => res({name: multi_name, cvis_path: multi_cvis_file, data: d}),
                "json").fail(function() {
                  res(null);
            });
          }));
        });
        // Load the main dump
        $.get("get/cvis/?cvis=" + encodeURIComponent(cvis_path), function(d) {
          // Load global data that is split into other files
          if (typeof d.logs === 'string') {
            proms.push(new Promise(function(res, rej) {
              $.get("get/cvis/?cvis=" + encodeURIComponent(cvis_path) + "&f=" + encodeURIComponent(d.logs),
                      logs => {d.logs = logs; res(null)}, "json").fail(function() {
                res(null);
              });
            }));
          }
          if (typeof d.draw_commands === 'string') {
            proms.push(new Promise(function(res, rej) {
              $.get("get/cvis/?cvis=" + encodeURIComponent(cvis_path) + "&f=" + encodeURIComponent(d.draw_commands),
                  draw_commands => {d.draw_commands = draw_commands; res(null)}, "json").fail(function() {
                res(null);
              });
            }));
          }

          Promise.all(proms).then(function(multi_datas) {
            var resolution = {data: d, cvis_path: cvis_path, replay_info: replay_info, multi: {}};
            $.each(multi_datas, function(_, multi_data) {
              if (multi_data != null) {
                resolution.multi[multi_data.name] = multi_data;
              }
            });
            resolve(resolution);
          });
        }, "json").fail(function() {
              resolve(null);
        });
    }, "json").fail(function() {
        resolve(null);
    });
  });
}

function create_worker(file, initial_data) {
  var w = new Worker('/static/replays/cdb/' + file);
  var s = {
    'worker': w,
    'last_result': null,
    'post': w.postMessage,
  };
  if (initial_data) w.postMessage({'init_data': initial_data});
  w.onmessage = function(m) {
    s['last_result'] = m.data;
  };
  return s;
}

function cvis_init_with_data(path, init_data, config) {
  if (init_data === null) {
    init_data = {"data":{"board_updates":{},"draw_commands":{},"heatmaps":[],"logs":[],"squads":{},"units_draw":{},"units_first_seen":{},"units_logs":{},"units_updates":{}},"cvis_path":null,"replay_info":{"multi":{}},"multi":{}};
    // $('.cvis-dbg-show-when-loading-failed').show();
    // $('.cvis-dbg-show-when-loading').hide();
    // // Still play the game, but without the cherryvis sidebar
    // $('.sidebar-cdb').hide();
    // Module.enable_main_update_loop();
    // return;
  }
  init_data.data.orders_names = {"0":"Die","1":"Stop","2":"Guard","3":"PlayerGuard","4":"TurretGuard","5":"BunkerGuard","6":"Move","7":"ReaverStop","8":"Attack1","9":"Attack2","10":"AttackUnit","11":"AttackFixedRange","12":"AttackTile","13":"Hover","14":"AttackMove","15":"InfestedCommandCenter","16":"UnusedNothing","17":"UnusedPowerup","18":"TowerGuard","19":"TowerAttack","20":"VultureMine","21":"StayInRange","22":"TurretAttack","23":"Nothing","24":"Unused_24","25":"DroneStartBuild","26":"DroneBuild","27":"CastInfestation","28":"MoveToInfest","29":"InfestingCommandCenter","30":"PlaceBuilding","31":"PlaceProtossBuilding","32":"CreateProtossBuilding","33":"ConstructingBuilding","34":"Repair","35":"MoveToRepair","36":"PlaceAddon","37":"BuildAddon","38":"Train","39":"RallyPointUnit","40":"RallyPointTile","41":"ZergBirth","42":"ZergUnitMorph","43":"ZergBuildingMorph","44":"IncompleteBuilding","45":"IncompleteMorphing","46":"BuildNydusExit","47":"EnterNydusCanal","48":"IncompleteWarping","49":"Follow","50":"Carrier","51":"ReaverCarrierMove","52":"CarrierStop","53":"CarrierAttack","54":"CarrierMoveToAttack","55":"CarrierIgnore2","56":"CarrierFight","57":"CarrierHoldPosition","58":"Reaver","59":"ReaverAttack","60":"ReaverMoveToAttack","61":"ReaverFight","62":"ReaverHoldPosition","63":"TrainFighter","64":"InterceptorAttack","65":"ScarabAttack","66":"RechargeShieldsUnit","67":"RechargeShieldsBattery","68":"ShieldBattery","69":"InterceptorReturn","70":"DroneLand","71":"BuildingLand","72":"BuildingLiftOff","73":"DroneLiftOff","74":"LiftingOff","75":"ResearchTech","76":"Upgrade","77":"Larva","78":"SpawningLarva","79":"Harvest1","80":"Harvest2","81":"MoveToGas","82":"WaitForGas","83":"HarvestGas","84":"ReturnGas","85":"MoveToMinerals","86":"WaitForMinerals","87":"MiningMinerals","88":"Harvest3","89":"Harvest4","90":"ReturnMinerals","91":"Interrupted","92":"EnterTransport","93":"PickupIdle","94":"PickupTransport","95":"PickupBunker","96":"Pickup4","97":"PowerupIdle","98":"Sieging","99":"Unsieging","100":"WatchTarget","101":"InitCreepGrowth","102":"SpreadCreep","103":"StoppingCreepGrowth","104":"GuardianAspect","105":"ArchonWarp","106":"CompletingArchonSummon","107":"HoldPosition","108":"QueenHoldPosition","109":"Cloak","110":"Decloak","111":"Unload","112":"MoveUnload","113":"FireYamatoGun","114":"MoveToFireYamatoGun","115":"CastLockdown","116":"Burrowing","117":"Burrowed","118":"Unburrowing","119":"CastDarkSwarm","120":"CastParasite","121":"CastSpawnBroodlings","122":"CastEMPShockwave","123":"NukeWait","124":"NukeTrain","125":"NukeLaunch","126":"NukePaint","127":"NukeUnit","128":"CastNuclearStrike","129":"NukeTrack","130":"InitializeArbiter","131":"CloakNearbyUnits","132":"PlaceMine","133":"RightClickAction","134":"SuicideUnit","135":"SuicideLocation","136":"SuicideHoldPosition","137":"CastRecall","138":"Teleport","139":"CastScannerSweep","140":"Scanner","141":"CastDefensiveMatrix","142":"CastPsionicStorm","143":"CastIrradiate","144":"CastPlague","145":"CastConsume","146":"CastEnsnare","147":"CastStasisField","148":"CastHallucination","149":"Hallucination2","150":"ResetCollision","151":"ResetHarvestCollision","152":"Patrol","153":"CTFCOPInit","154":"CTFCOPStarted","155":"CTFCOP2","156":"ComputerAI","157":"AtkMoveEP","158":"HarassMove","159":"AIPatrol","160":"GuardPost","161":"RescuePassive","162":"Neutral","163":"ComputerReturn","164":"InitializePsiProvider","165":"SelfDestructing","166":"Critter","167":"HiddenGun","168":"OpenDoor","169":"CloseDoor","170":"HideTrap","171":"RevealTrap","172":"EnableDoodad","173":"DisableDoodad","174":"WarpIn","175":"Medic","176":"MedicHeal","177":"HealMove","178":"MedicHoldPosition","179":"MedicHealToIdle","180":"CastRestoration","181":"CastDisruptionWeb","182":"CastMindControl","183":"DarkArchonMeld","184":"CastFeedback","185":"CastOpticalFlare","186":"CastMaelstrom","187":"JunkYardDog","188":"Fatal","189":"None","190":"Unknown"};
  init_data.data.types_names = {"0":"Terran_Marine","1":"Terran_Ghost","2":"Terran_Vulture","3":"Terran_Goliath","5":"Terran_Siege_Tank_Tank_Mode","7":"Terran_SCV","8":"Terran_Wraith","9":"Terran_Science_Vessel","10":"Hero_Gui_Montag","11":"Terran_Dropship","12":"Terran_Battlecruiser","13":"Terran_Vulture_Spider_Mine","14":"Terran_Nuclear_Missile","15":"Terran_Civilian","16":"Hero_Sarah_Kerrigan","17":"Hero_Alan_Schezar","19":"Hero_Jim_Raynor_Vulture","20":"Hero_Jim_Raynor_Marine","21":"Hero_Tom_Kazansky","22":"Hero_Magellan","23":"Hero_Edmund_Duke_Tank_Mode","25":"Hero_Edmund_Duke_Siege_Mode","27":"Hero_Arcturus_Mengsk","28":"Hero_Hyperion","29":"Hero_Norad_II","30":"Terran_Siege_Tank_Siege_Mode","32":"Terran_Firebat","33":"Spell_Scanner_Sweep","34":"Terran_Medic","35":"Zerg_Larva","36":"Zerg_Egg","37":"Zerg_Zergling","38":"Zerg_Hydralisk","39":"Zerg_Ultralisk","40":"Zerg_Broodling","41":"Zerg_Drone","42":"Zerg_Overlord","43":"Zerg_Mutalisk","44":"Zerg_Guardian","45":"Zerg_Queen","46":"Zerg_Defiler","47":"Zerg_Scourge","48":"Hero_Torrasque","49":"Hero_Matriarch","50":"Zerg_Infested_Terran","51":"Hero_Infested_Kerrigan","52":"Hero_Unclean_One","53":"Hero_Hunter_Killer","54":"Hero_Devouring_One","55":"Hero_Kukulza_Mutalisk","56":"Hero_Kukulza_Guardian","57":"Hero_Yggdrasill","58":"Terran_Valkyrie","59":"Zerg_Cocoon","60":"Protoss_Corsair","61":"Protoss_Dark_Templar","62":"Zerg_Devourer","63":"Protoss_Dark_Archon","64":"Protoss_Probe","65":"Protoss_Zealot","66":"Protoss_Dragoon","67":"Protoss_High_Templar","68":"Protoss_Archon","69":"Protoss_Shuttle","70":"Protoss_Scout","71":"Protoss_Arbiter","72":"Protoss_Carrier","73":"Protoss_Interceptor","74":"Hero_Dark_Templar","75":"Hero_Zeratul","76":"Hero_Tassadar_Zeratul_Archon","77":"Hero_Fenix_Zealot","78":"Hero_Fenix_Dragoon","79":"Hero_Tassadar","80":"Hero_Mojo","81":"Hero_Warbringer","82":"Hero_Gantrithor","83":"Protoss_Reaver","84":"Protoss_Observer","85":"Protoss_Scarab","86":"Hero_Danimoth","87":"Hero_Aldaris","88":"Hero_Artanis","89":"Critter_Rhynadon","90":"Critter_Bengalaas","91":"Special_Cargo_Ship","92":"Special_Mercenary_Gunship","93":"Critter_Scantid","94":"Critter_Kakaru","95":"Critter_Ragnasaur","96":"Critter_Ursadon","97":"Zerg_Lurker_Egg","98":"Hero_Raszagal","99":"Hero_Samir_Duran","100":"Hero_Alexei_Stukov","101":"Special_Map_Revealer","102":"Hero_Gerard_DuGalle","103":"Zerg_Lurker","104":"Hero_Infested_Duran","105":"Spell_Disruption_Web","106":"Terran_Command_Center","107":"Terran_Comsat_Station","108":"Terran_Nuclear_Silo","109":"Terran_Supply_Depot","110":"Terran_Refinery","111":"Terran_Barracks","112":"Terran_Academy","113":"Terran_Factory","114":"Terran_Starport","115":"Terran_Control_Tower","116":"Terran_Science_Facility","117":"Terran_Covert_Ops","118":"Terran_Physics_Lab","120":"Terran_Machine_Shop","122":"Terran_Engineering_Bay","123":"Terran_Armory","124":"Terran_Missile_Turret","125":"Terran_Bunker","126":"Special_Crashed_Norad_II","127":"Special_Ion_Cannon","128":"Powerup_Uraj_Crystal","129":"Powerup_Khalis_Crystal","130":"Zerg_Infested_Command_Center","131":"Zerg_Hatchery","132":"Zerg_Lair","133":"Zerg_Hive","134":"Zerg_Nydus_Canal","135":"Zerg_Hydralisk_Den","136":"Zerg_Defiler_Mound","137":"Zerg_Greater_Spire","138":"Zerg_Queens_Nest","139":"Zerg_Evolution_Chamber","140":"Zerg_Ultralisk_Cavern","141":"Zerg_Spire","142":"Zerg_Spawning_Pool","143":"Zerg_Creep_Colony","144":"Zerg_Spore_Colony","146":"Zerg_Sunken_Colony","147":"Special_Overmind_With_Shell","148":"Special_Overmind","149":"Zerg_Extractor","150":"Special_Mature_Chrysalis","151":"Special_Cerebrate","152":"Special_Cerebrate_Daggoth","154":"Protoss_Nexus","155":"Protoss_Robotics_Facility","156":"Protoss_Pylon","157":"Protoss_Assimilator","159":"Protoss_Observatory","160":"Protoss_Gateway","162":"Protoss_Photon_Cannon","163":"Protoss_Citadel_of_Adun","164":"Protoss_Cybernetics_Core","165":"Protoss_Templar_Archives","166":"Protoss_Forge","167":"Protoss_Stargate","168":"Special_Stasis_Cell_Prison","169":"Protoss_Fleet_Beacon","170":"Protoss_Arbiter_Tribunal","171":"Protoss_Robotics_Support_Bay","172":"Protoss_Shield_Battery","173":"Special_Khaydarin_Crystal_Form","174":"Special_Protoss_Temple","175":"Special_XelNaga_Temple","176":"Resource_Mineral_Field","177":"Resource_Mineral_Field_Type_2","178":"Resource_Mineral_Field_Type_3","184":"Special_Independant_Starport","188":"Resource_Vespene_Geyser","189":"Special_Warp_Gate","190":"Special_Psi_Disrupter","194":"Special_Zerg_Beacon","195":"Special_Terran_Beacon","196":"Special_Protoss_Beacon","197":"Special_Zerg_Flag_Beacon","198":"Special_Terran_Flag_Beacon","199":"Special_Protoss_Flag_Beacon","200":"Special_Power_Generator","201":"Special_Overmind_Cocoon","202":"Spell_Dark_Swarm","203":"Special_Floor_Missile_Trap","204":"Special_Floor_Hatch","205":"Special_Upper_Level_Door","206":"Special_Right_Upper_Level_Door","207":"Special_Pit_Door","208":"Special_Right_Pit_Door","209":"Special_Floor_Gun_Trap","210":"Special_Wall_Missile_Trap","211":"Special_Wall_Flame_Trap","212":"Special_Right_Wall_Missile_Trap","213":"Special_Right_Wall_Flame_Trap","214":"Special_Start_Location","215":"Powerup_Flag","216":"Powerup_Young_Chrysalis","217":"Powerup_Psi_Emitter","218":"Powerup_Data_Disk","219":"Powerup_Khaydarin_Crystal","220":"Powerup_Mineral_Cluster_Type_1","221":"Powerup_Mineral_Cluster_Type_2","222":"Powerup_Protoss_Gas_Orb_Type_1","223":"Powerup_Protoss_Gas_Orb_Type_2","224":"Powerup_Zerg_Gas_Sac_Type_1","225":"Powerup_Zerg_Gas_Sac_Type_2","226":"Powerup_Terran_Gas_Tank_Type_1","227":"Powerup_Terran_Gas_Tank_Type_2","228":"None","233":"Unknown"};

  var cvis_state = {
    'multi': init_data.multi,
    'rep_path': path,
    'cvis_path': init_data.cvis_path,
    'current_multi': config['multi'] !== undefined ? config['multi'] : '',
    'moving_to_frame': -1,
    'previous_update_frame': 0,
    'current_frame': 0,
    'previously_selected_ids': [],
    'selected_units': [],
    'tasks_tracked': [],
    'kill': false,
    'killed': false,
    'id2bw': null,
    'bw2id': null,
    'units_matching': {
      'start': null,
      'skipped': false,
    },
    'functions': {},
  };

  var global_data = init_data.data;
  init_data.multi[''] = {
    'data': init_data.data,
    'cvis_path': init_data.cvis_path,
    'name': 'Default',
  };
  if (cvis_state.current_multi != '') {
    global_data = init_data.multi[cvis_state.current_multi].data;
    cvis_state.cvis_path = init_data.multi[cvis_state.current_multi].cvis_path;
  }
  cvis_state['workers'] = {
    'logs': create_worker('workerlogs.js', global_data['logs'], {
      'total_count': 0,
      'entries': [],
    }),
    'board': create_worker('workerboard.js', {
      'total_count': 0,
      'entries': [],
    }),
  };
  _cvis_all_states.push(cvis_state);
  $('.cvis-empty-on-reset').html('');
  $('.cvis-dbg-show-when-running').show();
  $('.cvis-matching-units-skip').click(function() {
    cvis_state.units_matching.skipped = true;
  });

  // ========== MAIN UPDATE
  async function _cvis_update() {
    if (cvis_state['kill']) {
      $.each(cvis_state['workers'], function(_, w) {
        w.worker.terminate();
      });
      cvis_state['killed'] = true;
      return;
    }

    if (!Module.has_replay_loaded()) { // Replay still loading
      window.setTimeout(_cvis_update, 300);
      return;
    }

    if (cvis_state.id2bw == null) {
      var matching = cvis_dbg_match_units(global_data, cvis_state);
      if (!matching) {
        window.setTimeout(_cvis_update, 5);
        return;
      }
      cvis_dbg_lib_init(global_data, cvis_state);

      if (typeof global_data['board_updates'] === 'string') {
        global_data.board_updates = await cvis_state.functions.load_cvis_json_file_async(cvis_state.cvis_path, global_data.board_updates);
      }
      cvis_state.workers.board.worker.postMessage({'init_data': global_data.board_updates});

      cvis_dbg_unitslist_init(global_data, cvis_state);
      cvis_dbg_logs_init(global_data, cvis_state);
      cvis_dbg_logs_attachments_init(global_data, cvis_state);
      cvis_dbg_blackboard_init(global_data, cvis_state);
      cvis_dbg_tasks_init(global_data, cvis_state);
      cvis_dbg_trees_init(global_data, cvis_state);
      cvis_dbg_heatmaps_init(global_data, cvis_state);
      cvis_dbg_tensors_summaries_init(global_data, cvis_state);
      cvis_dbg_values_graph_init(global_data, cvis_state);

      $.each(cvis_state.multi, function(other_name, other) {
        if (other_name == cvis_state.current_multi) {
          return;
        }
        cvis_dbg_heatmaps_merge(global_data, cvis_state, other);
        cvis_dbg_trees_merge(global_data, cvis_state, other);
        cvis_dbg_tensors_summaries_merge(global_data, cvis_state, other);
      });


      if (config['frame'] !== undefined) {
        cvis_state.functions.openbw_set_current_frame(config['frame']);
      }
    }

    // Load data for selected or tracked units
    for (let unit_id of cvis_state.selected_units) {
      if (global_data.units_draw && global_data.units_draw[unit_id] && typeof global_data.units_draw[unit_id] === 'string') {
        global_data.units_draw[unit_id] =
            await cvis_state.functions.load_cvis_json_file_async(cvis_state.cvis_path, "drawCommands_" + unit_id + ".json.zstd");
      }
    }

    for (let unit of cvis_state.unitslist.tracked) {
      if (global_data.units_logs && global_data.units_logs[unit.unit_id] && typeof global_data.units_logs[unit.unit_id] === 'string') {
        global_data.units_logs[unit.unit_id] =
            await cvis_state.functions.load_cvis_json_file_async(cvis_state.cvis_path, "logs_" + unit.unit_id + ".json.zstd");
      }
    }

    cvis_state.current_frame = _cvis_current_frame();
    if (cvis_state.current_frame < cvis_state.moving_to_frame) {
      // OpenBW is moving to given frame, let's render cvis at the destination
      // frame right now
      cvis_state.current_frame = cvis_state.moving_to_frame;
    }
    else {
      cvis_state.moving_to_frame = -1;
    }
    $('.cvis-dbg-current-frame').text(cvis_state.current_frame);
    cvis_dbg_matching_update(global_data, cvis_state);
    cvis_dbg_unitslist_update_selected(global_data, cvis_state);
    cvis_dbg_unitslist_update_global(global_data, cvis_state);
    cvis_dbg_tasks_update(global_data, cvis_state);
    cvis_dbg_logs_update(global_data, cvis_state);
    cvis_dbg_blackboard_update(global_data, cvis_state);
    await cvis_dbg_drawcommands_update(global_data, cvis_state);
    cvis_dbg_trees_update(global_data, cvis_state);
    await cvis_dbg_squads_update(global_data, cvis_state);
    cvis_dbg_heatmaps_update(global_data, cvis_state);
    cvis_dbg_tensors_summaries_update(global_data, cvis_state);
    cvis_dbg_values_graph_update(global_data, cvis_state);
    cvis_dbg_lib_update_end(global_data, cvis_state);
    cvis_state.previous_update_frame = cvis_state.current_frame;

    window.setTimeout(_cvis_update, 500);
  }

  window.setTimeout(_cvis_update, 0);
}
