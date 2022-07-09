/*
 * Copyright (c) 2019-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

"use strict";

function cvis_dbg_drawcommands_update(global_data, cvis_state) {
  const COMMAND_DRAW_TEXT = 25;
  const COMMAND_DRAW_TEXT_SCREEN = 26;

  Module.clear_draw_commands();
  $('.cvis-cmd-draw').remove();
  var draw_commands = global_data['draw_commands'];
  if (draw_commands === undefined || $.isEmptyObject(draw_commands) || typeof draw_commands === 'string') return;

  var draw_commands_this_frame;

  // If there are combat sim draw commands, these override all others
  if (global_data.combatsim_draw) {
    draw_commands_this_frame = [];
    const idx_this_frame = global_data.combatsim_draw.frame * 4;
    const idx_next_frame = (global_data.combatsim_draw.frame + 1) * 4;
    for (let unit_id of cvis_state.selected_units) {
      const unitData = global_data.combatsim_draw.data[unit_id];
      if (!unitData) continue;

      // Unit is already dead at this frame
      if (idx_this_frame >= unitData.length) continue;

      // Draw movement from starting position
      if (idx_this_frame > 0) {
        draw_commands_this_frame.push({
          args: [unitData[0], unitData[1], 5, 255],
          code: 23,
          str: '.'
        });

        draw_commands_this_frame.push({
          args: [unitData[0], unitData[1], unitData[idx_this_frame], unitData[idx_this_frame + 1], 255],
          code: 20,
          str: '.'
        });
        if (draw_commands_this_frame[draw_commands_this_frame.length - 1].args[2] === undefined) debugger;
      }

      // Unit dies this frame - draw a red circle
      if (idx_next_frame >= unitData.length) {
        draw_commands_this_frame.push({
          args: [unitData[idx_this_frame], unitData[idx_this_frame + 1], 5, 111],
          code: 23,
          str: '.'
        });
        continue;
      }

      // Cooldown
      const cooldown = ''+unitData[idx_this_frame + 3];
      draw_commands_this_frame.push({
        args: [Math.max(unitData[idx_this_frame] - Math.floor(6 * cooldown.length / 2), 0), Math.max(unitData[idx_this_frame + 1] - 15, 0)],
        code: 25,
        str: ''+unitData[idx_this_frame + 3]
      });

      // Unit fired this frame - draw a circle and line to where it fired
      if (unitData[idx_next_frame + 3] > unitData[idx_this_frame + 3]) {
        // Draw a green circle and line to the target
        const targetData = global_data.combatsim_draw.data[unitData[idx_this_frame + 2]];
        if (targetData && targetData.length > idx_this_frame) {
          draw_commands_this_frame.push({
            args: [unitData[idx_this_frame], unitData[idx_this_frame + 1], targetData[idx_this_frame], targetData[idx_this_frame + 1], 117],
            code: 20,
            str: '.'
          });
          if (draw_commands_this_frame[draw_commands_this_frame.length - 1].args[2] === undefined) debugger;
        }
        draw_commands_this_frame.push({
          args: [unitData[idx_this_frame], unitData[idx_this_frame + 1], 5, 117],
          code: 23,
          str: '.'
        });
        continue;
      }

      // Current location
      draw_commands_this_frame.push({
        args: [unitData[idx_this_frame], unitData[idx_this_frame + 1], 5, 255],
        code: 23,
        str: '.'
      });
    }
    delete global_data.combatsim_draw;
  } else {
    draw_commands_this_frame = draw_commands[cvis_state.current_frame] || [];

    if (draw_commands[0]) {
      draw_commands_this_frame = draw_commands_this_frame.concat(draw_commands[0])
    }

    // Add unit-specific draw commands
    if (global_data.units_draw) {
      for (let unit_id of cvis_state.selected_units) {
        if (global_data.units_draw[unit_id] && typeof global_data.units_draw[unit_id] !== 'string' && global_data.units_draw[unit_id][cvis_state.current_frame]) {
          draw_commands_this_frame = draw_commands_this_frame.concat(global_data.units_draw[unit_id][cvis_state.current_frame])
        }
      }
    }
  }

  // Draw commands
  var screen_info = Module.get_screen_info();
  $.each(draw_commands_this_frame, function(_, command) {
    // Convert CherryPi IDs -> OpenBW IDs in arguments
    if ('cherrypi_ids_args_indices' in command) {
      if (!command.cherrypi_ids_args_indices.every(
          i => cvis_state.id2bw[command.args[i]] !== undefined))
        return;
      $.each(command.cherrypi_ids_args_indices, function(_, idx) {
        command.args[idx] = cvis_state.id2bw[command.args[idx]];
      });
      command.cherrypi_ids_args_indices = []; // So we don't do it twice
    }
    if (!Module.add_draw_command(command)) {
      console.log('Unable to draw command: ', command);
    }
  });
}
