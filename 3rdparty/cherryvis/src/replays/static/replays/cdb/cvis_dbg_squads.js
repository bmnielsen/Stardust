"use strict";

async function cvis_dbg_squads_update(global_data, cvis_state) {
  // Don't do anything if we have no data or the pane is closed
  if (!global_data.squads) return;
  if (!$('.squads-view').hasClass('show')) return;

  // Load legacy data if we have it in that format
  if (typeof global_data.squads === 'string') {
    global_data.squads_legacy = await cvis_state.functions.load_cvis_json_file_async(cvis_state.cvis_path, global_data.squads);
  }

  // Get the squads for the current frame
  let frame_squads;
  if (global_data.squads_legacy) {
    frame_squads = global_data.squads[cvis_state.current_frame] || [];
  } else {
    frame_squads = await cvis_state.functions.load_cvis_partitioned_json_file_async(cvis_state.cvis_path, global_data, 'squads', cvis_state.current_frame, 250);
  }

  // Generate a DOM id for each squad
  for (let frame_squad of frame_squads) {
    frame_squad.id = frame_squad.label.toLowerCase().replace(/[^a-z0-9]/g, "-");
  }

  // Update the squads nav
  const SQUADS_TABS_NAV = $('#squads-tabs-nav');
  update_dom_list(SQUADS_TABS_NAV, frame_squads, 'data-squad-id',
      function(e) {
        return $('<li class="nav-item delete-on-reset-cvis">').append(
            $('<a class="nav-link" data-toggle="tab" href="#squad-details" role="tab" aria-controls="squad-details" aria-selected="false">'
            ).attr('aria-controls', e.id
            ).attr('href', '#' + e.id
            ).text(e.label)
        );
      },
  );

  // Update the squads tabs
  const SQUADS_TABS = $('#squads-tabs');
  update_dom_list(SQUADS_TABS, frame_squads, 'data-squad-id',
      function(e) {
          var copy = SQUADS_TABS.children().filter('.template').clone();
          copy.attr('id', e.id);
          copy.removeClass('collapse template');
          copy.addClass('delete-on-reset-cvis');
          cvis_state.functions.parse_cvis_links(copy);
          return copy;
      },
  );

  // Show/hide the "no squads available" message
  if (frame_squads.length === 0) {
    $('.show-when-no-squads').show();
  } else {
    $('.show-when-no-squads').hide();
  }

  // If there is no selected squad, jump out now
  const active_squad_tab = SQUADS_TABS.find('.show');
  const current_squad = frame_squads.find(x => x.id === active_squad_tab.attr('data-squad-id'));
  if (!active_squad_tab || !current_squad) return;

  function set_position(el, x, y) {
    const wt_x = parseInt(x / XYPixelsPerWalktile);
    const wt_y = parseInt(y / XYPixelsPerWalktile);
    el.attr('data-pos-x-wt', wt_x)
      .attr('data-pos-y-wt', wt_y)
      .text('(' + wt_x + ', ' + wt_y + ')');
  }

  set_position(active_squad_tab.find('.squad-target'), current_squad.target_position_x, current_squad.target_position_y);
  active_squad_tab.find('.squad-unit-count').text(current_squad.count_combatUnits + current_squad.count_arbiters);
  active_squad_tab.find('.squad-observer-count').text(current_squad.count_observers);

  const vanguard_cluster = current_squad.clusters.find(x => x.is_vanguard);
  if (!vanguard_cluster)
  {
    $('.show-when-no-vanguard-cluster').show();
  } else {
    $('.show-when-no-vanguard-cluster').hide();

    set_position(active_squad_tab.find('.vanguard-center'), vanguard_cluster.center_x, vanguard_cluster.center_y);
    active_squad_tab.find('.vanguard-unit-count').text(vanguard_cluster.unit_count);
    active_squad_tab.find('.vanguard-target-percent').text(Math.round(vanguard_cluster.percent_distance_to_target * 100.0));

    let activity = vanguard_cluster.activity;
    if (vanguard_cluster.subactivity !== "None") {
      activity += " - " + vanguard_cluster.subactivity;
    }
    active_squad_tab.find('.vanguard-activity').text(activity);

    function valuepair(a, b) {
        return a + ' / ' + b;
    }
    function twodecimals(a) {
        return Math.round((a + Number.EPSILON) * 100) / 100
    }
    function percentLoss(initial, final) {
        if (initial < 0.1) return 0;
        return Math.round(100.0 * (1.0 - (final / initial)));
    }
    function simresult(sim_result, label) {
      if (!sim_result) {
          active_squad_tab.find(`.hide-when-no-${label}-result`).hide();
          return;
      }

      set_position(active_squad_tab.find(`.${label}-choke`), sim_result.choke_x, sim_result.choke_y);

      // These all come directly from the data
      active_squad_tab.find(`.${label}-decision`).text(sim_result.decision ? 'ATTACK' : 'RETREAT');
      active_squad_tab.find(`.${label}-units`)
          .text(valuepair(sim_result.myUnitCount, sim_result.enemyUnitCount) +
              (sim_result.enemyHasUndetectedUnits ? '*' : ''));
      active_squad_tab.find(`.${label}-initial-scores`).text(valuepair(sim_result.initialMine, sim_result.initialEnemy));
      active_squad_tab.find(`.${label}-final-scores`).text(valuepair(sim_result.finalMine, sim_result.finalEnemy));
      active_squad_tab.find(`.${label}-diff-scores`)
          .text(valuepair(sim_result.finalMine-sim_result.initialMine,
              sim_result.finalEnemy-sim_result.initialEnemy));
      active_squad_tab.find(`.${label}-distance-factor`).text(twodecimals(sim_result.distanceFactor));
      active_squad_tab.find(`.${label}-aggression`).text(twodecimals(sim_result.aggression));
      active_squad_tab.find(`.${label}-reinforcements`)
          .text(twodecimals(sim_result.reinforcementPercentage) + '@' + twodecimals(sim_result.closestReinforcements));

      // These are computed to match what we do for decision making
      active_squad_tab.find(`.${label}-percent-loss`).text(
          valuepair(percentLoss(sim_result.initialMine, sim_result.finalMine),
              percentLoss(sim_result.initialEnemy, sim_result.finalEnemy)));
      active_squad_tab.find(`.${label}-gain`)
          .text(sim_result.finalMine - sim_result.initialMine -
              (sim_result.finalEnemy - sim_result.initialEnemy));
      active_squad_tab.find(`.${label}-percent-gain`)
          .text(percentLoss(sim_result.initialEnemy, sim_result.finalEnemy) -
              percentLoss(sim_result.initialMine, sim_result.finalMine));

      if (sim_result.unitLog) {
          if (active_squad_tab.find(`.${label}-draw`).is(':checked')) {
              if (!sim_result.unitLogState) {
                  let endFrame = 0;
                  for (const log of Object.values(sim_result.unitLog)) endFrame = Math.max(endFrame, (log.length / 4) - 1);
                  sim_result.unitLogState = {
                      currentFrame: 0,
                      endFrame: endFrame
                  };
                  active_squad_tab.find(`.${label}-draw-frame-slider`).attr('max', ''+endFrame);
              } else {
                  sim_result.unitLogState.currentFrame = parseInt(active_squad_tab.find(`.${label}-draw-frame-slider`).val());
              }
              active_squad_tab.find(`.${label}-draw-frame`).text(sim_result.unitLogState.currentFrame);

              global_data.combatsim_draw = {
                  frame: sim_result.unitLogState.currentFrame,
                  data: sim_result.unitLog
              };

              active_squad_tab.find(`.hide-when-no-${label}-draw`).show();
              active_squad_tab.find(`.hide-when-no-${label}-unitlogs`).show();
          } else {
              active_squad_tab.find(`.hide-when-no-${label}-draw`).hide();
          }
      } else {
          active_squad_tab.find(`.hide-when-no-${label}-unitlogs`).hide();
      }

      active_squad_tab.find(`.hide-when-no-${label}-result`).show();
    }

    simresult(vanguard_cluster.sim_result, 'vanguard-sim');
    simresult(vanguard_cluster.regroup_sim_result, 'vanguard-regroup-sim');
  }
}
