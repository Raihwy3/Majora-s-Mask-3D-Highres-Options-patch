#!/usr/bin/env python3
"""Build the experimental v44 native menus and render-mode adapter."""
from pathlib import Path
import json
import os
import struct
import subprocess
import sys
import zipfile
from elftools.elf.elffile import ELFFile
from audit_baseline import records
from inspect_inputs import apply_bps, sha
from inspect_layouts import check as inspect_layout_contracts
from build_options_tabs import build as build_tabs_layout
from preview_options_tabs import preview as preview_tabs

ROOT = Path(__file__).resolve().parent
BASE = Path('/home/r/Downloads/0004000000125600.dec.code')
BPS = Path('/home/r/Downloads/code.bps')
TITLE = '0004000000125600'


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def branch(source, target, link=False):
    delta = target - source - 8
    if delta % 4 or not -(1 << 25) <= delta < (1 << 25):
        raise ValueError('Unrepresentable ARM branch')
    return struct.pack('<I', (0xeb000000 if link else 0xea000000) | ((delta >> 2) & 0xffffff))


def make_ips(spans):
    result = bytearray(b'PATCH')
    end = 0
    for offset, data in sorted(spans):
        if offset < end:
            raise ValueError('Overlapping output records')
        end = offset + len(data)
        while data:
            block, data = data[:65535], data[65535:]
            if offset == 0x454f46 or offset >= 1 << 24:
                raise ValueError('Unrepresentable IPS offset')
            result.extend(offset.to_bytes(3, 'big'))
            if len(set(block)) == 1:
                result.extend(b'\0\0' + len(block).to_bytes(2, 'big') + block[:1])
            else:
                result.extend(len(block).to_bytes(2, 'big') + block)
            offset += len(block)
    return bytes(result + b'EOF')


def patched(source, patch, capacity):
    result = bytearray(source)
    result.extend(bytes(capacity - len(result)))
    for offset, data in records(patch):
        if offset + len(data) > capacity:
            raise ValueError('Record exceeds the mapped executable')
        result[offset:offset + len(data)] = data
    return result


def normalized(image, address, size, translate=None):
    translate = translate or {}
    result = []
    for pc in range(address, address + size, 4):
        word = u32(image, pc - 0x100000)
        if word & 0x0e000000 == 0x0a000000:
            delta = word & 0xffffff
            if delta & 0x800000:
                delta -= 0x1000000
            result.append(('branch', word >> 24, pc + 8 + delta * 4))
        elif word & 0x0f7f0000 == 0x051f0000:
            delta = word & 4095
            if not word & (1 << 23):
                delta = -delta
            value = u32(image, pc + 8 + delta - 0x100000)
            result.append(('literal', word & ~0x00800fff, translate.get(value, value)))
        else:
            result.append(('word', word))
    return result


def main():
    subprocess.run(["make", "native", "check", "check-native", "check-menu", "check-render", "check-text", "check-rect", "check-style", "check-tabs", "check-scanout", "check-resolve", "check-depth", "check-settings", "check-motion"],
                   cwd=ROOT, check=True)
    asset_path=Path('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar')
    asset=asset_path.read_bytes()
    if asset[:4]!=b'GAR\x02' or u32(asset,4)!=len(asset):
        raise ValueError('Unexpected Options GAR header')
    count=struct.unpack_from('<H',asset,10)[0]
    table,data=struct.unpack_from('<II',asset,16)
    extracted={}
    for i in range(count):
        size,short,name=struct.unpack_from('<III',asset,table+12*i)
        key=asset[name:asset.index(0,name)].decode('ascii')
        at=u32(asset,data+4*i)
        if at+size>len(asset): raise ValueError('Options asset outside archive')
        extracted[key]=asset[at:at+size]
    required={
        'MenuOption.mfl':['page1_g','page2_g','btn_page1_g','btn_page2_g','btn_return_l'],
        'BasicPlate.mfl':['choice_text_g','switching_text_g','switching_volume_text_g',
                          'direct_left_l','direct_right_l','bb_pane00'],
        'CursorOption.mfl':['frame_base','frame_fx'],
    }
    for key,names in required.items():
        model=extracted[key]
        if model[:8]!=b'MFL '+struct.pack('<I',4):
            raise ValueError('Unsupported native layout version')
        for name in names:
            if b'\0'+name.encode()+b'\0' not in model:
                raise ValueError(f'Missing native widget {key}/{name}')
    layout_contracts=inspect_layout_contracts()
    source = BASE.read_bytes()
    if sha(source) != '050a58834365187ca67211eef0a29816715e9ab9f7289e8372b06fb906620580':
        raise ValueError('Unsupported stock executable')
    baseline = (ROOT / 'baseline/code.ips').read_bytes()
    if sha(baseline) != '33a5ed0ac0cfc95891b403bf828a05a2c7d529aca8254507a063b9fb864fc8df':
        raise ValueError('Unexpected high-res baseline')
    header = bytearray((ROOT / 'baseline/exheader.bin').read_bytes())
    if sha(header) != '323b95902562aaff41b93134c43d07165e0b878c4b771b1e2a56ca70b2d796d4':
        raise ValueError('Unexpected input header')
    header[0x248] |= 0x80
    struct.pack_into('<I',header,0x3f0,u32(header,0x3f0)|(1<<9))
    with (ROOT / 'build/native.elf').open('rb') as handle:
        elf = ELFFile(handle)
        table = elf.get_section_by_name('.symtab')
        symbols = {s.name: s['st_value'] for s in table.iter_symbols()}
        if any(s['sh_type'] in ('SHT_REL', 'SHT_RELA') and s['sh_size']
               for s in elf.iter_sections()):
            raise ValueError('Unresolved ELF relocations')
    boot = (ROOT / 'build/bootstrap.bin').read_bytes()
    module = (ROOT / 'build/module.bin').read_bytes()
    start, file_end = symbols['__module_start'], symbols['__module_file_end']
    if b'Joker.Main.EdgeText\0' in module:
        raise ValueError('Unsafe standalone EdgeText resource path reintroduced')
    if b'Joker.Main.MessageWindow\0' not in module:
        raise ValueError('Missing complete font-model resource path')
    rx_end, bss_end = symbols['__module_rx_end'], symbols['__module_bss_end']
    text_addr, text_pages = struct.unpack_from('<II', header, 0x10)
    ro_addr, ro_pages = struct.unpack_from('<II', header, 0x20)
    data_addr, data_pages = struct.unpack_from('<II', header, 0x30)
    old_bss = u32(header, 0x3c)
    old_end = data_addr + data_pages * 4096
    if start != old_end + ((old_bss + 4095) & ~4095):
        raise ValueError('Module overlaps the existing process image/BSS')
    if not start < rx_end <= file_end <= bss_end:
        raise ValueError('Invalid module layout')
    if boot[0x64:0x68] != bytes(4) or len(boot) > 0x120:
        raise ValueError('Bootstrap damages existing weak runtime handle/text boundary')
    if len(module) > file_end - start:
        raise ValueError('Module file exceeds its declared region')
    module = module.ljust(file_end - start, b'\0')
    data_size = file_end - data_addr
    struct.pack_into('<II', header, 0x34, data_size // 4096, data_size)
    struct.pack_into('<I', header, 0x3c, bss_end - file_end)
    for svc in (0x03, 0x23, 0x32, 0x33, 0x35, 0x70):
        for off in range(0x370, 0x3e0, 4):
            value = u32(header, off)
            if value >> 27 == 0b11110 and (value >> 24) & 7 == svc // 24:
                struct.pack_into('<I', header, off, value | (1 << (svc % 24)))
                break
        else:
            raise ValueError('No matching SVC descriptor bank')
    capacity = text_pages * 4096 + ro_pages * 4096 + data_size
    module_offset = text_pages * 4096 + ro_pages * 4096 + start - data_addr
    if module_offset != start - 0x100000:
        raise ValueError('Unexpected file/address mapping')
    replacements = {
        0x5bd0: 'mm_hires_create',
        0x6c58: 'mm_hires_bottom',
        0x8d74: 'mm_hires_setup',
    }
    spans = []
    dynamic_sites = {0x10ad8, 0x250bc, 0x258fc, 0x25ac8, 0x25bf4, 0x25c1c, 0x51be64}
    for off, data in records(baseline):
        if off == 0x51cee0 or off in dynamic_sites:
            continue
        if off in replacements:
            data = branch(off + 0x100000, symbols[replacements[off]], True)
        spans.append((off, data))
    restoration = apply_bps(source, BPS.read_bytes())
    if source[0x6bc8:0x6bcc]!=struct.pack('<f',10.0) or restoration[0x6bc8:0x6bcc]!=struct.pack('<f',10.0):
        raise ValueError('Native stereo depth default differs from verified10.0')
    for lo,hi in [(0x4a1e2c,0x4a2018),(0x2e904,0x2e980),(0x1006e4,0x1006f4),(0x92008,0x92018),(0x513b8,0x513c8),(0x69e50,0x69e60)]:
        if source[lo:hi]!=restoration[lo:hi]:
            raise ValueError(f'Restoration changes native input/UI API at {lo:#x}')
    hooks = []
    def add_hook(offset, target, original, link=False, size=4):
        if source[offset:offset + len(original)] != original:
            raise ValueError(f'Unexpected stock hook bytes at {offset:#x}')
        if restoration[offset:offset + len(original)] != original:
            raise ValueError(f'Restoration changes hook bytes at {offset:#x}')
        value = branch(offset + 0x100000, symbols[target], link)
        value += struct.pack('<I', 0xe320f000) * ((size - 4) // 4)
        spans.append((offset, value))
        hooks.append({'offset': hex(offset), 'target': target, 'target_address': hex(symbols[target]),
                      'linking': link, 'bytes_replaced': size})
    for offset, target, opcode in [
        (0x691c, 'mm_hook_stereo_depth', 0xed9f0aa9),
        (0x10ad8, 'mm_hook_rect_width', 0xe3a03e19),
        (0x258fc, 'mm_hook_copy_a', 0xe3a03e19),
        (0x25ac8, 'mm_hook_copy_b', 0xe3a03e19),
        (0x25bf4, 'mm_hook_total_width', 0xe3a0ce19),
        (0x25c1c, 'mm_hook_inverse_width', 0xe2692e19),
        (0x4a31bc, 'mm_options_draw_hook', 0xe92d4030),
        (0x4a3318, 'mm_options_close_hook', 0xe92d4070),
        (0x4a34d0, 'mm_options_finish_hook', 0xe92d41f0),
        (0x49b6a0, 'mm_main_calc_hook', 0xe92d4ff0),
        (0x49b27c, 'mm_main_draw_hook', 0xe92d4070),
    ]:
        add_hook(offset, target, struct.pack('<I', opcode))
    add_hook(0x250bc, 'mm_hook_scene_size', struct.pack('<3I', 0xe3a00e19, 0x13a030f0, 0x03a03e1e), size=12)
    add_hook(0x51be64, 'mm_render_stereo', struct.pack('<2I', 0xe59f0028, 0xe5d01084), size=8)
    for offset, target, original_target in [
        (0x16748, 'mm_cstick_speed', 0x12e904),
        (0x1675c, 'mm_cstick_speed', 0x12e904),
        (0x4a3e40, 'mm_options_input_hook', 0x5a2ac8),
        (0x4a3d9c, 'mm_options_touch_hook', 0x5a2580),
        (0x4a41f0, 'mm_help_calc_hook', 0x196c38),
        (0x15aa0, 'mm_render_transfer', 0x1254d8),
        (0x15b10, 'mm_render_transfer', 0x1254d8),
        (0x48b28, 'mm_render_present', 0x15bc60),
        (0x25594, 'mm_resolve_packet', 0x136890),
    ]:
        add_hook(offset, target, branch(offset + 0x100000, original_target, True), link=True)

    for name, offset in [('mm_game_options_draw', 0x4a31bc), ('mm_game_options_close', 0x4a3318),
                         ('mm_game_main_calc', 0x49b6a0), ('mm_game_main_draw', 0x49b27c),
                         ('mm_game_options_finish', 0x4a34d0)]:
        at = symbols[name] - start
        if module[at:at+4] != source[offset:offset+4]:
            raise ValueError(f'Trampoline prologue differs: {name}')
        if module[at+4:at+8] != branch(symbols[name]+4, offset+0x100004):
            raise ValueError(f'Trampoline continuation differs: {name}')
    spans.extend([
        (0, branch(0x100000, symbols['mm_bootstrap'])),
        (0x51cee0, boot),
        (old_end - 0x100000, bytes(start - old_end)),
        (module_offset, module),
    ])
    patch = make_ips(spans)
    stock_test = patched(source, patch, capacity)
    restoration = apply_bps(source, BPS.read_bytes())
    rst_test = patched(restoration, patch, capacity)
    stable_stock = patched(source, baseline, capacity)
    old_destroy = 0x61cf10
    new_destroy = symbols['mm_hires_create'] - 36
    translate = {old_destroy: new_destroy}
    for old, name, count in [
        (0x61cee0, 'mm_hires_setup', 32),
        (0x61cf00, 'mm_hires_bottom', 16),
        (0x61cf10, None, 36),
        (0x61cf48, 'mm_hires_create', 96),
    ]:
        new = symbols[name] if name else new_destroy
        if normalized(stable_stock, old, count, translate) != normalized(stock_test, new, count):
            raise ValueError(f'Relocated high-res routine differs at {old:#x}')
    allowed = [(o, o + len(d)) for o, d in records(patch)]
    for off in range(len(restoration)):
        if restoration[off] != rst_test[off] and not any(a <= off < z for a, z in allowed):
            raise ValueError('Unexpected Restoration modification')
    for off, count in [(0x51cd90, 0x150), (0x49ba10, 8), (0x676c, 4), (0x16a30, 4)]:
        if restoration[off:off + count] != rst_test[off:off + count]:
            raise ValueError('Existing Restoration hook damaged')
    for image in (stock_test, rst_test):
        if image[0x51cf44:0x51cf48] != bytes(4):
            raise ValueError('Weak-runtime handle changed')
        if any(image[old_end - 0x100000:start - 0x100000]):
            raise ValueError('Converted BSS not zero-initialized')
        if image[module_offset:module_offset + len(module)] != module:
            raise ValueError('Module not installed at linked address')
    output = ROOT / 'build/display-menu-v44'
    title_dir = output / 'luma/titles' / TITLE
    title_dir.mkdir(parents=True, exist_ok=True)
    (title_dir / 'code.ips').write_bytes(patch)
    (title_dir / 'exheader.bin').write_bytes(header)
    tab_layout=build_tabs_layout(title_dir / 'romfs/layout/Joker.Main.Play.Option.gar')
    preview_tabs()
    (output / 'tab-preview.png').write_bytes((ROOT / 'build/native-tabs/tab-preview.png').read_bytes())
    rollback = output / 'rollback'
    rollback.mkdir(exist_ok=True)
    with zipfile.ZipFile('/home/r/Downloads/mm3d-native-loader-test-v2.zip') as previous:
        rollback_ips = previous.read(f'luma/titles/{TITLE}/code.ips')
        rollback_header = previous.read(f'luma/titles/{TITLE}/exheader.bin')
    if sha(rollback_ips) != '6e9f3df070a5e0255977ae2d4478acb513aa3b5b7a1aafe29ed292361679b820':
        raise ValueError('Rollback is not the console-tested v2 patch')
    (rollback / 'code.ips').write_bytes(rollback_ips)
    (rollback / 'exheader.bin').write_bytes(rollback_header)
    (rollback / 'Joker.Main.Play.Option.gar').write_bytes(asset)
    report = {
        'stage': 'Experimental native Graphics page / START menu / Persistent settings, C-stick response and native UI feedback v44',
        'native_menu_and_renderer_host_checks_pass': True,
        'native_options_asset_sha256': sha(asset),
        'native_style': 'Three native sprite tabs, three native rows/cursor, six bottom labels and three top help labels and a native top-screen help panel with base layout updates',
        'native_tabs_layout_sha256': sha(tab_layout.read_bytes()),
        'native_tabs_layout_source_sha256': sha(asset),
        'native_tabs_hardware_tested': False,
        'native_tabs_asset_and_control_checks_pass': True,
        'native_show_property_word_regression_checked': True,
        'single_line_label_anchors_based_on_v15_console_capture': True,
        'descriptions_on_top_in_owned_native_help_panel': True,
        'tab_faces_before_backing_matching_original_asset': True,
        'software_compositor_reproduces_v16_occlusion': True,
        'closed_message_window_update_bypassed_on_owned_labels_only': True,
        'native_layout_contracts': layout_contracts,
        'unavailable_modes': [],
        'all_non_highres_modes_require_four_validated_backing_records': True,
        'packed_texture_copies_retain_512_by_800_backing': True,
        'native_arm_transfer_command_checks_pass': True,
        'appended_tab_backing_alpha': 255,
        'v19_hardware_result': 'All non-highres outputs black; stereo absent',
        'temporary_bottom_diagnostic_rows': False,
        'v20_hardware_result': 'Original F0121 E0 S0 G00 W400 B400 T400 X501 Eyes1 R1 Q0; 2D F0141 X500 otherwise same; both black',
        'diagnostic_mode_labels': {},
        'normal_mode_labels_restored': True,
        'diagnostic_partial_images_are_intentional': False,
        'v21_hardware_result': 'LCD test displays half-image stretched; Resolve test remains black',
        'trace_actual_gpu_dispatch_arguments': False,
        'trace_changes_no_gpu_arguments': True,
        'v23_hardware_result': 'High-res and SS2D display correctly; native2D black. SS2D was incorrectly assumed black before user correction.',
        'packed_source_origin': 'Physical framebuffer height minus active scene width (rotated Y), at 512 RGBA8 pixels per row',
        'native_400_scene_source_offset_bytes': 819200,
        'packed_resolve_staging_bytes': 1536000,
        'packed_resolve_region': 'FCRAM; one owned native GPU buffer',
        'native_queue_tracks_repack_and_resolve_separately': True,
        'ss3d_resolve_path': 'VRAM source plus819200 -> native cropped ScaleX resolve from512x400 tail; no staging TextureCopy',
        'ss3d_scene_per_eye': [400, 480],
        'stereo_strength_percent_choices': [0,50,75,100,125,150,175,200,250,320],
        'stereo_strength_default_percent': 100,
        'stereo_strength_labels': ['Off','Very low','Low','Normal','Higher','High','Very high','Extra high','Extreme','Maximum'],
        'stereo_view_removed': True,
        'view_and_projection_observation_hooks_removed': True,
        'gpu_dispatch_observer_removed': True,
        'bottom_diagnostic_title_and_status_removed': True,
        'bottom_owned_text': ['Display','selected display mode','3D strength','strength level','Look speed','speed level'],
        'native_tabs_digit3_font': 'Original Page2 numeral upper curve, mirrored for lower bowl; native atlas UV(125,102,15,8)',
        'native_tabs_digit3_geometry': [15,15],
        'settings_persistence': 'CRC-checked alternating SD files; read-back verification; load after native FS/GPU readiness',
        'cstick_sensitivity_percent_choices': [50,75,100,125,150,175,200],
        'cstick_sensitivity_default_percent': 100,
        'cstick_native_call_hooks': ['0x116748','0x11675c'],
        'cstick_speed_runs_after_native_positive_clamp': True,
        'cstick_raw_processing_call_unchanged': '0x121f0c',
        'look_speed_menu_label': 'Look speed',
        'primary_circle_pad_call_unchanged': '0x121f28',
        'native_ui_sounds': ['page','cursor','value','confirm','back'],
        'native_ui_animations': ['private native MenuOption vertical page entry/exit','native staggered row alpha','cursor fall/rise','BasicPlate arrow states'],
        'independent_page3_does_not_follow_hidden_page2_transform': True,
        'live_options_controller_not_replayed_for_page3_entry': True,
        'native_page_motion_has_bounded_visibility_fallback': True,
        'native_motion_reads_evaluated_y_at_widget_90': True,
        'native_motion_reads_evaluated_alpha_at_widget_d0': True,
        'native_stopped_player_detached_while_page3_owns_previous_pose': True,
        'stereo_strength_native_default': 10.0,
        'stereo_strength_load_hook': '0x10691c',
        'stereo_strength_native_literal_unchanged': True,
        'stereo_strength_session_only': False,
        'stereo_strength_controls': 'Up/Down choose row; Left/Right adjusts immediately; physical3D slider retained',
        'stereo_strength_arm_hook_checks_pass': True,
        'stereo_strength_hardware_verified': False,
        'ss3d_staging_allocation_retained_for_controlled_comparison': True,
        'other_modes_resolve_paths_unchanged_from_v25': True,
        'ss3d_scene_pixel_reduction_from_v25_percent': 50,
        'ss3d_performance_hardware_verified': False,
        'mono_secondary_framebuffer_aliases_primary': True,
        'narrow_output_requires_four_validated_800_column_backing_records': True,
        'native_arm_scanout_pointer_regression_checks_pass': True,
        'apply_available_modes': ['Original', 'Performance 2D', 'High-resolution 2D', 'Supersampling 2D', 'Supersampling + 3D'],
        'copy_rectangle_height_fix': 'Selected scene height/Y scale instead of native stereo-dependent 240/480',
        'native_arm_copy_rectangle_regression_checks_pass': True,
        'standalone_edge_text_constructor_path_disabled': True,
        'contains_locally_patched_supplied_options_layout': True,
        'hooks': hooks,
        'selectable_modes': ['Original', 'Performance 2D', 'High-resolution 2D', 'Supersampling 2D', 'Supersampling + 3D'],
        'mode_switching': 'Queued until native stereo/pass-state update; maximum backing storage retained',
        'sd_persistence': True,
        'new_modes_hardware_verified': False,
        'rollback': 'Console-tested native loader v2',
        'loader_v2_hardware_report': 'User launched game, loaded a save and walked around without a crash',
        'bootstrap_instruction_contract_checks_pass': True,
        'v1_bad_syscalls_and_wrong_register_regressions_rejected': True,
        'title_id': TITLE,
        'ips_sha256': sha(patch), 'header_sha256': sha(header),
        'bootstrap_bytes': len(boot), 'module_initialized_bytes': len(module),
        'module_rx_bytes': rx_end - start, 'module_bss_bytes': bss_end - file_end,
        'module_address': hex(start), 'mapped_code_bytes': capacity,
        'old_bss_preserved_as_zero_initialized_data': True,
        'all_four_relocated_highres_routines_match_baseline': True,
        'restoration_existing_startup_input_notebook_hooks_preserved': True,
        'verified_original_and_recovered_restoration_inputs': True,
        'hardware_tested': False,
    }
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'README.txt').write_text("MM3D EU display Options build v44\n\nINSTALL: Merge the complete luma/ folder onto the SD card, replacing all\nthree files together:\n  luma/titles/0004000000125600/code.ips\n  luma/titles/0004000000125600/exheader.bin\n  luma/titles/0004000000125600/romfs/layout/Joker.Main.Play.Option.gar\nKeep existing code.bps, other RomFS files and saved display-settings files.\n\nPAGE3: Display, 3D strength and Look speed. Descriptions stay on top. Black\ntab backing remains. Page3 digit is assembled from the original tab numeral strokes, including their outline and shading, with the duplicated middle pixel row removed. No 3D view or bottom diagnostic text.\n\nLOOK SPEED: Very low50%, Low75%, Normal100%, Higher125%, High150%,\nVery high175%, Maximum200%. v33/v34 scaled the secondary stick integers\nbefore native normalization. That native conversion caps positive values at\n1.0 while negative values can pass beyond-1.0, producing unequal gain.\nv35 restores the raw-processing call and applies the speed multiplier AFTER\nthe horizontal/vertical floating-point conversion at116748/11675c instead.\nNormal returns the exact original output. Primary Circle Pad, calibration,\ndead-zone, inversion and button bindings keep their original processing.\nExisting camera constraints still apply. Console testing is needed to verify\nthe resulting camera response across gameplay situations.\n\nANIMATIONS: Fixes v35's fade/overlap without scrolling. Native animation\nwrites an evaluated property block separately from each widget's base pose.\nThe adapter now reads evaluated Y at widget+90 and alpha at widget+d0, as\nconfirmed by the native animation dispatcher and property writers. Base Y\nat+40 and base alpha at+80 stay unchanged during playback.\n\nThe live previous-page animation is detached while Page3 controls its outgoing\nposition, so the native calculator cannot overwrite that movement with the\nstopped player's final pose. The old player is restored when returning;\na newly started native R transition is preserved. Setup no longer consumes\nan extra animation frame. The same private native MenuOption, original\nvertical slide/staggered fade tracks, input ownership and bounded visibility\nfallback from v35 remain. No animation curves or game textures are replaced.\n\nCONTROLS: R cycles Page1 -> Page2 -> Page3 -> Page1; L moves back.\nD-pad and Circle Pad Down continue from Page2 into Page3, then loops to Page1 after its last row. Up from the first Page3 row returns to the last Page2 row. Up/Down chooses a row; Left/Right changes its value. A applies Display.\nStrength and Look speed apply immediately. A also retries a failed save.\nB returns to Page2. START during gameplay opens Options and pauses game time.\nPage3 supports buttons and touch: tap a row to select it, tap arrows to change values, and tap the Display value to apply. Tap the bottom tabs to switch pages. The native B/Cancel button is visible and touch-enabled on Page3. The top heading uses the original Options orange (#FFBE00). Circle Pad directions and native held-direction repeat work on Page3. Up from the first Page1 row wraps to the last Page3 row. Row positions and spacing match the native page (Y=4/52/100). A held or dragged touch does not repeat. Inputs wait for page transitions.\n\nSAVED SETTINGS: Applied Display, 3D strength and Look speed use the same\nversioned two-file SD journal introduced in v33:\n  /luma/titles/0004000000125600/display-settings-a.bin\n  /luma/titles/0004000000125600/display-settings-b.bin\nExisting settings remain compatible. Each record has a sequence and CRC;\nwrites are flushed, closed and read back. Invalid/torn records fall back to\nthe previous valid record. Defaults are High-res, Normal strength and Normal\nLook speed. Loading waits for native FS/GPU readiness. A failed save leaves\nthe setting active for this run and shows a retry message on top. Delete both\nfiles to reset defaults. The ZIP contains no saved-settings file.\n\nRENDERING: Display profiles and SS3D400x480-per-eye vertical supersampling\nremain as in v34. No FPS, timing, bloom or stereo-projection change.\n\nVALIDATION: Required local loader/menu/render/text/layout/tab checks pass,\nas do SD journal/restart/torn-write tests. Speed tests cover both signs,\nfull/partial deflection, the positive-cap regression, and Normal identity.\nAnimation tests verify native opcodes, separate base/evaluated pose storage,\npaused-player restoration, supplied native slide/fade tracks, and check channel\nfollowing, input ownership through exit, and missing/stalled/invalid animation\nfallback and cleanup. Hook/API bytes are checked against the supplied stock\nand recovered Restoration executables. These are local checks, not console\nverification of camera feel, animation appearance, SD access or sounds.\n\nNo Project Restoration implementation is included. Original Options layout\nwidget/pane/animation indices and animation bytes are retained. The supplied\narchive is locally patched only for the native three-tab asset extension.\n\nROLLBACK: Older ZIPs remain in Downloads. Replace all three installation\nfiles together. Saved settings can be retained with v33/v34. rollback/ also\ncontains the older console-tested v2 code/header and original Options GAR.\n")
    destination = Path('/home/r/Downloads/mm3d-display-menu-v44.zip')
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(output))
    print(json.dumps(report, indent=2))
    print(destination)


if __name__ == '__main__':
    main()
