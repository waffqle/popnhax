// clang-format off
#include <windows.h>
#include <process.h>
#include <psapi.h>
// clang-format on

#include <vector>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <io.h>

#define F_OK 0
#define access _access

#include "../util/search.h"

#include "../minhook/hde32.h"
#include "../minhook/include/MinHook.h"

#include "../popnhax/config.h"
#include "../util/log.h"
#include "../util/patch.h"
#include "../util/xmlprop.hpp"
#include "xmlhelper.h"
#include "translation.h"

#include "tableinfo.h"
#include "loader.h"


#include "SearchFile.h"

const char *g_game_dll_fn = nullptr;
const char *g_config_fn = nullptr;
FILE *g_log_fp = nullptr;


#define DEBUG 0

#if DEBUG == 1
double g_multiplier = 1.;
DWORD (*real_timeGetTime)();
DWORD patch_timeGetTime()
{
 static DWORD last_real = 0;
 static DWORD cumul_offset = 0;

 if (last_real == 0)
 {
     last_real = real_timeGetTime()*g_multiplier;
     return last_real;
 }

 DWORD real = real_timeGetTime()*g_multiplier;
 DWORD elapsed = real-last_real;
 if (elapsed > 16) cumul_offset+= elapsed;

 last_real = real;
 return real - cumul_offset;

}

bool patch_get_time()
{
    HMODULE hinstLib = GetModuleHandleA("winmm.dll");
    MH_CreateHook((LPVOID)GetProcAddress(hinstLib, "timeGetTime"), (LPVOID)patch_timeGetTime,
                      (void **)&real_timeGetTime);
    return true;
}

static void memdump(uint8_t* addr, uint8_t len)
{
    LOG("MEMDUMP  :\n");
    for (int i=0; i<len; i++)
    {
        LOG("%02x ", *addr);
        addr++;
        if ((i+1)%16 == 0)
            LOG("\n");
    }

}
#endif

uint8_t *add_string(uint8_t *input);

struct popnhax_config config = {};

PSMAP_BEGIN(config_psmap, static)
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, hidden_is_offset,
                                 "/popnhax/hidden_is_offset")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, iidx_hard_gauge,
                                 "/popnhax/iidx_hard_gauge")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, survival_gauge,
                                 "/popnhax/survival_gauge")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, survival_iidx,
                                 "/popnhax/survival_iidx")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, survival_spicy,
                                 "/popnhax/survival_spicy")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_fast_slow,
                                 "/popnhax/show_fast_slow")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_details,
                                 "/popnhax/show_details")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_offset,
                                 "/popnhax/show_offset")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, pfree,
                                 "/popnhax/pfree")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, quick_retire,
                                 "/popnhax/quick_retire")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, back_to_song_select,
                                 "/popnhax/back_to_song_select")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, score_challenge,
                                 "/popnhax/score_challenge")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_hd_timing,
                                 "/popnhax/force_hd_timing")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, force_hd_resolution,
                                 "/popnhax/force_hd_resolution")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_unlocks,
                                 "/popnhax/force_unlocks")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, audio_source_fix,
                                 "/popnhax/audio_source_fix")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, unset_volume,
                                 "/popnhax/unset_volume")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, event_mode,
                                 "/popnhax/event_mode")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, remove_timer,
                                 "/popnhax/remove_timer")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, freeze_timer,
                                 "/popnhax/freeze_timer")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, skip_tutorials,
                                 "/popnhax/skip_tutorials")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_full_opt,
                                 "/popnhax/force_full_opt")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, netvs_off,
                                 "/popnhax/netvs_off")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, guidese_off,
                                 "/popnhax/guidese_off")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_db,
                                 "/popnhax/patch_db")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_expansions,
                                 "/popnhax/disable_expansions")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_redirection,
                                 "/popnhax/disable_redirection")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_multiboot,
                                 "/popnhax/disable_multiboot")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_xml_auto,
                                 "/popnhax/patch_xml_auto")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, patch_xml_filename,
                                 "/popnhax/patch_xml_filename")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, practice_mode,
                                 "/popnhax/practice_mode")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, force_datecode,
                                 "/popnhax/force_datecode")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, network_datecode,
                                 "/popnhax/network_datecode")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_keysounds,
                                 "/popnhax/disable_keysounds")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, keysound_offset,
                                 "/popnhax/keysound_offset")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, audio_offset,
                                 "/popnhax/audio_offset")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, beam_brightness,
                                 "/popnhax/beam_brightness")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, fps_uncap,
                                 "/popnhax/fps_uncap")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_translation,
                                 "/popnhax/disable_translation")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, translation_debug,
                                 "/popnhax/translation_debug")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, enhanced_polling,
                                 "/popnhax/enhanced_polling")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, debounce,
                                 "/popnhax/debounce")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, enhanced_polling_stats,
                                 "/popnhax/enhanced_polling_stats")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, enhanced_polling_priority,
                                 "/popnhax/enhanced_polling_priority")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, hispeed_auto,
                                 "/popnhax/hispeed_auto")
                PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U16, struct popnhax_config, hispeed_default_bpm,
                                 "/popnhax/hispeed_default_bpm")
PSMAP_END

enum BufferIndexes {
    MUSIC_TABLE_IDX = 0,
    CHART_TABLE_IDX,
    STYLE_TABLE_IDX,
    FLAVOR_TABLE_IDX,
    CHARA_TABLE_IDX,
};

typedef struct {
    uint64_t type;
    uint64_t method;
    uint64_t offset;
    uint64_t expectedValue;
    uint64_t size;
} UpdateOtherEntry;

typedef struct {
    uint64_t type;
    uint64_t offset;
    uint64_t size;
} UpdateBufferEntry;

typedef struct {
    uint64_t method;
    uint64_t offset;
} HookEntry;

const size_t LIMIT_TABLE_SIZE = 5;
uint64_t limit_table[LIMIT_TABLE_SIZE] = {0};
uint64_t new_limit_table[LIMIT_TABLE_SIZE] = {0};
uint64_t buffer_addrs[LIMIT_TABLE_SIZE] = {0};

uint8_t *new_buffer_addrs[LIMIT_TABLE_SIZE] = {nullptr};

std::vector<UpdateBufferEntry *> buffer_offsets;
std::vector<UpdateOtherEntry *> other_offsets;
std::vector<HookEntry *> hook_offsets;

char *g_datecode_override = nullptr;

void (*real_asm_patch_datecode)();

void asm_patch_datecode() {
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov ecx,10\n");
    __asm("mov esi, %0\n": :"b"((int32_t) g_datecode_override));
    __asm("add edi, 6\n");
    __asm("rep movsb\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");
    real_asm_patch_datecode();
}

/* retrieve destination address when it checks for "soft/ext", then wait next iteration to overwrite */
uint8_t g_libavs_datecode_patch_state = 0;
char *datecode_property_ptr;

void (*real_asm_patch_datecode_libavs)();

void asm_patch_datecode_libavs() {
    if (g_libavs_datecode_patch_state == 2)
        __asm("jmp leave_datecode_patch\n");

    if (g_libavs_datecode_patch_state == 1) {
        __asm("push edx\n");
        __asm("push eax\n");
        __asm("push ecx\n");
        /* memcpy(datecode_property_ptr, g_datecode_override, 10);*/
        __asm("mov edx,_g_datecode_override\n");
        __asm("mov eax,_datecode_property_ptr\n");
        __asm("mov ecx,dword ptr ds:[edx]      \n");
        __asm("mov dword ptr ds:[eax],ecx      \n");
        __asm("mov ecx,dword ptr ds:[edx+4]    \n");
        __asm("mov dword ptr ds:[eax+4],ecx    \n");
        __asm("movzx edx,word ptr ds:[edx+8]   \n");
        __asm("mov word ptr ds:[eax+8],dx      \n");
        __asm("pop ecx\n");
        __asm("pop eax\n");
        __asm("pop edx\n");

        g_libavs_datecode_patch_state++;
        __asm("jmp leave_datecode_patch\n");
    }

    __asm("mov %0, [esp+0x38]\n":"=a"(datecode_property_ptr):);
    if ((uint32_t) datecode_property_ptr > 0x200000 && datecode_property_ptr[0] == 's' &&
        datecode_property_ptr[5] == 'e' && datecode_property_ptr[7] == 't') {
        __asm("mov %0, ecx\n":"=c"(datecode_property_ptr):);
        g_libavs_datecode_patch_state++;
    }
    __asm("leave_datecode_patch:\n");
    real_asm_patch_datecode_libavs();
}

void (*real_omnimix_patch_jbx)();

void omnimix_patch_jbx() {
    __asm("mov al, 'X'\n");
    __asm("mov byte [edi+4], al\n");
    real_omnimix_patch_jbx();
}

/* dummy function to replace real one when not found */
bool is_normal_mode_best_effort() {
    return true;
}

bool (*popn22_is_normal_mode)() = is_normal_mode_best_effort;

uint32_t g_startsong_addr = 0;
uint32_t g_transition_addr = 0;
uint32_t g_stage_addr = 0;
uint32_t g_score_addr = 0;
bool g_pfree_mode = false;
bool g_end_session = false;
bool g_return_to_options = false;
bool g_return_to_song_select = false;
bool g_return_to_song_select_num9 = false;

void (*real_screen_transition)();

void quickexit_screen_transition() {
    if (g_return_to_options) {
        __asm("mov dword ptr [edi+0x30], 0x1C\n");
        //flag is set back to false in the option select screen after score cleanup
    } else if (g_return_to_song_select) {
        __asm("mov dword ptr [edi+0x30], 0x17\n");

        __asm("push eax");
        __asm("call %0"::"a"(popn22_is_normal_mode));
        __asm("test al,al");
        __asm("pop eax");
        __asm("je skip_change_flag");

        if (g_pfree_mode) {
            g_return_to_song_select = false;
        }
        //flag is set back to false in hook_stage_increment otherwise
    }

    __asm("skip_change_flag:");
    g_end_session = false;
    real_screen_transition();
}

void (*real_retrieve_score)();

void quickretry_retrieve_score() {
    __asm("mov %0, esi\n":"=S"(g_score_addr): :);
    /* let's force E and fail medal for quick exit
     * (regular retire or end of song will overwrite)
     */
    auto *clear_type = (uint8_t *) (g_score_addr + 0x24);
    auto *clear_rank = (uint8_t *) (g_score_addr + 0x25);
    if (*clear_type == 0) {
        *clear_type = 1;
        *clear_rank = 1;
    }
    real_retrieve_score();
}

void quickexit_option_screen_cleanup() {
    if (g_return_to_options) {
        /* we got here from quickretry, cleanup score */
        __asm("push ecx\n");
        __asm("push esi\n");
        __asm("push edi\n");
        __asm("mov edi, %0\n": :"D"(g_score_addr));
        __asm("mov esi, edi\n");
        __asm("add esi, 0x560\n");
        __asm("mov ecx, 0x98\n");
        __asm("rep movsd\n");
        __asm("pop edi\n");
        __asm("pop esi\n");
        __asm("pop ecx\n");
        g_return_to_options = false;
    }
}

uint32_t g_addr_icca;

void (*real_option_screen)();

void quickexit_option_screen() {
    quickexit_option_screen_cleanup();

    __asm("push ebx\n");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 28\n"); // numpad8: 80 00 00 00
    __asm("cmp bl, 8\n");
    __asm("pop ebx\n");
    __asm("jne real_option_screen\n");

    /* numpad 8 is held, rewrite transition pointer */
    __asm("pop edi\n");
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("xor edx, edx\n");
    __asm("mov ecx, %0\n": :"c"(g_startsong_addr));
    __asm("push edx\n");
    __asm("push ecx\n");
    __asm("push edi\n");

    __asm("real_option_screen:\n");
    real_option_screen();
}

void (*real_option_screen_later)();

void backtosongselect_option_screen() {
    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("jne exit_back_select\n");
    g_return_to_song_select = true;
    g_return_to_song_select_num9 = true;

    __asm("exit_back_select:\n");

    real_option_screen_later();
}

void (*real_backtosongselect_option_screen_auto_leave)();

void backtosongselect_option_screen_auto_leave() {
    if (g_return_to_song_select_num9) {
        g_return_to_song_select_num9 = false;
        __asm("mov al, 1\n");
    }
    real_backtosongselect_option_screen_auto_leave();
}

void (*real_option_screen_yellow)();

void backtosongselect_option_yellow() {
    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("jne exit_back_select_yellow\n");

    g_return_to_song_select = true;
    g_return_to_song_select_num9 = true;

    __asm("exit_back_select_yellow:\n");
    real_option_screen_yellow();
}

uint32_t g_option_yellow_leave_addr = 0;

void (*real_backtosongselect_option_screen_yellow_auto_leave)();

void backtosongselect_option_screen_yellow_auto_leave() {
    if (g_return_to_song_select_num9) {
        g_return_to_song_select_num9 = false;
        __asm("push %0\n": :"m"(g_option_yellow_leave_addr));
        __asm("ret\n");
    }
    real_backtosongselect_option_screen_yellow_auto_leave();
}

static bool r_ran;
static bool regul_flg;
//static bool dj_auto;
static bool ex_res_flg;
static bool disp = false;
static bool use_sp_flg;
uint32_t ori_plop = 0;
uint32_t *g_plop_addr;
uint32_t add_stage_addr;

void (*restore_op)();

void restore_plop() {
    __asm("push esi\n");
    __asm("push ebx\n");
    __asm("mov eax, [%0]\n"::"a"(*g_plop_addr));
    __asm("mov ebx, %0\n"::"b"(ori_plop));
    __asm("mov byte ptr [eax+0x34], bl\n");
    __asm("pop ebx\n");
    __asm("pop esi\n");
    restore_op();
}


/*
numpad values:

   | <<24 <<28 <<16
---+---------------
 8 |   7    8    9
 4 |   4    5    6
 2 |   1    2    3
 1 |   0    00

 e.g. numpad 9 = 8<<16 = 00 08 00 00
      numpad 2 = 2<<28 = 20 00 00 00
*/
void (*real_game_loop)();

void quickexit_game_loop() {
    __asm("push ebx\n");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("je leave_song\n");

    __asm("shr ebx, 12\n"); // (adds to the previous shr 16) numpad8: 08 00 00 00
    __asm("cmp bl, 8\n");
    __asm("jne call_real\n");

    /* numpad 8 is pressed: quick retry if pfree is active */
    use_sp_flg = false;

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_check");

    if (!g_pfree_mode) {
        __asm("skip_pfree_check:");
        __asm("jmp call_real\n");
    }

    g_return_to_options = true;
    /* numpad 7 or 9 is pressed */
    __asm("leave_song:\n");
    __asm("mov eax, 1\n");
    __asm("pop ebx\n");
    __asm("ret\n");

    /* no quick exit */
    __asm("call_real:\n");
    __asm("pop ebx\n");

/* r2nk226 ついかテスト */
    if (regul_flg | r_ran) {
        use_sp_flg = true;
    }
    ex_res_flg = false;
    disp = true;
    real_game_loop();

}

int16_t g_keysound_offset = 0;

void (*real_eval_timing)();

void patch_eval_timing() {
    __asm("mov esi, %0\n": :"b"((int32_t) g_keysound_offset));
    real_eval_timing();
}

void (*real_result_loop)();

void quickexit_result_loop() {
    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");
    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("je quit_session\n");

    __asm("shr ebx, 12\n"); // (adds to the previous shr 16) numpad8: 08 00 00 00
    __asm("cmp bl, 8\n");
    __asm("jne call_real_result\n");

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_quickexit_pfree_check");

    if (!g_pfree_mode) {
        __asm("skip_quickexit_pfree_check:");
        __asm("jmp call_real_result\n");
    }

    g_return_to_options = true; //transition screen hook will catch it
    __asm("jmp call_real_result\n");

    __asm("quit_session:\n");
    g_end_session = true;
    g_return_to_options = false;
    /* set value 5 in g_stage_addr and -4 in g_transition_addr (to get fade to black transition) */
    __asm("mov ebx, %0\n": :"b"(g_stage_addr));
    __asm("mov dword ptr[ebx], 5\n");
    __asm("mov ebx, %0\n": :"b"(g_transition_addr));
    __asm("mov dword ptr[ebx], 0xFFFFFFFC\n"); //quit session

    disp = false;// 7.9 message off

    __asm("call_real_result:\n");
// r2nk226#1109 ついかテスト
    ex_res_flg = true;
    real_result_loop();
}

void (*real_result_button_loop)();

void quickexit_result_button_loop() {
    if (g_end_session || g_return_to_options) {
        //g_return_to_options is reset in quickexit_option_screen_cleanup(), g_end_session is reset in quickexit_screen_transition()
        __asm("mov al, 1\n");
    }
    real_result_button_loop();
}

uint32_t g_timing_addr = 0;
bool g_timing_require_update = false;

void (*real_set_timing_func)();

void modded_set_timing_func() {
    if (!g_timing_require_update) {
        real_set_timing_func();
        return;
    }

    /* timing contains offset, add to it instead of replace */
    __asm("push ebx\n");
    __asm("mov ebx, %0\n"::"m"(g_timing_addr));
    __asm("add [ebx], eax\n");
    __asm("pop ebx\n");

    g_timing_require_update = false;
}

volatile uint32_t g_masked_hidden = 0;

uint32_t g_show_hidden_addr = 0; /* offset from ESP at which hidden setting value is */
void (*real_show_hidden_result)();

void asm_show_hidden_result() {
    if (g_masked_hidden) {
        __asm("push edx\n");
        __asm("mov edx, esp\n");
        __asm("add edx, %0\n"::""(g_show_hidden_addr):);
        __asm("add edx, 4\n"); /* to account for the "push edx" */
        __asm("or dword ptr [edx], 0x00000001\n");
        g_masked_hidden = 0;
        __asm("pop edx\n");
    }

    __asm("call_real_hidden_result:\n");
    real_show_hidden_result();
}

void (*real_stage_update)();

void hook_stage_update() {
    __asm("mov ebx, dword ptr [esi+0x14]\n");
    __asm("lea ebx, [ebx+0xC]\n");
    __asm("mov %0, ebx\n":"=b"(g_transition_addr): :);

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_stage_update_pfree_check");

    if (!g_pfree_mode) {
        __asm("skip_stage_update_pfree_check:");
        real_stage_update();
    }
}

/* this hook is installed only when back_to_song_select is enabled and pfree is not */
void (*real_stage_increment)();

void hook_stage_increment() {
    if (!g_return_to_song_select)
        real_stage_increment();
    else
        g_return_to_song_select = false;
}

void (*real_check_music_idx)();

extern "C" void check_music_idx();

extern "C" int8_t check_music_idx_handler(int32_t music_idx, int32_t chart_idx, int32_t result) {
    int8_t override_flag = get_chart_type_override(new_buffer_addrs[MUSIC_TABLE_IDX], music_idx & 0xffff,
                                                   chart_idx & 0x0f);

    LOG("music_idx: %d, result: %d, override_flag: %d\n", music_idx & 0xffff, result, override_flag);

    if (override_flag != -1) {
        return override_flag;
    }

    return (music_idx & 0xffff) > limit_table[CHART_TABLE_IDX] ? 1 : result;
}

asm(
        ".global _check_music_idx\n"
        "_check_music_idx:\n"
        "       call [_real_check_music_idx]\n"
        "       movzx eax, al\n"
        "       push eax\n"
        "       push ebx\n"
        "       push esi\n"
        "       call _check_music_idx_handler\n"
        "       add esp, 12\n"
        "       ret\n"
        );

void (*real_check_music_idx_usaneko)();

extern "C" void check_music_idx_usaneko();
extern "C" int8_t check_music_idx_handler_usaneko(int32_t result, int32_t chart_idx, int32_t music_idx) {
    return check_music_idx_handler(music_idx, chart_idx, result);
}

asm(
        ".global _check_music_idx_usaneko\n"
        "_check_music_idx_usaneko:\n"
        "       push edi\n"
        "       push ebx\n"
        "       cmp eax, 0x18\n"
        "       setl al\n"
        "       movzx eax, al\n"
        "       push eax\n"
        "       call _check_music_idx_handler_usaneko\n"
        "       add esp, 12\n"
        "       jmp [_real_check_music_idx_usaneko]\n"
        );

char *parse_patchdb(const char *input_filename, char *base_data) {
    const char *folder = "data_mods\\";
    char *input_filepath = (char *) calloc(strlen(input_filename) + strlen(folder) + 1, sizeof(char));

    sprintf(input_filepath, "%s%s", folder, input_filename);

    property *config_xml = load_prop_file(input_filepath);

    free(input_filepath);

    char *target = (char *) calloc(64, sizeof(char));
    property_node_refer(config_xml, property_search(config_xml, nullptr, "/patches"), "target@", PROPERTY_TYPE_ATTR,
                        target, 64);

    READ_U32(config_xml, nullptr, "/patches/limits/music", limit_music)
    READ_U32(config_xml, nullptr, "/patches/limits/chart", limit_chart)
    READ_U32(config_xml, nullptr, "/patches/limits/style", limit_style)
    READ_U32(config_xml, nullptr, "/patches/limits/flavor", limit_flavor)
    READ_U32(config_xml, nullptr, "/patches/limits/chara", limit_chara)

    limit_table[MUSIC_TABLE_IDX] = limit_music;
    limit_table[CHART_TABLE_IDX] = limit_chart;
    limit_table[STYLE_TABLE_IDX] = limit_style;
    limit_table[FLAVOR_TABLE_IDX] = limit_flavor;
    limit_table[CHARA_TABLE_IDX] = limit_chara;

    READ_HEX(config_xml, nullptr, "/patches/buffer_base_addrs/music", buffer_addrs[MUSIC_TABLE_IDX])
    buffer_addrs[MUSIC_TABLE_IDX] =
            buffer_addrs[MUSIC_TABLE_IDX] > 0 ? (uint64_t) base_data + (buffer_addrs[MUSIC_TABLE_IDX] - 0x10000000)
                                              : buffer_addrs[MUSIC_TABLE_IDX];

    READ_HEX(config_xml, nullptr, "/patches/buffer_base_addrs/chart", buffer_addrs[CHART_TABLE_IDX])
    buffer_addrs[CHART_TABLE_IDX] =
            buffer_addrs[CHART_TABLE_IDX] > 0 ? (uint64_t) base_data + (buffer_addrs[CHART_TABLE_IDX] - 0x10000000)
                                              : buffer_addrs[CHART_TABLE_IDX];

    READ_HEX(config_xml, nullptr, "/patches/buffer_base_addrs/style", buffer_addrs[STYLE_TABLE_IDX])
    buffer_addrs[STYLE_TABLE_IDX] =
            buffer_addrs[STYLE_TABLE_IDX] > 0 ? (uint64_t) base_data + (buffer_addrs[STYLE_TABLE_IDX] - 0x10000000)
                                              : buffer_addrs[STYLE_TABLE_IDX];

    READ_HEX(config_xml, nullptr, "/patches/buffer_base_addrs/flavor", buffer_addrs[FLAVOR_TABLE_IDX])
    buffer_addrs[FLAVOR_TABLE_IDX] =
            buffer_addrs[FLAVOR_TABLE_IDX] > 0 ? (uint64_t) base_data + (buffer_addrs[FLAVOR_TABLE_IDX] - 0x10000000)
                                               : buffer_addrs[FLAVOR_TABLE_IDX];

    READ_HEX(config_xml, nullptr, "/patches/buffer_base_addrs/chara", buffer_addrs[CHARA_TABLE_IDX])
    buffer_addrs[CHARA_TABLE_IDX] =
            buffer_addrs[CHARA_TABLE_IDX] > 0 ? (uint64_t) base_data + (buffer_addrs[CHARA_TABLE_IDX] - 0x10000000)
                                              : buffer_addrs[CHARA_TABLE_IDX];

    printf("limit music: %lld\n", limit_table[MUSIC_TABLE_IDX]);
    printf("limit chart: %lld\n", limit_table[CHART_TABLE_IDX]);
    printf("limit style: %lld\n", limit_table[STYLE_TABLE_IDX]);
    printf("limit flavor: %lld\n", limit_table[FLAVOR_TABLE_IDX]);
    printf("limit chara: %lld\n", limit_table[CHARA_TABLE_IDX]);

    printf("buffer music: %llx\n", buffer_addrs[MUSIC_TABLE_IDX]);
    printf("buffer chart: %llx\n", buffer_addrs[CHART_TABLE_IDX]);
    printf("buffer style: %llx\n", buffer_addrs[STYLE_TABLE_IDX]);
    printf("buffer flavor: %llx\n", buffer_addrs[FLAVOR_TABLE_IDX]);
    printf("buffer chara: %llx\n", buffer_addrs[CHARA_TABLE_IDX]);

    const char *types[LIMIT_TABLE_SIZE] = {"music", "chart", "style", "flavor", "chara"};

    // Read buffers_patch_addrs
    for (size_t i = 0; i < LIMIT_TABLE_SIZE; i++) {
        const char strbase[] = "/patches/buffers_patch_addrs";
        size_t search_str_len = strlen(strbase) + 1 + strlen(types[i]) + 1;
        char *search_str = (char *) calloc(search_str_len, sizeof(char));

        if (!search_str) {
            printf("Couldn't create buffer of size %d\n", search_str_len);
            exit(1);
        }

        sprintf(search_str, "%s/%s", strbase, types[i]);
        printf("search_str: %s\n", search_str);

        property_node *prop = nullptr;
        if ((prop = property_search(config_xml, nullptr, search_str))) {
            for (; prop != nullptr; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx\n", offset);

                auto *buffer_offset = new UpdateBufferEntry();
                buffer_offset->type = i;
                buffer_offset->offset = offset;
                buffer_offsets.push_back(buffer_offset);
            }
        }
    }

    // Read other_patches
    for (size_t i = 0; i < LIMIT_TABLE_SIZE; i++) {
        const char strbase[] = "/patches/other_patches";
        size_t search_str_len = strlen(strbase) + 1 + strlen(types[i]) + 1;
        char *search_str = (char *) calloc(search_str_len, sizeof(char));

        if (!search_str) {
            printf("Couldn't create buffer of size %d\n", search_str_len);
            exit(1);
        }

        sprintf(search_str, "%s/%s", strbase, types[i]);
        printf("search_str: %s\n", search_str);

        property_node *prop = nullptr;
        if ((prop = property_search(config_xml, nullptr, search_str))) {
            for (; prop != nullptr; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                char methodStr[256] = {};
                property_node_refer(config_xml, prop, "method@", PROPERTY_TYPE_ATTR, methodStr, sizeof(methodStr));
                uint32_t method = atoi(methodStr);

                char expectedStr[256] = {};
                property_node_refer(config_xml, prop, "expected@", PROPERTY_TYPE_ATTR, expectedStr,
                                    sizeof(expectedStr));
                uint64_t expected = strtol((const char *) expectedStr, nullptr, 16);

                char sizeStr[256] = {};
                property_node_refer(config_xml, prop, "size@", PROPERTY_TYPE_ATTR, sizeStr, sizeof(sizeStr));
                uint64_t size = strtol((const char *) sizeStr, nullptr, 16);

                if (size == 0) {
                    // I can't think of any patches that require a different size than 4 bytes
                    size = 4;
                }

                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx, method = %d, expected = %llx, size = %lld\n", offset, method, expected, size);

                auto *other_offset = new UpdateOtherEntry();
                other_offset->type = i;
                other_offset->method = method;
                other_offset->offset = offset;
                other_offset->expectedValue = expected;
                other_offset->size = size;
                other_offsets.push_back(other_offset);
            }
        }
    }

    // Read hook_addrs
    {
        property_node *prop = nullptr;
        if ((prop = property_search(config_xml, nullptr, "/patches/hook_addrs/offset"))) {
            for (; prop != nullptr; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                char methodStr[256] = {};
                property_node_refer(config_xml, prop, "method@", PROPERTY_TYPE_ATTR, methodStr, sizeof(methodStr));
                uint32_t method = atoi(methodStr);

                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx\n", offset);

                auto *hook_offset = new HookEntry();
                hook_offset->offset = offset;
                hook_offset->method = method;
                hook_offsets.push_back(hook_offset);
            }
        }
    }

    return target;
}

static bool patch_purelong() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x80\x1A\x06\x00\x83\xFA\x08\x77\x08", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Couldn't find score increment function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 24;
        auto *patch_str = (uint8_t *) patch_addr;

        DWORD old_prot;
        VirtualProtect((LPVOID) patch_str, 20, PAGE_EXECUTE_READWRITE, &old_prot);
        /* replace cdq/idiv by xor/div in score increment computation to avoid score overflow */
        for (int i = 12; i >= 0; i--) {
            patch_str[i + 1] = patch_str[i];
        }
        patch_str[0] = 0x33;
        patch_str[1] = 0xD2;
        patch_str[3] = 0x34;
        VirtualProtect((LPVOID) patch_str, 20, old_prot, &old_prot);
    }

    return true;
}

static bool patch_datecode(char *datecode) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    g_datecode_override = strdup(datecode);

    {
        int64_t pattern_offset = search(data, dllSize, "\x8D\x44\x24\x10\x88\x4C\x24\x10\x88\x5C\x24\x11\x8D\x50\x01",
                                        15, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t) data + pattern_offset + 0x08;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) asm_patch_datecode,
                          (void **) &real_asm_patch_datecode);

            LOG("popnhax: datecode set to %s", g_datecode_override);
        } else {
            LOG("popnhax: Couldn't patch datecode\n");
            return false;
        }
    }

    if (!config.network_datecode) {
        LOG("\n");
        return true;
    }

    /* network_datecode is on: also patch libavs so that forced datecode shows in network packets */
    DWORD avsdllSize = 0;
    char *avsdata = getDllData("libavs-win32.dll", &avsdllSize);
    {
        int64_t pattern_offset = search(avsdata, avsdllSize, "\x57\x56\x89\x34\x24\x8B\xF2\x8B\xD0\x0F\xB6\x46\x2E", 13,
                                        0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t) avsdata + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) asm_patch_datecode_libavs,
                          (void **) &real_asm_patch_datecode_libavs);

            LOG(" (including network)\n");
        } else {
            LOG(" (WARNING: failed to apply to network)\n");
            return false;
        }
    }
    return true;
}

static bool patch_database(uint8_t force_unlocks) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    patch_purelong();

    {
        int64_t pattern_offset = search(data, dllSize, "\x8D\x44\x24\x10\x88\x4C\x24\x10\x88\x5C\x24\x11\x8D\x50\x01",
                                        15, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) omnimix_patch_jbx,
                          (void **) &real_omnimix_patch_jbx);

            LOG("popnhax: Patched X rev for omnimix\n");
        } else {
            LOG("popnhax: Couldn't find rev patch\n");
        }
    }

    char *target;

    if (config.patch_xml_auto) {
        const char *filename = nullptr;
        SearchFile s;
        uint8_t *datecode = nullptr;
        bool found = false;

        if (config.force_datecode[0] != '\0') {
            LOG("popnhax: auto detect patch file with datecode override %s\n", config.force_datecode);
            datecode = (uint8_t *) strdup(config.force_datecode);
        } else {
            LOG("popnhax: auto detect patch file from ea3-config\n");
            property *config_xml = load_prop_file("prop/ea3-config.xml");
            READ_STR_OPT(config_xml, property_search(config_xml, nullptr, "/ea3/soft"), "ext", datecode)
            free(config_xml);
        }

        if (datecode == nullptr) {
            LOG("popnhax: patch_db: failed to retrieve datecode from ea3-config. Please disable patch_xml_auto option and use patch_xml_filename to specify which file should be used.\n");
            return false;
        }

        LOG("popnhax: patch_db: datecode : %s\n", datecode);
        LOG("popnhax: patch_db: XML patch files search...\n");
        s.search("data_mods", "xml", false);
        auto result = s.getResult();

        LOG("popnhax: patch_db: found %d xml files in data_mods\n", result.size());
        for (uint16_t i = 0; i < result.size(); i++) {
            filename = result[i].c_str() + 10; // strip "data_mods\" since parsedb will prepend it...
            LOG("%d : %s\n", i, filename);
            if (strstr(result[i].c_str(), (const char *) datecode) != nullptr) {
                found = true;
                LOG("popnhax: patch_db: found matching datecode, end search\n");
                break;
            }
        }

        if (!found) {
            LOG("popnhax: patch_db: matching datecode not found, defaulting to latest patch file.\n");
        }

        LOG("popnhax: patch_db: using %s\n", filename);
        target = parse_patchdb(filename, data);

    } else {

        target = parse_patchdb(config.patch_xml_filename, data);

    }

    if (config.disable_redirection) {
        config.disable_expansions = true;
    }

    musichax_core_init(
            force_unlocks,
            !config.disable_expansions,
            !config.disable_redirection,
            target,

            data,

            limit_table[MUSIC_TABLE_IDX],
            &new_limit_table[MUSIC_TABLE_IDX],
            (char *) buffer_addrs[MUSIC_TABLE_IDX],
            &new_buffer_addrs[MUSIC_TABLE_IDX],

            limit_table[CHART_TABLE_IDX],
            &new_limit_table[CHART_TABLE_IDX],
            (char *) buffer_addrs[CHART_TABLE_IDX],
            &new_buffer_addrs[CHART_TABLE_IDX],

            limit_table[STYLE_TABLE_IDX],
            &new_limit_table[STYLE_TABLE_IDX],
            (char *) buffer_addrs[STYLE_TABLE_IDX],
            &new_buffer_addrs[STYLE_TABLE_IDX],

            limit_table[FLAVOR_TABLE_IDX],
            &new_limit_table[FLAVOR_TABLE_IDX],
            (char *) buffer_addrs[FLAVOR_TABLE_IDX],
            &new_buffer_addrs[FLAVOR_TABLE_IDX],

            limit_table[CHARA_TABLE_IDX],
            &new_limit_table[CHARA_TABLE_IDX],
            (char *) buffer_addrs[CHARA_TABLE_IDX],
            &new_buffer_addrs[CHARA_TABLE_IDX]
    );
    limit_table[STYLE_TABLE_IDX] = new_limit_table[STYLE_TABLE_IDX];

    if (config.disable_redirection) {
        LOG("Redirection-related code is disabled, buffer address, buffer size and related patches will not be applied");
        printf("Redirection-related code is disabled, buffer address, buffer size and related patches will not be applied");
        return true;
    }

    // Patch buffers
    for (auto &buffer_offset: buffer_offsets) {
        if (buffer_offset->offset == 0 || buffer_addrs[buffer_offset->type] == 0) {
            continue;
        }

        // buffer_base_offsets is required because it will point to the beginning of all of the buffers to calculate offsets
        uint64_t patch_addr = (int64_t) data + (buffer_offset->offset - 0x10000000);
        uint64_t cur_addr = *(int32_t *) patch_addr;

        uint64_t mem_addr =
                (uint64_t) new_buffer_addrs[buffer_offset->type] + (cur_addr - buffer_addrs[buffer_offset->type]);

        printf("Patching %llx -> %llx @ %llx (%llx - %llx = %llx)\n", cur_addr, mem_addr, buffer_offset->offset,
               cur_addr, buffer_addrs[buffer_offset->type], cur_addr - buffer_addrs[buffer_offset->type]);

        patch_memory(patch_addr, (char *) &mem_addr, 4);
    }

    LOG("popnhax: patched memory db locations\n");

    if (config.disable_expansions) {
        printf("Expansion-related code is disabled, buffer size and related patches will not be applied");
        LOG("Expansion-related code is disabled, buffer size and related patches will not be applied");
        return true;
    }

    for (auto &other_offset: other_offsets) {
        // Get current limit so it can be used for later calculations
        uint32_t cur_limit = limit_table[other_offset->type];
        uint32_t limit_diff = cur_limit < new_limit_table[other_offset->type]
                              ? new_limit_table[other_offset->type] - cur_limit
                              : 0;

        if (limit_diff != 0) {
            uint64_t cur_value = 0;

            if (other_offset->size == 1) {
                cur_value = *(uint8_t *) (data + (other_offset->offset - 0x10000000));
            } else if (other_offset->size == 2) {
                cur_value = *(uint16_t *) (data + (other_offset->offset - 0x10000000));
            } else if (other_offset->size == 4) {
                cur_value = *(uint32_t *) (data + (other_offset->offset - 0x10000000));
            } else if (other_offset->size == 8) {
                cur_value = *(uint64_t *) (data + (other_offset->offset - 0x10000000));
            }

            if (other_offset->method != 12 && cur_value != other_offset->expectedValue) {
                printf("ERROR! Expected %llx, found %llx @ %llx!\n", other_offset->expectedValue, cur_value,
                       other_offset->offset);
                LOG("ERROR! Expected %llx, found %llx @ %llx!\n", other_offset->expectedValue, cur_value,
                    other_offset->offset);
                exit(1);
            }

            uint64_t value = cur_value;
            uint32_t patch_size = 4;
            switch (other_offset->method) {
                // Add limit_diff to value
                case 0:
                    value = cur_value + limit_diff;
                    break;

                    // Add limit_diff * 4 to value
                case 1:
                    value = cur_value + (limit_diff * 4);
                    break;

                    // Add limit diff * 6 * 4 (number of charts * 4?)
                case 2:
                    value = cur_value + ((limit_diff * 6) * 4);
                    break;

                    // Add limit_diff * 6
                case 3:
                    value = cur_value + (limit_diff * 6);
                    break;

                    // Add limit_diff * 0x90
                case 4:
                    value = cur_value + (limit_diff * 0x90);
                    break;

                    // Add limit_diff * 0x48
                case 5:
                    value = cur_value + (limit_diff * 0x48);
                    break;

                    // Add limit_diff * 0x120
                case 6:
                    value = cur_value + (limit_diff * 0x120);
                    break;

                    // Add limit_diff * 0x440
                case 7:
                    value = cur_value + (limit_diff * 0x440);
                    break;

                    // Add limit_diff * 0x0c
                case 8:
                    value = cur_value + (limit_diff * 0x0c);
                    break;

                    // Add limit_diff * 0x3d0
                case 9:
                    value = cur_value + (limit_diff * 0x3d0);
                    break;

                    // Add (limit_diff * 0x3d0) + (limit_diff * 0x9c)
                case 10:
                    value = cur_value + (limit_diff * 0x3d0) + (limit_diff * 0x9c);
                    break;

                    // Add (limit_diff * 0x3d0) + (limit_diff * 0x9c) + (limit_diff * 0x2a0)
                case 11:
                    value = cur_value + (limit_diff * 0x3d0) + (limit_diff * 0x9c) + (limit_diff * 0x2a0);
                    break;

                    // NOP
                case 12:
                    value = 0x9090909090909090;
                    patch_size = other_offset->size;
                    break;

                default:
                    printf("Unknown command: %lld\n", other_offset->method);
                    break;
            }

            if (value != cur_value) {
                uint64_t patch_addr = (int64_t) data + (other_offset->offset - 0x10000000);
                patch_memory(patch_addr, (char *) &value, patch_size);

                printf("Patched %llx: %llx -> %llx\n", other_offset->offset, cur_value, value);
            }
        }
    }

    HMODULE _moduleBase = GetModuleHandle(g_game_dll_fn);
    for (auto &hook_offset: hook_offsets) {
        switch (hook_offset->method) {
            case 0: {
                // Peace hook
                printf("Hooking %llx %p\n", hook_offset->offset, _moduleBase);
                MH_CreateHook((void *) ((uint8_t *) _moduleBase + (hook_offset->offset - 0x10000000)),
                              (void *) &check_music_idx, (void **) &real_check_music_idx);
                break;
            }

            case 1: {
                // Usaneko hook
                printf("Hooking %llx %p\n", hook_offset->offset, _moduleBase);
                uint8_t nops[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
                patch_memory((uint64_t) ((uint8_t *) _moduleBase + (hook_offset->offset - 0x10000000) - 6),
                             (char *) &nops, 6);
                MH_CreateHook((void *) ((uint8_t *) _moduleBase + (hook_offset->offset - 0x10000000)),
                              (void *) &check_music_idx_usaneko, (void **) &real_check_music_idx_usaneko);
                break;
            }

            default:
                printf("Unknown hook command: %lld\n", hook_offset->method);
                break;
        }
    }

    LOG("popnhax: patched limit-related code\n");

    return true;
}

static bool patch_audio_source_fix() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x85\xC0\x75\x96\x8D\x70\x7F\xE8\xF8\x2B\x00\x00", 12, 0,
                            "\x90\x90\x90\x90", 4)) {
        return false;
    }

    LOG("popnhax: audio source fixed\n");

    return true;
}

static bool patch_unset_volume() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = search(data, dllSize, "\x04\x00\x81\xC4\x00\x01\x00\x00\xC3\xCC", 10, 0);
    if (first_loc == -1) {
        return false;
    }

    int64_t pattern_offset = search(data, 0x10, "\x83", 1, first_loc);
    if (pattern_offset == -1) {
        return false;
    }

    uint64_t patch_addr = (int64_t) data + pattern_offset;
    patch_memory(patch_addr, (char *) "\xC3", 1);

    LOG("popnhax: windows volume untouched\n");

    return true;
}

static bool patch_event_mode() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x8B\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
                                           "\xCC\xCC\xC7", 17, 0, "\x31\xC0\x40\xC3", 4)) {
        return false;
    }

    LOG("popnhax: event mode forced\n");

    return true;
}

static bool patch_remove_timer() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = search(data, dllSize, "\x8B\xAC\x24\x68\x01", 5, 0);
    if (first_loc == -1) {
        return false;
    }

    int64_t pattern_offset = search(data, 0x15, "\x0F", 1, first_loc);
    if (pattern_offset == -1) {
        return false;
    }

    uint64_t patch_addr = (int64_t) data + pattern_offset;
    patch_memory(patch_addr, (char *) "\x90\xE9", 2);

    LOG("popnhax: timer removed\n");

    return true;
}

static bool patch_freeze_timer() {
    if (!find_and_patch_hex(g_game_dll_fn, "\xC7\x45\x38\x09\x00\x00\x00", 7, 0, "\x90\x90\x90\x90\x90\x90\x90", 7)) {
        return false;
    }

    LOG("popnhax: timer frozen at 10 seconds remaining\n");

    return true;
}

static bool patch_skip_tutorials() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t first_loc = search(data, dllSize, "\xFD\xFF\x5E\xC2\x04\x00\xE8", 7, 0);
        if (first_loc == -1) {
            return false;
        }

        int64_t pattern_offset = search(data, 0x10, "\x74", 1, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        patch_memory(patch_addr, (char *) "\xEB", 1);
    }

    {
        int64_t pattern_offset = search(data, dllSize, "\x66\x85\xC0\x75\x5E\x6A", 6, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        patch_memory(patch_addr, (char *) "\x66\x85\xC0\xEB\x5E\x6A", 6);
    }

    {
        int64_t pattern_offset = search(data, dllSize, "\x00\x5F\x5E\x66\x83\xF8\x01\x75", 8, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        patch_memory(patch_addr, (char *) "\x00\x5F\x5E\x66\x83\xF8\x01\xEB", 8);
    }

    LOG("popnhax: menu and long note tutorials skipped\n");

    return true;
}

bool force_unlock_deco_parts() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x83\xC4\x04\x83\x38\x00\x75\x22", 8, 6, "\x90\x90", 2)) {
        LOG("popnhax: no deco parts to unlock\n");
        return false;
    }

    LOG("popnhax: unlocked deco parts\n");

    return true;
}

bool force_unlock_songs() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int music_unlocks = 0, chart_unlocks = 0;

    {
        // 0xac here is the size of music_entry. May change in the future
        int64_t pattern_offset = search(data, dllSize, "\x69\xC0\xAC\x00\x00\x00\x8B\x80", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: couldn't unlock songs and charts\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t *) ((int64_t) data + pattern_offset + 8);
        buffer_offset -= 0x1c; // The difference between music_entry.mask and music_entry.fw_genre_ptr to put it at the beginning of the entry
        auto *entry = (music_entry *) buffer_offset;

        for (int32_t i = 0;; i++) {
            // Detect end of table
            // Kind of iffy but I think it should work
            if (entry->charts[6] != 0 && entry->hold_flags[7] != 0) {
                // These should *probably* always be 0 but won't be after the table ends
                break;
            }

            if ((entry->mask & 0x08000000) != 0) {
                LOG("[%04d] Unlocking %s\n", i, entry->title_ptr);
                music_unlocks++;
            }

            if ((entry->mask & 0x00000080) != 0) {
                LOG("[%04d] Unlocking charts for %s\n", i, entry->title_ptr);
                chart_unlocks++;
            }

            if ((entry->mask & 0x08000080) != 0) {
                uint32_t new_mask = entry->mask & ~0x08000080;
                patch_memory((uint64_t) &entry->mask, (char *) &new_mask, 4);
            }

            entry++; // Move to the next music entry
        }
    }

    LOG("popnhax: unlocked %d songs and %d charts\n", music_unlocks, chart_unlocks);

    return true;
}

bool force_unlock_charas() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int chara_unlocks = 0;

    {
        // 0x4c here is the size of character_entry. May change in the future
        int64_t pattern_offset = search(data, dllSize, "\x98\x6B\xC0\x4C\x8B\x80", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: couldn't unlock characters\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t *) ((int64_t) data + pattern_offset + 6);
        buffer_offset -= 0x48; // The difference between character_entry.game_version and character_entry.chara_id_ptr to put it at the beginning of the entry
        auto *entry = (character_entry *) buffer_offset;

        for (int32_t i = 0;; i++) {
            // Detect end of table
            // Kind of iffy but I think it should work
            if (entry->_pad1[0] != 0 || entry->_pad2[0] != 0 || entry->_pad2[1] != 0 || entry->_pad2[2] != 0) {
                // These should *probably* always be 0 but won't be after the table ends
                break;
            }

            uint32_t new_flags = entry->flags & ~3;

            if (new_flags != entry->flags && entry->disp_name_ptr != nullptr &&
                strlen((char *) entry->disp_name_ptr) > 0) {
                LOG("Unlocking [%04d] %s... %08x -> %08x\n", i, entry->disp_name_ptr, entry->flags, new_flags);
                patch_memory((uint64_t) &entry->flags, (char *) &new_flags, sizeof(uint32_t));

                if ((entry->flavor_idx == 0 || entry->flavor_idx == -1)) {
                    int flavor_idx = 1;
                    patch_memory((uint64_t) &entry->flavor_idx, (char *) &flavor_idx, sizeof(uint32_t));
                    LOG("Setting default flavor for chara id %d\n", i);
                }

                chara_unlocks++;
            }

            entry++; // Move to the next character entry
        }
    }

    LOG("popnhax: unlocked %d characters\n", chara_unlocks);

    return true;
}

static bool patch_unlocks_offline() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize - 0xE0000, "\xB8\x49\x06\x00\x00\x66\x3B", 7, 0xE0000);
        if (pattern_offset == -1) {
            LOG("Couldn't find first song unlock\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 2;
        patch_memory(patch_addr, (char *) "\x90\x90", 2);
    }

    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xA9\x06\x00\x00\x68\x74", 6, 5, "\xEB", 1)) {
            LOG("Couldn't find second song unlock\n");
            return false;
        }
    }

    LOG("popnhax: songs unlocked for offline\n");

    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xA9\x10\x01\x00\x00\x74", 6, 5, "\xEB", 1)  /* unilab */
            && !find_and_patch_hex(g_game_dll_fn, "\xA9\x50\x01\x00\x00\x74", 6, 5, "\xEB", 1)) {
            LOG("Couldn't find character unlock\n");
            return false;
        }

        LOG("popnhax: characters unlocked for offline\n");
    }

    return true;
}

/* helper function to retrieve numpad status address */
static bool get_addr_icca(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\xE8\x4B\x14\x00\x00\x84\xC0\x74\x03\x33\xC0\xC3\x8B\x0D", 14, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = *(uint32_t *) ((int64_t) data + pattern_offset + 14);
#if DEBUG == 1
    LOG("ICCA MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve timing offset global var address */
static bool get_addr_timing_offset(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\xB8\xB4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
        return false;
    }

    uint32_t offset_delta = *(uint32_t *) ((int64_t) data + pattern_offset + 6);
    addr = *(uint32_t *) (((int64_t) data + pattern_offset + 10) + offset_delta + 1);
#if DEBUG == 1
    LOG("OFFSET MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve beam brightness address */
static bool get_addr_beam_brightness(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\xB8\x64\x00\x00\x00\xD9", 6, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t) data + pattern_offset + 1);
#if DEBUG == 1
    LOG("BEAM BRIGHTNESS MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve SD timing address */
static bool get_addr_sd_timing(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\xB8\xC4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t) data + pattern_offset + 1);
#if DEBUG == 1
    LOG("SD TIMING MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve HD timing address */
static bool get_addr_hd_timing(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\xB8\xB4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t) data + pattern_offset + 1);
#if DEBUG == 1
    LOG("HD TIMING MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

void (*real_commit_options)();

void hidden_is_offset_commit_options() {
    __asm("push eax\n");
    /* check if hidden active */
    __asm("xor eax, eax\n");
    __asm("mov eax, [esi]\n");
    __asm("shr eax, 0x18\n");
    __asm("or %0, eax\n":"=m"(g_masked_hidden):); /* save to restore display when using result_screen_show_offset patch */
    __asm("cmp eax, 1\n");
    __asm("jne call_real_commit\n");

    /* disable hidden ingame */
    __asm("and ecx, 0x00FFFFFF\n"); // -kaimei
    __asm("and edx, 0x00FFFFFF\n"); //  unilab

    /* flag timing for update */
    __asm("mov %0, 1\n":"=m"(g_timing_require_update):);
    //g_timing_require_update = true;

    /* write into timing offset */
    __asm("push ebx\n");
    __asm("movsx eax, word ptr [esi+4]\n");
    __asm("neg eax\n");
    __asm("mov ebx, %0\n"::"m"(g_timing_addr));
    __asm("mov [ebx], eax\n");
    __asm("pop ebx\n");

    /* quit */
    __asm("call_real_commit:\n");
    __asm("pop eax\n");
    real_commit_options();
}

static bool patch_hidden_is_offset() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    if (!get_addr_timing_offset(&g_timing_addr)) {
        LOG("popnhax: hidden is offset: cannot find timing offset address\n");
        return false;
    }

    /* patch option commit to store hidden value directly as offset */
    {
        /* find option commit function (unilab) */
        uint8_t shift = 6;
        int64_t pattern_offset = search(data, dllSize, "\x03\xC7\x8D\x44\x01\x2A\x89\x10", 8, 0);
        if (pattern_offset == -1) {
            /* wasn't found, look for older function */
            pattern_offset = search(data, dllSize, "\x0F\xB6\xC3\x03\xCF\x8D", 6, 0);
            shift = 14;

            if (pattern_offset == -1) {
                LOG("popnhax: hidden is offset: cannot find address\n");
                return false;
            }
#if DEBUG == 1
            LOG("popnhax: hidden is offset: appears to be kaimei or less\n");
#endif
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + shift;
        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hidden_is_offset_commit_options,
                      (void **) &real_commit_options);
    }

    /* turn "set offset" into an elaborate "sometimes add to offset" to use our hidden+ value as adjust */
    {
        char set_offset_fun[6] = "\xA3\x00\x00\x00\x00";
        auto *cast_code = (uint32_t *) &set_offset_fun[1];
        *cast_code = g_timing_addr;

        int64_t pattern_offset = search(data, dllSize, set_offset_fun, 5, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: hidden is offset: cannot find offset update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) modded_set_timing_func,
                      (void **) &real_set_timing_func);

    }

    LOG("popnhax: hidden is offset: hidden is now an offset adjust\n");
    return true;
}

static bool patch_show_hidden_adjust_result_screen() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = search(data, dllSize, "\x6A\x00\x0F\xBE\xCB", 5, 0);
    if (first_loc == -1)
        return false;


    int64_t pattern_offset = search(data, 0x200, "\x80\xBC\x24", 3, first_loc);
    if (pattern_offset == -1) {
        return false;
    }
    g_show_hidden_addr = *((uint32_t *) ((int64_t) data + pattern_offset + 0x03));
    uint64_t hook_addr = (int64_t) data + pattern_offset;
    MH_CreateHook((LPVOID) (hook_addr), (LPVOID) asm_show_hidden_result,
                  (void **) &real_show_hidden_result);


    LOG("popnhax: show hidden/adjust value on result screen\n");

    return true;
}

static bool force_show_fast_slow() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = search(data, dllSize, "\x6A\x00\x0F\xBE\xCB", 5, 0);
    if (first_loc == -1) {
        return false;
    }

    {
        int64_t pattern_offset = search(data, 0x50, "\x0F\x85", 2, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        patch_memory(patch_addr, (char *) "\x90\x90\x90\x90\x90\x90", 6);
    }

    LOG("popnhax: always show fast/slow on result screen\n");

    return true;
}


void (*real_show_detail_result)();

void hook_show_detail_result() {
    static uint32_t last_call = 0;

    __asm("push eax\n");
    __asm("push edx\n");

    uint32_t curr_time = timeGetTime();  //will clobber eax
    if (curr_time - last_call > 10000) //will clobber edx
    {
        last_call = curr_time;
        __asm("pop edx\n");
        __asm("pop eax\n");
        //force press yellow button
        __asm("mov al, 1\n");
    } else {
        last_call = curr_time;
        __asm("pop edx\n");
        __asm("pop eax\n");
    }

    real_show_detail_result();
}

static bool force_show_details_result() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = search(data, dllSize, "\x8B\x45\x48\x8B\x58\x0C\x6A\x09\x68\x80\x00\x00\x00", 13, 0);
    if (first_loc == -1) {
        LOG("popnhax: show details: cannot find result screen button check (1)\n");
        return false;
    }

    {
        int64_t pattern_offset = search(data, 0x50, "\x84\xC0", 2, first_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: show details: cannot find result screen button check (2)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_show_detail_result,
                      (void **) &real_show_detail_result);
    }

    LOG("popnhax: force show details on result screen\n");

    return true;
}


uint8_t g_pfree_song_offset = 0x54;
uint16_t g_pfree_song_offset_2 = 0x558;

void (*popn22_get_powerpoints)();

void (*popn22_get_chart_level)();

/* POWER POINTS LIST FIX */
uint8_t g_pplist_idx = 0;         // also serves as elem count
int32_t g_pplist[20] = {0};       // 20 elements for power_point_list (always ordered)
int32_t g_power_point_value = -1; // latest value (hook uses update_pplist() to add to g_pplist array)
int32_t *g_real_pplist;            // list that the game retrieves from server
uint32_t *allocated_pplist_copy;    // pointer to the location where the game's pp_list modified copy resides

void pplist_reset() {
    for (int &i: g_pplist)
        i = -1;
    g_pplist_idx = 0;
}

/* add new value (stored in g_power_point_value) to g_pp_list */
void pplist_update() {
    if (g_power_point_value == -1)
        return;

    if (g_pplist_idx == 20) {
        for (int i = 0; i < 19; i++) {
            g_pplist[i] = g_pplist[i + 1];
        }
        g_pplist_idx = 19;
    }

    g_pplist[g_pplist_idx++] = g_power_point_value;
    g_power_point_value = -1;
}

/* copy real pp_list to our local copy and check length */
void pplist_retrieve() {
    for (int i = 0; i < 20; i++) {
        g_pplist[i] = g_real_pplist[i];
    }

    /* in the general case your pplist will be full from the start so this is optimal */
    g_pplist_idx = 19;
    while (g_pplist[g_pplist_idx] == -1) {
        if (g_pplist_idx == 0)
            return;
        g_pplist_idx--;
    }
    g_pplist_idx++;
}

void (*real_pfree_pplist_init)();

void hook_pfree_pplist_init() {
    __asm("push eax");
    __asm("push ebx");
    __asm("lea ebx, [eax+0x1C4]\n");
    __asm("mov %0, ebx\n":"=m"(g_real_pplist));
    __asm("call %0\n"::"a"(pplist_retrieve));
    __asm("pop ebx");
    __asm("pop eax");
    real_pfree_pplist_init();
}

void (*real_pfree_pplist_inject)();

void hook_pfree_pplist_inject() {
    __asm("lea esi, %0\n"::"m"(g_pplist[g_pplist_idx]));
    __asm("mov dword ptr [esp+0x40], esi\n");

    __asm("lea esi, %0\n"::"m"(g_pplist));
    __asm("mov eax, dword ptr [esp+0x3C]\n");
    __asm("mov %0, eax\n":"=m"(allocated_pplist_copy));
    __asm("mov dword ptr [esp+0x3C], esi\n");
    __asm("movzx eax, %0\n"::"m"(g_pplist_idx));

    real_pfree_pplist_inject();
}

/* restore original pointer so that it can be freed */
void (*real_pfree_pplist_inject_cleanup)();

void hook_pfree_pplist_inject_cleanup() {
    __asm("mov esi, %0\n"::"m"(allocated_pplist_copy));
    __asm("call %0\n"::"a"(pplist_reset));

    real_pfree_pplist_inject_cleanup();
}

/* hook is installed in stage increment function */
void (*real_pfree_cleanup)();

void hook_pfree_cleanup() {
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_cleanup");

    __asm("push esi\n");
    __asm("push edi\n");
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("movzx eax, byte ptr [%0]\n"::"m"(g_pfree_song_offset));
    __asm("movzx ebx, word ptr [%0]\n"::"m"(g_pfree_song_offset_2));
    __asm("lea edi, dword ptr [esi+eax]\n");
    __asm("lea esi, dword ptr [esi+ebx]\n");
    __asm("push esi\n");
    __asm("push edi\n");

    /* compute powerpoints before cleanup */
    __asm("sub eax, 0x20\n"); // eax still contains g_pfree_song_offset
    __asm("neg eax\n");
    __asm("lea eax, dword ptr [edi+eax]\n");
    __asm("mov eax, dword ptr [eax]\n"); // music id (edi-0x38 or edi-0x34 depending on game)

    __asm("cmp ax, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jae cleanup_score\n");

    __asm("push 0\n");
    __asm("push eax\n");
    __asm("shr eax, 0x10\n"); //sheet id in al
    __asm("call %0\n"::"b"(popn22_get_chart_level));
    __asm("add esp, 8\n");
    __asm("mov bl, byte ptr [edi+0x24]\n"); // medal

    /* push "is full combo" param */
    __asm("cmp bl, 8\n");
    __asm("setae dl\n");
    __asm("movzx ecx, dl\n");
    __asm("push ecx\n");

    /* push "is clear" param */
    __asm("cmp bl, 4\n");
    __asm("setae dl\n");
    __asm("movzx ecx, dl\n");
    __asm("push ecx\n");

    __asm("mov ecx, eax\n"); // diff level
    __asm("mov eax, dword ptr [edi]\n"); // score
    __asm("call %0\n"::"b"(popn22_get_powerpoints));
    __asm("mov %0, eax\n":"=a"(g_power_point_value):);
    __asm("call %0\n"::"a"(pplist_update));

    __asm("cleanup_score:\n");
    /* can finally cleanup score */
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("mov ecx, 0x98");
    __asm("rep movsd");
    __asm("pop edx");
    __asm("pop eax");
    __asm("pop edi");
    __asm("pop esi");
    __asm("jmp cleanup_end");

    __asm("skip_pfree_cleanup:\n");
    real_pfree_cleanup();

    __asm("cleanup_end:\n");
}

/* hook without the power point fixes (eclale best effort) */
void hook_pfree_cleanup_simple() {
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_cleanup_simple");

    __asm("push esi\n");
    __asm("push edi\n");
    __asm("push eax\n");
    __asm("push ebx\n");
    __asm("push edx\n");
    __asm("movsx eax, byte ptr [%0]\n"::"m"(g_pfree_song_offset));
    __asm("movsx ebx, word ptr [%0]\n"::"m"(g_pfree_song_offset_2));
    __asm("lea edi, dword ptr [esi+eax]\n");
    __asm("lea esi, dword ptr [esi+ebx]\n");
    __asm("mov ecx, 0x98");
    __asm("rep movsd");
    __asm("pop edx");
    __asm("pop ebx");
    __asm("pop eax");
    __asm("pop edi");
    __asm("pop esi");
    __asm("ret");

    __asm("skip_pfree_cleanup_simple:\n");
    real_pfree_cleanup();
}

static bool patch_pfree() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    bool simple = false;
    pplist_reset();

    /* retrieve is_normal_mode function */
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xC4\x0C\x33\xC0\xC3\xCC\xCC\xCC\xCC\xE8", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find is_normal_mode function, fallback to best effort (active in all modes)\n");
        } else {
            popn22_is_normal_mode = (bool (*)()) (data + pattern_offset + 0x0A);
        }
    }

    /* stop stage counter (2 matches, 1st one is the good one) */
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xF8\x04\x77\x3E", 5, 0);
        if (pattern_offset == -1) {
            LOG("couldn't find stop stage counter\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x05;
        g_stage_addr = *(uint32_t *) (patch_addr + 1);

        /* hook to retrieve address for exit to thank you for playing screen */
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_stage_update,
                      (void **) &real_stage_update);

    }

    /* retrieve memory zone parameters to prepare for cleanup */
    char offset_from_base = 0x00;
    char offset_from_stage1[2] = {0x00, 0x00};
    int64_t child_fun_loc = 0;
    {
        int64_t offset = search(data, dllSize, "\x8D\x46\xFF\x83\xF8\x0A\x0F", 7, 0);
        if (offset == -1) {
#if DEBUG == 1
            LOG("popnhax: pfree: failed to retrieve struct size and offset\n");
#endif
            /* best effort for older games compatibility (works with eclale) */
            offset_from_base = 0x54;
            offset_from_stage1[0] = 0x04;
            offset_from_stage1[1] = 0x05;
            simple = true;
            goto pfree_apply;
        }
        uint32_t child_fun_rel = *(uint32_t *) ((int64_t) data + offset - 0x04);
        child_fun_loc = offset + child_fun_rel;
    }

    {
        int64_t pattern_offset = search(data, 0x40, "\xCB\x69", 2, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: failed to retrieve offset from stage1 (child_fun_loc = %llx\n", child_fun_loc);
            return false;
        }

        offset_from_stage1[0] = *(uint8_t *) ((int64_t) data + pattern_offset + 0x03);
        offset_from_stage1[1] = *(uint8_t *) ((int64_t) data + pattern_offset + 0x04);
#if DEBUG == 1
        LOG("popnhax: pfree: offset_from_stage1 is %02x %02x\n",offset_from_stage1[0],offset_from_stage1[1]);
#endif
    }

    {
        int64_t pattern_offset = search(data, 0x40, "\x8d\x74\x01", 3, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: failed to retrieve offset from base\n");
            return false;
        }

        offset_from_base = *(uint8_t *) ((int64_t) data + pattern_offset + 0x03);
#if DEBUG == 1
        LOG("popnhax: pfree: offset_from_base is %02x\n",offset_from_base);
#endif
    }

    pfree_apply:
    g_pfree_song_offset = offset_from_base;
    g_pfree_song_offset_2 = *((uint16_t *) offset_from_stage1);
    g_pfree_song_offset_2 += offset_from_base;

    /* cleanup score and stats */
    {
        int64_t pattern_offset = search(data, dllSize, "\xFE\x46\x0E\x80", 4, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find stage update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        /* replace stage number increment with a score cleanup function */
        if (simple) {
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_pfree_cleanup_simple,
                          (void **) &real_pfree_cleanup);
            LOG("popnhax: premium free enabled (WARN: no power points fix)\n");
            return true;
        }

        /* compute and save power points to g_pplist before cleaning up memory zone */
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_pfree_cleanup,
                      (void **) &real_pfree_cleanup);
    }

    /* fix power points */
    {
        int64_t pattern_offset = search(data, dllSize, "\x8A\xD8\x8B\x44\x24\x0C\xE8", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find get_power_points function\n");
            return false;
        }

        popn22_get_chart_level = (void (*)()) (data + pattern_offset - 0x07);
    }

    {
        int64_t pattern_offset = search(data, dllSize, "\x3D\x50\xC3\x00\x00\x7D\x05", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find get_power_points function\n");
            return false;
        }

        popn22_get_powerpoints = (void (*)()) (data + pattern_offset);
    }
    /* init pp_list */
    {
        int64_t pattern_offset = search(data, dllSize, "\x6B\xD2\x64\x2B\xCA\x51\x50\x68", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find power point load function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x1A;

        /* copy power point list to g_pplist on profile load and init g_pplist_idx */
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_pfree_pplist_init,
                      (void **) &real_pfree_pplist_init);
    }

    /* inject pp_list at end of credit */
    {
        int64_t pattern_offset = search(data, dllSize, "\x8B\x74\x24\x3C\x66\x8B\x04\x9E", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find end of credit power point handling function (1)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x07;

        /* make power point list pointers point to g_pplist at the end of processing */
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_pfree_pplist_inject,
                      (void **) &real_pfree_pplist_inject);
    }
    /* prevent crash when playing only customs in a credit */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\x8E\x5C\xFF\xFF\xFF\xEB\x04", 8, 6, "\x90\x90", 2)) {
            LOG("popnhax: pfree: cannot patch end list pointer\n");
        }
    }

    /* restore pp_list pointer so that it is freed at end of credit */
    {
        int64_t pattern_offset = search(data, dllSize, "\x7E\x04\x2B\xC1\x8B\xF8\x3B\xF5", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find end of credit power point handling function (2)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x06;

        /* make power point list pointers point to g_pplist at the end of processing */
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_pfree_pplist_inject_cleanup,
                      (void **) &real_pfree_pplist_inject_cleanup);
    }

    LOG("popnhax: premium free enabled\n");

    return true;
}

static bool patch_quick_retire(bool pfree) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    if (!pfree) {
        /* pfree already installs this hook
         */
        {
            int64_t pattern_offset = search(data, dllSize, "\x83\xF8\x04\x77\x3E", 5, 0);
            if (pattern_offset == -1) {
                LOG("couldn't find stop stage counter\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset - 0x05;
            g_stage_addr = *(uint32_t *) (patch_addr + 1);

            /* hook to retrieve address for exit to thank you for playing screen */
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_stage_update,
                          (void **) &real_stage_update);

        }

        /* prevent stage number increment when going back to song select without pfree */
        if (config.back_to_song_select) {
            int64_t pattern_offset = search(data, dllSize, "\xFE\x46\x0E\x80", 4, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: quick retire: cannot find stage update function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            /* hook to retrieve address for exit to thank you for playing screen */
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) hook_stage_increment,
                          (void **) &real_stage_increment);
        }

    }

    /* instant retire with numpad 9 in song */
    {
        int64_t pattern_offset = search(data, dllSize, "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x08\x0F\xBF\x05", 12, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve song loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca)) {
            LOG("popnhax: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickexit_game_loop,
                      (void **) &real_game_loop);
    }

    /* instant exit with numpad 9 on result screen */
    {
        int64_t pattern_offset = search(data, dllSize, "\xF8\x53\x55\x56\x57\x8B\xE9\x8B\x75\x00\x8B\x85", 12, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve result screen loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca)) {
            LOG("popnhax: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x05;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickexit_result_loop,
                      (void **) &real_result_loop);
    }

    /* no need to press red button when numpad 8 or 9 is pressed on result screen */
    {
        int64_t pattern_offset = search(data, dllSize, "\x84\xC0\x75\x0F\x8B\x8D\x1C\x0A\x00\x00\xE8", 11, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve result screen button check\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x1A;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickexit_result_button_loop,
                      (void **) &real_result_button_loop);

    }

    LOG("popnhax: quick retire enabled\n");

    /* retrieve songstart function pointer for quick retry */
    {
        int64_t pattern_offset = search(data, dllSize, "\xE9\x0C\x01\x00\x00\x8B\x85\x10\x0A\x00\x00", 11, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve song start function\n");
            return false;
        }
        uint64_t patch_addr = (int64_t) data + pattern_offset - 4;
        g_startsong_addr = *(uint32_t *) (patch_addr);
    }

    /* instant retry (go back to option select) with numpad 8 */
    {
        /* retrieve current stage score addr for cleanup (also used to fix quick retire medal) */
        int64_t pattern_offset = search(data, dllSize, "\xF3\xA5\x5F\x5E\x5B\xC2\x04\x00", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve score addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 5;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickretry_retrieve_score,
                      (void **) &real_retrieve_score);
    }
    {
        /* hook quick retire transition to go back to option select instead */
        int64_t pattern_offset = search(data, dllSize, "\x8B\xE8\x8B\x47\x30\x83\xF8\x17", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve screen transition function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickexit_screen_transition,
                      (void **) &real_screen_transition);
    }

    /* instant launch song with numpad 8 on option select (hold 8 during song for quick retry) */
    {
        int64_t pattern_offset = search(data, dllSize, "\x51\x50\x8B\x83\x0C\x0A\x00\x00\xEB\x09\x33\xD2", 12, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve option screen loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca)) {
            LOG("popnhax: quick retry: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 12 + 5 + 2;
        MH_CreateHook((LPVOID) patch_addr, (LPVOID) quickexit_option_screen,
                      (void **) &real_option_screen);
    }

    if (config.back_to_song_select) {
        /* go back to song select with numpad 9 on song option screen (before pressing yellow) */
        {
            int64_t pattern_offset = search(data, dllSize, "\x8B\x85\x0C\x0A\x00\x00\x83\x78\x34\x00\x75", 11, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) backtosongselect_option_screen,
                          (void **) &real_option_screen_later);
        }
        /* automatically leave option screen after numpad 9 press */
        {
            int64_t pattern_offset = search(data, dllSize,
                                            "\x84\xC0\x75\x63\x8B\x85\x10\x0A\x00\x00\x83\xC0\x04\xBF\x0C\x00\x00\x00",
                                            18, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) backtosongselect_option_screen_auto_leave,
                          (void **) &real_backtosongselect_option_screen_auto_leave);
        }

        /* go back to song select with numpad 9 on song option screen (after pressing yellow) */
        {
            int64_t pattern_offset = search(data, dllSize, "\x8B\x85\x0C\x0A\x00\x00\x83\x78\x38\x00\x75", 11, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: quick retry: cannot retrieve option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) backtosongselect_option_yellow,
                          (void **) &real_option_screen_yellow);
        }
        /* automatically leave after numpad 9 press */
        {
            int64_t pattern_offset = search(data, dllSize,
                                            "\x8B\x55\x00\x8B\x82\x9C\x00\x00\x00\x6A\x01\x8B\xCD\xFF\xD0\x80\xBD", 17,
                                            0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen yellow leave addr\n");
                return false;
            }

            g_option_yellow_leave_addr = (int32_t) data + pattern_offset - 0x05;
            pattern_offset = search(data, dllSize, "\x84\xC0\x0F\x84\xF1\x00\x00\x00\x8B\xC5", 10, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen yellow button check function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) patch_addr, (LPVOID) backtosongselect_option_screen_yellow_auto_leave,
                          (void **) &real_backtosongselect_option_screen_yellow_auto_leave);
        }

        LOG("popnhax: quick retire: return to song select enabled\n");
    }

    if (pfree)
        LOG("popnhax: quick retry enabled\n");

    return true;
}

static bool patch_add_to_base_offset(int8_t delta) {
    int32_t new_value = delta;
    char *as_hex = (char *) &new_value;

    LOG("popnhax: base offset: adding %d to base offset.\n", delta);

    /* call get_addr_timing_offset() so that it can still work after timing value is overwritten */
    uint32_t original_timing;
    get_addr_timing_offset(&original_timing);

    uint32_t sd_timing_addr;
    if (!get_addr_sd_timing(&sd_timing_addr)) {
        LOG("popnhax: base offset: cannot find base SD timing\n");
        return false;
    }

    int32_t current_value = *(int32_t *) sd_timing_addr;
    new_value = current_value + delta;
    patch_memory(sd_timing_addr, as_hex, 4);
    LOG("popnhax: base offset: SD offset is now %d.\n", new_value);


    uint32_t hd_timing_addr;
    if (!get_addr_hd_timing(&hd_timing_addr)) {
        LOG("popnhax: base offset: cannot find base HD timing\n");
        return false;
    }
    current_value = *(int32_t *) hd_timing_addr;
    new_value = current_value + delta;
    patch_memory(hd_timing_addr, as_hex, 4);
    LOG("popnhax: base offset: HD offset is now %d.\n", new_value);

    return true;
}

bool g_enhanced_poll_ready = false;

int (*usbPadRead)(uint32_t *);

uint32_t g_poll_rate_avg = 0;
uint32_t g_last_button_state = 0;
uint8_t g_debounce = 0;
int32_t g_button_state[9] = {0};

static unsigned int __stdcall enhanced_polling_stats_proc(void *ctx) {
    HMODULE hinstLib = GetModuleHandleA("ezusb.dll");
    usbPadRead = (int (*)(uint32_t *)) GetProcAddress(hinstLib, "?usbPadRead@@YAHPAK@Z");
    static uint8_t button_debounce[9] = {0};

    for (int &i: g_button_state) {
        i = -1;
    }

    while (!g_enhanced_poll_ready) {
        Sleep(500);
    }

    if (config.enhanced_polling_priority) {
        SetThreadPriority(GetCurrentThread(), config.enhanced_polling_priority);
        fprintf(stderr, "[Enhanced polling] Thread priority set to %d\n", GetThreadPriority(GetCurrentThread()));
    }

    uint32_t count = 0;
    uint32_t count_time = 0;

    uint32_t curr_poll_time = 0;
    uint32_t prev_poll_time = 0;
    while (g_enhanced_poll_ready) {
        uint32_t pad_bits;
        /* ensure at least 1ms has elapsed between polls
         * (beware of SD cab hardware compatibility)
         */
        curr_poll_time = timeGetTime();
        if (curr_poll_time == prev_poll_time) {
            curr_poll_time++;
            Sleep(1);
        }
        prev_poll_time = curr_poll_time;

        if (count == 0) {
            count_time = curr_poll_time;
        }

        usbPadRead(&pad_bits);
        g_last_button_state = pad_bits;

        unsigned int buttonState = (g_last_button_state >> 8) & 0x1FF;
        for (int i = 0; i < 9; i++) {
            if (((buttonState >> i) & 1)) {
                if (g_button_state[i] == -1) {
                    g_button_state[i] = curr_poll_time;
                }
                //debounce on release (since we're forced to stub usbPadReadLast)
                button_debounce[i] = g_debounce;
            } else {
                if (button_debounce[i] > 0) {
                    button_debounce[i]--;
                }

                if (button_debounce[i] == 0) {
                    g_button_state[i] = -1;
                } else {
                    //debounce ongoing: flag button as still pressed
                    g_last_button_state |= 1 << (8 + i);
                }
            }
        }
        count++;
        if (count == 100) {
            g_poll_rate_avg = timeGetTime() - count_time;
            count = 0;
        }
    }
    return 0;
}

static unsigned int __stdcall enhanced_polling_proc(void *ctx) {
    HMODULE hinstLib = GetModuleHandleA("ezusb.dll");
    usbPadRead = (int (*)(uint32_t *)) GetProcAddress(hinstLib, "?usbPadRead@@YAHPAK@Z");
    static uint8_t button_debounce[9] = {0};

    for (int &i: g_button_state) {
        i = -1;
    }

    while (!g_enhanced_poll_ready) {
        Sleep(500);
    }

    if (config.enhanced_polling_priority) {
        SetThreadPriority(GetCurrentThread(), config.enhanced_polling_priority);
        fprintf(stderr, "[Enhanced polling] Thread priority set to %d\n", GetThreadPriority(GetCurrentThread()));
    }

    uint32_t curr_poll_time = 0;
    uint32_t prev_poll_time = 0;
    while (g_enhanced_poll_ready) {
        uint32_t pad_bits;
        /* ensure at least 1ms has elapsed between polls
         * (beware of SD cab hardware compatibility)
         */
        curr_poll_time = timeGetTime();
        if (curr_poll_time == prev_poll_time) {
            curr_poll_time++;
            Sleep(1);
        }
        prev_poll_time = curr_poll_time;

        usbPadRead(&pad_bits);
        g_last_button_state = pad_bits;

        unsigned int buttonState = (g_last_button_state >> 8) & 0x1FF;
        for (int i = 0; i < 9; i++) {
            if (((buttonState >> i) & 1)) {
                if (g_button_state[i] == -1) {
                    g_button_state[i] = curr_poll_time;
                }
                //debounce on release (since we're forced to stub usbPadReadLast)
                button_debounce[i] = g_debounce;
            } else {
                if (button_debounce[i] > 0) {
                    button_debounce[i]--;
                }

                if (button_debounce[i] == 0) {
                    g_button_state[i] = -1;
                } else {
                    //debounce ongoing: flag button as still pressed
                    g_last_button_state |= 1 << (8 + i);
                }
            }
        }
    }
    return 0;
}

uint32_t buttonGetMillis(uint8_t button) {
    if (g_button_state[button] == -1)
        return 0;

    uint32_t but = g_button_state[button];
    uint32_t curr = timeGetTime();
    if (but <= curr) {
        uint32_t res = curr - but;
        return res;
    }

    return 0;
}

uint32_t usbPadReadHook_addr = 0;

int usbPadReadHook(uint32_t *pad_bits) {
    // if we're here then ioboard is ready
    g_enhanced_poll_ready = true;

    // return last known input
    *pad_bits = g_last_button_state;
    return 0;
}

uint32_t g_offset_fix[9] = {0};
uint8_t g_poll_index = 0;
uint32_t g_poll_offset = 0;

void (*real_enhanced_poll)();

void patch_enhanced_poll() {
    /* eax contains button being checked [0-8]
     * esi contains delta about to be evaluated
     * we need to do esi -= buttonGetMillis([%eax]); to fix the offset accurately */
    __asm("nop\n");
    __asm("nop\n");
    __asm("mov %0, al\n":"=m"(g_poll_index): :);
    g_poll_offset = buttonGetMillis(g_poll_index);
    __asm("sub esi, %0\n": :"b"(g_poll_offset));
    g_offset_fix[g_poll_index] = g_poll_offset;
    real_enhanced_poll();
}

static HANDLE enhanced_polling_thread;

static bool patch_enhanced_polling(uint8_t debounce, bool stats) {
    g_debounce = debounce;

    if (enhanced_polling_thread == nullptr) {
        if (stats) {
            enhanced_polling_thread = (HANDLE) _beginthreadex(
                    nullptr,
                    0,
                    enhanced_polling_stats_proc,
                    nullptr,
                    0,
                    nullptr);
        } else {
            enhanced_polling_thread = (HANDLE) _beginthreadex(
                    nullptr,
                    0,
                    enhanced_polling_proc,
                    nullptr,
                    0,
                    nullptr);
        }

    } // thread will remain dormant while g_enhanced_poll_ready == false

    /* patch eval timing function to fix offset depending on how long ago the button was pressed */
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\xC6\x44\x24\x0C\x00\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find eval timing function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x05;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) patch_enhanced_poll,
                      (void **) &real_enhanced_poll); // substract

    }

    /* patch calls to usbPadRead and usbPadReadLast */
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find usbPadRead call (1)\n");
            return false;
        }
        pattern_offset = search(data, dllSize - pattern_offset - 1, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7,
                                pattern_offset + 1);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find usbPadRead call (2)\n");
            return false;
        }
        usbPadReadHook_addr = (uint32_t) &usbPadReadHook;
        void *addr = (void *) &usbPadReadHook_addr;
        auto as_int = (uint32_t) addr;

        // game will call usbPadReadHook instead of real usbPadRead (function call hook in order not to interfere with tools)
        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x04; // usbPadRead function address
        patch_memory(patch_addr, (char *) &as_int, 4);

        // don't call usbPadReadLast as it messes with the 1000Hz polling, we'll be running our own debouncing instead
        patch_addr = (int64_t) data + pattern_offset - 20;
        patch_memory(patch_addr, (char *) "\x90\x90\x90\x90\x90\x90", 6);

    }

    LOG("popnhax: enhanced polling enabled");
    if (g_debounce != 0)
        LOG(" (%u ms debounce)", g_debounce);
    LOG("\n");

    return true;
}

void (*real_chart_load)();

void patch_chart_load_old() {
    /* This is mostly the same patch as the new version, except :
     * - eax and ecx have a different interpretation
     * - a chart chunk is only 2 dwords
     */
    __asm("cmp word ptr [edi+eax*8+4], 0x245\n");
    __asm("jne old_patch_chart_load_end\n");

    /* keysound event has been found, we need to convert timestamp */
    __asm("mov word ptr [edi+eax*8+4], 0x745\n"); // we'll undo it if we cannot apply it
    __asm("push eax\n"); // we'll convert to store chunk pointer (keysound timestamp at beginning)
    __asm("push ebx\n"); // we'll store a copy of our chunk pointer there
    __asm("push edx\n"); // we'll store the button info there
    __asm("push esi\n"); // we'll store chart growth for part2 subroutine there
    __asm("push ecx\n"); // we'll convert to store remaining chunks

    /* Convert eax,ecx registers to same format as later games so the rest of the patch is similar
     * TODO: factorize
     */
    __asm("sub ecx, eax\n"); // ecx is now remaining chunks rather than chart size
    __asm("imul eax, 8\n");
    __asm("add eax, edi\n"); //now eax is chunk pointer

    /* PART1: check button associated with keysound, then look for next note event for this button */
    __asm("xor dx, dx\n");
    __asm("mov dl, byte ptr [eax+7]\n");
    __asm("shr edx, 4\n"); //dl now contains button value ( 00-08 )

    __asm("mov ebx, eax\n"); //save chunk pointer into ebx for our rep movsd when a match is found
    __asm("old_next_chart_chunk:\n");
    __asm("add eax, 0x8\n"); // +0x08 in old chart format
    __asm("sub ecx, 1\n");
    __asm("jz old_end_of_chart\n");

    /* check where the keysound is used */
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne old_check_first_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne old_check_first_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    //keysound change for this button before we found a key using it :(
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [edi+eax*8+4], 0x0\n"); // disable operation, cannot be converted to a 0x745, and should not apply
    __asm("jmp old_patch_chart_load_end\n");

    __asm("old_check_first_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne old_next_chart_chunk\n");
    /* found note event */
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne old_next_chart_chunk\n");
    /* found MATCHING note event */
    __asm("mov edx, dword ptr [ebx+4]\n"); //save operation (we need to shift the whole block first)

    /* move the whole block just before the note event to preserve timestamp ordering */
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov edi, ebx\n");
    __asm("mov esi, ebx\n");
    __asm("add esi, 0x08\n");
    __asm("mov ecx, eax\n");
    __asm("sub ecx, 0x08\n");
    __asm("sub ecx, ebx\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event just before our matching 0x145 which eax points to */
    __asm("mov dword ptr [eax-0x08+0x04], edx\n"); // operation
    __asm("mov edx, dword ptr [eax]\n");
    __asm("mov dword ptr [eax-0x08], edx\n"); // timestamp

    /* PART2: Look for other instances of same button key events (0x0145) before the keysound is detached */
    __asm("xor esi, esi\n"); //init chart growth counter

    /* look for next note event for same button */
    __asm("mov ebx, dword ptr [eax-0x08+0x04]\n"); //operation copy
    __asm("mov dl, byte ptr [eax+6]\n"); //dl now contains button value ( 00-08 )

    __asm("old_next_same_note_chunk:\n");
    __asm("add eax, 0x8\n");
    __asm("sub ecx, 1\n");
    __asm("jz old_end_of_same_note_search\n"); //end of chart reached

    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne old_check_if_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne old_check_if_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    __asm("jmp old_end_of_same_note_search\n"); // found matching 0x245 (keysound change for this button), we can stop search

    __asm("old_check_if_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne old_next_same_note_chunk\n");
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne old_next_same_note_chunk\n");

    //found a match! time to grow the chart..
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov esi, eax\n");
    __asm("mov edi, eax\n");
    __asm("add edi, 0x08\n");
    __asm("imul ecx, 0x08\n"); //ecx is number of chunks left, we want number of bytes for now, dword later
    __asm("std\n");
    __asm("add esi, ecx\n");
    __asm("sub esi, 0x04\n"); //must be on very last dword from chart
    __asm("add edi, ecx\n");
    __asm("sub edi, 0x04\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("cld\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event copy */
    // timestamp is already correct as it's leftover from the 0x145
    __asm("mov dword ptr [eax+0x04], ebx\n"); // operation
    __asm("add esi, 1\n"); //increase growth counter

    /* ebx still contains the 0x745 operation, dl still contains button, but eax points to 0x745 rather than the 0x145 so let's fix */
    /* note that remaining chunks is still correct due to growth, so no need to decrement ecx */
    __asm("add eax, 0x8\n");
    __asm("jmp old_next_same_note_chunk\n"); //look for next occurrence

    /* KEYSOUND PROCESS END */
    __asm("old_end_of_same_note_search:\n");
    /* restore before next timestamp */
    __asm("pop ecx\n");
    __asm("add ecx, esi\n"); // take chart growth into account
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");

    /* next iteration should revisit the same block since we shifted... anticipate the +0xC/-1/+0x64 that will be done by real_chart_load() */
    __asm("sub eax, 1\n");
    __asm("sub dword ptr [edi+eax*8], 0x64\n");
    __asm("jmp old_patch_chart_load_end\n");

    __asm("old_end_of_chart:\n");
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [edi+eax*8+4], 0x245\n"); // no match found (ad-lib keysound?), restore opcode
    __asm("old_patch_chart_load_end:\n");
    real_chart_load();
}

void patch_chart_load() {
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne patch_chart_load_end\n");

    /* keysound event has been found, we need to convert timestamp */
    __asm("mov word ptr [eax+4], 0x745\n"); // we'll undo it if we cannot apply it
    __asm("push eax\n"); // chunk pointer (keysound timestamp at beginning)
    __asm("push ebx\n"); // we'll store a copy of our chunk pointer there
    __asm("push edx\n"); // we'll store the button info there
    __asm("push esi\n"); // we'll store chart growth for part2 subroutine there
    __asm("push ecx\n"); // remaining chunks

    /* PART1: check button associated with keysound, then look for next note event for this button */
    __asm("xor dx, dx\n");
    __asm("mov dl, byte ptr [eax+7]\n");
    __asm("shr edx, 4\n"); //dl now contains button value ( 00-08 )

    __asm("mov ebx, eax\n"); //save chunk pointer into ebx for our rep movsd when a match is found
    __asm("next_chart_chunk:\n");
    __asm("add eax, 0xC\n");
    __asm("sub ecx, 1\n");
    __asm("jz end_of_chart\n");

    /* check where the keysound is used */
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne check_first_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne check_first_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    //keysound change for this button before we found a key using it :(
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [eax+4], 0x0\n"); // disable operation, cannot be converted to a 0x745, and should not apply
    __asm("jmp patch_chart_load_end\n");

    __asm("check_first_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne next_chart_chunk\n");
    /* found note event */
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne next_chart_chunk\n");
    /* found MATCHING note event */
    __asm("mov edx, dword ptr [ebx+4]\n"); //save operation (we need to shift the whole block first)

    /* move the whole block just before the note event to preserve timestamp ordering */
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov edi, ebx\n");
    __asm("mov esi, ebx\n");
    __asm("add esi, 0x0C\n");
    __asm("mov ecx, eax\n");
    __asm("sub ecx, 0x0C\n");
    __asm("sub ecx, ebx\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event just before our matching 0x145 which eax points to */
    __asm("mov dword ptr [eax-0x0C+0x04], edx\n"); // operation
    __asm("mov edx, dword ptr [eax]\n");
    __asm("mov dword ptr [eax-0x0C], edx\n"); // timestamp
    __asm("mov dword ptr [eax-0x0C+0x08], 0x0\n"); // cleanup possible longnote duration leftover

    /* PART2: Look for other instances of same button key events (0x0145) before the keysound is detached */
    __asm("xor esi, esi\n"); //init chart growth counter

    /* look for next note event for same button */
    __asm("mov ebx, dword ptr [eax-0x0C+0x04]\n"); //operation copy
    __asm("mov dl, byte ptr [eax+6]\n"); //dl now contains button value ( 00-08 )

    __asm("next_same_note_chunk:\n");
    __asm("add eax, 0xC\n");
    __asm("sub ecx, 1\n");
    __asm("jz end_of_same_note_search\n"); //end of chart reached

    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne check_if_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne check_if_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    __asm("jmp end_of_same_note_search\n"); // found matching 0x245 (keysound change for this button), we can stop search

    __asm("check_if_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne next_same_note_chunk\n");
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne next_same_note_chunk\n");

    //found a match! time to grow the chart..
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov esi, eax\n");
    __asm("mov edi, eax\n");
    __asm("add edi, 0x0C\n");
    __asm("imul ecx, 0x0C\n"); //ecx is number of chunks left, we want number of bytes for now, dword later
    __asm("std\n");
    __asm("add esi, ecx\n");
    __asm("sub esi, 0x04\n"); //must be on very last dword from chart
    __asm("add edi, ecx\n");
    __asm("sub edi, 0x04\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("cld\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event copy */
    // timestamp is already correct as it's leftover from the 0x145
    __asm("mov dword ptr [eax+0x04], ebx\n"); // operation
    __asm("mov dword ptr [eax+0x08], 0x0\n"); // cleanup possible longnote duration leftover
    __asm("add esi, 1\n"); //increase growth counter

    /* ebx still contains the 0x745 operation, dl still contains button, but eax points to 0x745 rather than the 0x145 so let's fix */
    /* note that remaining chunks is still correct due to growth, so no need to decrement ecx */
    __asm("add eax, 0xC\n");
    __asm("jmp next_same_note_chunk\n"); //look for next occurrence

    /* KEYSOUND PROCESS END */
    __asm("end_of_same_note_search:\n");
    /* restore before next timestamp */
    __asm("pop ecx\n");
    __asm("add ecx, esi\n"); // take chart growth into account
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");

    /* next iteration should revisit the same block since we shifted... anticipate the +0xC/-1/+0x64 that will be done by real_chart_load() */
    __asm("sub eax, 0xC\n");
    __asm("add ecx, 1\n");
    __asm("sub dword ptr [eax], 0x64\n");
    __asm("jmp patch_chart_load_end\n");

    __asm("end_of_chart:\n");
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [eax+4], 0x245\n"); // no match found (ad-lib keysound?), restore opcode
    __asm("patch_chart_load_end:\n");
    real_chart_load();
}

static bool patch_disable_keysound() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x00\x00\x72\x05\xB9\xFF", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: keysound disable: cannot find offset\n");
            return false;
        }

        /* detect if usaneko+ */
        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x0F;
        uint8_t check_byte = *((uint8_t *) (patch_addr + 1));

        if (check_byte == 0x04) {
            LOG("popnhax: keysound disable: old game version\n");
            //return false;
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) patch_chart_load_old,
                          (void **) &real_chart_load);
        } else {
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) patch_chart_load,
                          (void **) &real_chart_load); //rewrite chart to get rid of keysounds
        }

        LOG("popnhax: no more keysounds\n");
    }

    return true;
}

static bool patch_keysound_offset(int8_t value) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    g_keysound_offset = -1 * value;

    patch_add_to_base_offset(value);

    {
        int64_t pattern_offset = search(data, dllSize, "\xC6\x44\x24\x0C\x00\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: keysound offset: cannot prepatch\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x07;
        patch_memory(patch_addr, (char *) "\x03", 1); // change "mov esi" into "add esi"

        MH_CreateHook((LPVOID) (patch_addr - 0x03), (LPVOID) patch_eval_timing,
                      (void **) &real_eval_timing); // preload esi with g_keysound_offset

        if (!config.audio_offset)
            LOG("popnhax: keysound offset: timing offset by %d ms\n", value);
        else
            LOG("popnhax: audio offset: audio offset by %d ms\n", -1 * value);
    }

    return true;
}

static bool patch_add_to_beam_brightness(int8_t delta) {
    int32_t new_value = delta;
    char *as_hex = (char *) &new_value;

    LOG("popnhax: beam brightness: adding %d to beam brightness.\n", delta);

    uint32_t beam_brightness_addr;
    if (!get_addr_beam_brightness(&beam_brightness_addr)) {
        LOG("popnhax: beam brightness: cannot find base address\n");
        return false;
    }

    int32_t current_value = *(int32_t *) beam_brightness_addr;
    new_value = current_value + delta;
    if (new_value < 0) {
        LOG("popnhax: beam brightness: fix invalid value (%d -> 0)\n", new_value);
        new_value = 0;
    }
    if (new_value > 255) {
        LOG("popnhax: beam brightness: fix invalid value (%d -> 255)\n", new_value);
        new_value = 255;
    }

    patch_memory(beam_brightness_addr, as_hex, 4);
    patch_memory(beam_brightness_addr + 0x39, as_hex, 4);
    LOG("popnhax: beam brightness is now %d.\n", new_value);

    return true;
}

static bool patch_beam_brightness(uint8_t value) {
    uint32_t newvalue = value;
    char *as_hex = (char *) &newvalue;
    bool res = true;

    /* call get_addr_beam_brightness() so that it can still work after base value is overwritten */
    uint32_t beam_brightness_addr;
    get_addr_beam_brightness(&beam_brightness_addr);

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\x64\x00\x00\x00\xD9", 6, 0x3A, as_hex, 4)) {
        LOG("popnhax: base offset: cannot patch HD beam brightness\n");
        res = false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\x64\x00\x00\x00\xD9", 6, 1, as_hex, 4)) {
        LOG("popnhax: base offset: cannot patch SD beam brightness\n");
        res = false;
    }

    return res;
}

uint16_t *g_course_id_ptr;
uint16_t *g_course_song_id_ptr;

void (*real_parse_ranking_info)();

void score_challenge_retrieve_addr() {
    /* only set pointer if g_course_id_ptr is not set yet,
     * to avoid overwriting with past challenge data which
     * is sent afterwards
     */
    __asm("mov ebx, %0\n": :"b"(g_course_id_ptr));
    __asm("test ebx, ebx\n");
    __asm("jne parse_ranking_info\n");

    __asm("lea %0, [esi]\n":"=S"(g_course_id_ptr): :);
    __asm("lea %0, [esi+0x8D4]\n":"=S"(g_course_song_id_ptr): :);
    __asm("parse_ranking_info:\n");
    real_parse_ranking_info();
}

void (*score_challenge_prep_songdata)();

void (*score_challenge_song_inject)();

void (*score_challenge_test_if_logged1)();

void (*score_challenge_test_if_normal_mode)();

void (*real_make_score_challenge_category)();

void make_score_challenge_category() {
    __asm("push ecx\n");
    __asm("push ebx\n");
    __asm("push edi\n");

    if (g_course_id_ptr && *g_course_id_ptr != 0) {
        score_challenge_test_if_logged1();
        __asm("mov al, byte ptr ds:[eax+0x1A5]\n"); /* or look for this function 8A 80 A5 01 00 00 C3 CC */
        __asm("test al, al\n");
        __asm("je leave_score_challenge\n");

        score_challenge_test_if_normal_mode();
        __asm("test al, al\n");
        __asm("jne leave_score_challenge\n");

        __asm("mov cx, %0\n": :"r"(*g_course_song_id_ptr));
        __asm("mov dl,6\n");
        __asm("lea eax,[esp+0x8]\n");
        score_challenge_prep_songdata();
        __asm("lea edi,[esi+0x24]\n");
        __asm("mov ebx,eax\n");
        score_challenge_song_inject();
        __asm("mov byte ptr ds:[esi+0xA4],1\n");
    }

    __asm("leave_score_challenge:\n");
    __asm("pop edi\n");
    __asm("pop ebx\n");
    __asm("pop ecx\n");
}

/* all code handling score challenge is still in the game but the
 * function responsible for building and adding the score challenge
 * category to song selection has been stubbed. let's rewrite it
 */
static bool patch_score_challenge() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* Part1: retrieve course id and song id, useful and will simplify a little */
    {

        int64_t pattern_offset = search(data, dllSize, "\x81\xC6\xCC\x08\x00\x00\xC7\x44\x24", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find course/song address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) score_challenge_retrieve_addr,
                      (void **) &real_parse_ranking_info);
    }

    /* Part2: retrieve subfunctions which used to be called by the now stubbed function */
    {
        int64_t pattern_offset = search(data, dllSize, "\x66\x89\x08\x88\x50\x02", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find song data prep function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        score_challenge_prep_songdata = (void (*)()) patch_addr;
    }
    {
        int64_t pattern_offset = search(data, dllSize - 0x60000,
                                        "\x8B\x4F\x0C\x83\xEC\x10\x56\x85\xC9\x75\x04\x33\xC0\xEB\x08\x8B\x47\x14\x2B\xC1\xC1\xF8\x02\x8B\x77\x10\x8B\xD6\x2B\xD1\xC1\xFA\x02\x3B\xD0\x73\x2B",
                                        37, 0x60000);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find category song inject function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        score_challenge_song_inject = (void (*)()) patch_addr;
    }
    {
        int64_t pattern_offset = search(data, dllSize, "\x8B\x01\x8B\x50\x14\xFF\xE2\xC3\xCC\xCC\xCC\xCC", 12, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find check if logged function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x24;

        score_challenge_test_if_logged1 = (void (*)()) patch_addr;
    }
    {
        int64_t pattern_offset = search(data, dllSize, "\xF7\xD8\x1B\xC0\x40\xC3\xE8", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find check if logged function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x6;

        score_challenge_test_if_normal_mode = (void (*)()) patch_addr;
    }

    /* Part3: "unstub" the score challenge category creation */
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xF8\x10\x77\x75\xFF\x24\x85", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find category building loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x66;

        uint32_t function_offset = *((uint32_t *) (patch_addr + 0x01));
        uint64_t function_addr = patch_addr + 5 + function_offset;

        MH_CreateHook((LPVOID) (function_addr), (LPVOID) make_score_challenge_category,
                      (void **) &real_make_score_challenge_category);
    }

    LOG("popnhax: score challenge reactivated (requires server support)\n");
    return true;
}

static bool patch_base_offset(int32_t value) {
    char *as_hex = (char *) &value;
    bool res = true;

    /* call get_addr_timing_offset() so that it can still work after timing value is overwritten */
    uint32_t original_timing;
    get_addr_timing_offset(&original_timing);

    uint32_t sd_timing_addr;
    get_addr_sd_timing(&sd_timing_addr);

    uint32_t hd_timing_addr;
    get_addr_hd_timing(&hd_timing_addr);

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xC4\xFF\xFF\xFF", 5, 1, as_hex, 4)) {
        LOG("popnhax: base offset: cannot patch base SD timing\n");
        res = false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xB4\xFF\xFF\xFF", 5, 1, as_hex, 4)) {
        LOG("popnhax: base offset: cannot patch base HD timing\n");
        res = false;
    }

    return res;
}

static bool patch_hd_timing() {
    if (!patch_base_offset(-76)) {
        LOG("popnhax: HD timing: cannot set HD offset\n");
        return false;
    }

    LOG("popnhax: HD timing forced\n");
    return true;
}

static bool patch_hd_resolution(uint8_t mode) {
    if (mode > 2) {
        LOG("ponhax: HD resolution invalid value %d\n", mode);
        return false;
    }

    /* set popkun and beam brightness to 85 instead of 100, like HD mode does */
    if (!patch_beam_brightness(85)) {
        LOG("popnhax: HD resolution: cannot set beam brightness\n");
        return false;
    }

    /* set window to 1360*768 */
    if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB6\xC0\xF7\xD8\x1B\xC0\x25\xD0\x02", 10, -5,
                            "\xB8\x50\x05\x00\x00\xC3\xCC\xCC\xCC", 9)
        && !find_and_patch_hex(g_game_dll_fn, "\x84\xc0\x74\x14\x0f\xb6\x05", 7, -5,
                               "\xB8\x50\x05\x00\x00\xC3\xCC\xCC\xCC", 9)) {
        LOG("popnhax: HD resolution: cannot find screen width function\n");
        return false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\x0f\xb6\xc0\xf7\xd8\x1b\xc0\x25\x20\x01", 10, -5,
                            "\xB8\x00\x03\x00\x00\xC3\xCC\xCC\xCC", 9))
        LOG("popnhax: HD resolution: cannot find screen height function\n");

    if (!find_and_patch_hex(g_game_dll_fn, "\x8B\x54\x24\x20\x53\x51\x52\xEB\x0C", 9, -6, "\x90\x90", 2))
        LOG("popnhax: HD resolution: cannot find screen aspect ratio function\n");


    if (mode == 1) {
        /* move texts (by forcing HD behavior) */
        if (!find_and_patch_hex(g_game_dll_fn, "\x1B\xC9\x83\xE1\x95\x81\xC1\x86", 8, -5,
                                "\xB9\xFF\xFF\xFF\xFF\x90\x90", 7))
            LOG("popnhax: HD resolution: cannot move gamecode position\n");

        if (!find_and_patch_hex(g_game_dll_fn, "\x6a\x01\x6a\x00\x50\x8b\x06\x33\xff", 9, -7, "\xEB", 1))
            LOG("popnhax: HD resolution: cannot move credit/network position\n");
    }

    LOG("popnhax: HD resolution forced%s\n", (mode == 2) ? " (centered texts)" : "");

    return true;
}

static bool patch_fps_uncap() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x7E\x07\xB9\x0C\x00\x00\x00\xEB\x09\x85\xC9", 11, 0, "\xEB\x1C", 2)) {
        LOG("popnhax: fps uncap: cannot find frame limiter\n");
        return false;
    }

    LOG("popnhax: fps uncapped\n");
    return true;
}

void (*real_song_options)();

void patch_song_options() {
    __asm("mov byte ptr[ecx+0xA15], 0\n");
    real_song_options();
}

void (*real_numpad0_options)();

void patch_numpad0_options() {
    __asm("mov byte ptr[ebp+0xA15], 1\n");
    real_numpad0_options();
}

/* r2nk226 */


/* playerdata */
static bool get_addr_pldata(uint32_t *res) {
    static uint32_t addr = 0;

    if (addr != 0) {
        *res = addr;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\x14\xFF\xE2\xC3\xCC\xCC", 6, 0); // +10hからポインタ群がある
    if (pattern_offset == -1) {
        return false;
    }

    addr = *(uint32_t *) ((int64_t) data + pattern_offset + 0x31);// 取りたいのは+31hのとこ
    g_plop_addr = (uint32_t *) ((int64_t) data + pattern_offset + 0x31);

#if DEBUG == 1
    LOG("pldata is %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* Unilab Check */
static bool unilab_check() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\x03\xC7\x8D\x44\x01\x2A\x89\x10", 8, 0);
    if (pattern_offset == -1) {
        return false;
    }
    LOG("popnhax: guidese : Unilabooooooo\n");
    return true;
}

/* INPUT numkey */
uint32_t *input_func;

static bool get_addr_numkey() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = search(data, dllSize, "\x85\xC9\x74\x08\x8B\x01\x8B\x40\x24\x52\xFF\xD0", 12, 0);
    if (pattern_offset == -1) {
        return false;
    }

    input_func = (uint32_t *) ((int64_t) data + pattern_offset + 26);
#if DEBUG == 1
    LOG("INPUT num addr %p\n", input_func);
#endif
    return true;
}

/* R-RANDOM address */
uint32_t *g_ran_addr;
uint32_t *btaddr;
uint32_t *ran_func;

static bool get_addr_random() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn,
                            &dllSize); // hook \x83\xC4\x04\xB9\x02\x00\x00\x00 button \x03\xC5\x83\xF8\x09\x7C\xDE

    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xC4\x04\xB9\x02\x00\x00\x00", 8, 0);
        if (pattern_offset == -1) {
            return false;
        }

        g_ran_addr = (uint32_t *) ((int64_t) data + pattern_offset - 0x13);

    }
    {
        int64_t pattern_offset = search(data, dllSize, "\x51\x55\x56\xC7\x44\x24\x08\x00\x00\x00", 10, 0);
        if (pattern_offset == -1) {
            return false;
        }
        ran_func = (uint32_t *) ((int64_t) data + pattern_offset);
    }
    {
        int64_t pattern_offset = search(data, dllSize, "\x03\xC5\x83\xF8\x09\x7C\xDE", 7, 0);
        if (pattern_offset == -1) {
            return false;
        }

        btaddr = (uint32_t *) ((int64_t) data + pattern_offset - 20);

    }
#if DEBUG == 1
    LOG("popnhax: get_addr_random: g_ran_addr is %p\n", g_ran_addr);
    LOG("popnhax: get_addr_random: ran_func is %p\n", ran_func);
    LOG("popnhax: get_addr_random: btaddr is %x\n", *btaddr);
#endif
    return true;
}

/* R-RANDOM hook */
void (*real_get_random)();

void r_random() {
    if (r_ran) {
        __asm("mov eax, [%0]\n"::"a"(*g_plop_addr));
        __asm("mov al, byte ptr [eax+0x34]\n"); //+34がプレイオプション0正規、1ミラー、2乱、3S乱
        __asm("cmp al, 2\n");
        __asm("jae call_rran_end\n"); /* jae=jnc 正規とミラーのみ処理 */
        __asm("movzx eax, al\n");
        __asm("nop\n":"=a"(ori_plop)::); //もとのオプション退避
        __asm("push 0\n");
        __asm("mov ebx, %0\n"::"b"(*btaddr)); //mov ebx,1093A124
        __asm("call %0\n"::"a"(ran_func)); //call 100B3B80
        __asm("add esp, 4\n");
        /* 1093A124 にレーンNOランダム生成されるから
            0番目の数字を加算って処理をする
            0の場合は1番目の数字を加算        */
        __asm("lea ebx, dword ptr [%0]\n"::"b"(*btaddr));
        __asm("cmp word ptr [ebx], 0\n");
        __asm("jne lane_no\n");
        __asm("add ebx, 2\n");
        __asm("lane_no:\n");
        __asm("movzx ebx, word ptr [ebx]\n");

        __asm("push ebx\n"); //加算数値を退避

        __asm("lea eax, dword ptr [%0]\n"::"a"(*btaddr));

        /* lane create base*/
        if (ori_plop == 0) {
            __asm("mov edx, [%0]\n"::"d"(*g_plop_addr));
            __asm("mov byte ptr [edx+0x34], 1\n"); // ここでプレイオプションに1を書き込む。正規だけミラー偽装あとで戻すこと
            __asm("xor ecx, ecx\n");
            __asm("lane_seiki:\n");
            //__asm("lea eax, dword ptr [%0]\n"::"a"(*btaddr));
            __asm("mov word ptr [eax+ecx*2], cx\n"); //mov word ptr [0x1093A124 + eax*2], cx
            __asm("inc ecx\n");
            __asm("cmp ecx,9\n");
            __asm("jl lane_seiki\n");
            //__asm("jmp next_lane\n");
        } else if (ori_plop == 1) {
            __asm("xor ecx, ecx\n");
            __asm("lane_mirror:\n");
            __asm("mov edx, 8\n");
            __asm("sub edx, ecx\n");
            //__asm("lea eax, dword ptr [%0]\n"::"a"(*btaddr));
            __asm("mov word ptr [eax+ecx*2], dx\n");//mov word ptr [0x1093A124 + eax*2], dx
            __asm("inc ecx\n");
            __asm("cmp ecx,9\n");
            __asm("jl lane_mirror\n");
        }

        __asm("pop ebx\n"); //ebxに加算する数字が入ってる
        __asm("next_lane:\n");
        __asm("lea ebp, [eax + edi*2]\n");
        __asm("movzx edx, word ptr [ebp]\n");
        __asm("add dx, bx\n");
        __asm("cmp dx, 9\n");
        __asm("jc no_in\n");
        __asm("sub dx, 9\n");

        __asm("no_in:\n");
        __asm("mov word ptr [ebp], dx\n");
        __asm("inc edi\n");
        __asm("cmp edi,9\n");
        __asm("jl next_lane\n");
        __asm("call_rran_end:\n");
    }
    real_get_random();
}

/* Get address for Special Menu */
uint32_t *g_rend_addr;
uint32_t *font_color;
uint32_t *font_rend_func;

static bool get_rendaddr() {
    static uint32_t addr = 0;

    if (addr != 0) {
        return true;
    }
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize,
                                        "\x3b\xC3\x74\x13\xC7\x00\x02\x00\x00\x00\x89\x58\x04\x89\x58\x08", 16, 0);
        if (pattern_offset == -1) {
            return false;
        }
        g_rend_addr = (uint32_t *) ((int64_t) data + pattern_offset - 4);
        font_color = (uint32_t *) ((int64_t) data + pattern_offset + 36);
    }

    {
        int64_t pattern_offset = search(data, dllSize,
                                        "\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x8B\x4C\x24\x0C\x8D\x44\x24\x10",
                                        24, 0);
        if (pattern_offset == -1) {
            return false;
        }

        font_rend_func = (uint32_t *) ((int64_t) data + pattern_offset + 16);
    }

#if DEBUG == 1
    LOG("popnhax: get_rendaddr: g_rend_addr is %x\n", *g_rend_addr);
    LOG("popnhax: get_rendaddr: font_color is %x\n", *font_color);
    LOG("popnhax: get_rendaddr: font_rend_func is %p\n", font_rend_func);
#endif

    return true;
}

/* speed change */
uint32_t *g_2dx_addr;
uint32_t *g_humen_addr;
uint32_t *g_soflan_addr;

static bool get_speedaddr() {
    static uint32_t addr = 0;

    if (addr != 0) {
        return true;
    }
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xC4\x0C\x8B\xC3\x8D\x7C\x24", 8, 0);
        if (pattern_offset == -1) {
            return false;
        }

        g_2dx_addr = (uint32_t *) ((int64_t) data + pattern_offset + 16);

    }
    {
        int64_t pattern_offset = search(data, dllSize, "\x8B\x74\x24\x18\x8D\x0C\x5B\x8B\x54\x8E\xF4", 11, 0);
        if (pattern_offset == -1) {
            return false;
        }

        g_humen_addr = (uint32_t *) ((int64_t) data + pattern_offset);

    }
    {
        int64_t pattern_offset = search(data, dllSize, "\x0F\xBF\xC5\x83\xC4\x0C\x66\x89\x6E\x08", 10, 0);
        if (pattern_offset == -1) {
            return false;
        }

        g_soflan_addr = (uint32_t *) ((int64_t) data + pattern_offset + 10);

    }
#if DEBUG == 1
    LOG("popnhax: get_speedaddr: g_2dx_addr is %p\n", g_2dx_addr);
    LOG("popnhax: get_speedaddr: g_humen_addr is %p\n", g_humen_addr);
    LOG("popnhax: get_speedaddr: g_soflan_addr is %p\n", g_soflan_addr);
#endif
    return true;
}

void (*real_get_bpm)();

void regul_speed() {
    if (regul_flg) {
        __asm("mov bp, 0x64\n"); //BPM 100
        __asm("mov [esi+8], bp\n");
    }
    __asm("movsx eax, bp\n");
    real_get_bpm();
}

static uint32_t speed;

void (*real_2dx_addr)();

void ex_2dx_speed() {
    if (speed == 1) {
        __asm("mov dword ptr [edi+0x30], 0x89D0\n");// 44100Hz -> 35280Hz
        __asm("mov ecx, dword ptr [edi+0x34]\n");
        __asm("shl ecx, 0x02\n");
        __asm("mov eax, 0x33333334\n");
        __asm("mul ecx\n");
        __asm("mov eax, edx\n");
        __asm("shr eax, 0x1F\n");
        __asm("add eax, edx\n");
        __asm("mov dword ptr [edi+0x34], eax\n");
    } else if (speed == 2) {
        __asm("mov dword ptr [edi+0x30], 0x72D8\n");// 44100Hz -> 29400Hz
        __asm("mov ecx, dword ptr [edi+0x34]\n");
        __asm("shl ecx, 0x01\n");
        __asm("mov eax, 0x55555556\n");
        __asm("mul ecx\n");
        __asm("mov eax, edx\n");
        __asm("shr eax, 0x1F\n");
        __asm("add eax, edx\n");
        __asm("mov dword ptr [edi+0x34], eax\n");
    }
    real_2dx_addr();
}

void ex_chart() {
    use_sp_flg = true;
    __asm("mov al, byte ptr [esp+0x448]\n");
    __asm("cmp al, 1\n");
    __asm("mov ecx, dword ptr [esp+0x30]\n");
    __asm("jne call_ex\n");
    __asm("lea ecx, dword ptr [ecx+ecx*2]\n");
    __asm("shr ecx, 1\n");
    __asm("call_ex:\n");
    __asm("lea ecx, dword ptr [ecx-0x6C]\n");
    __asm("mov eax, 0x15555556\n");
    __asm("mul ecx\n");
    __asm("mov eax, edx\n");
    __asm("shr eax, 0x1F\n");
    __asm("add eax, edx\n");
    __asm("xor ecx, ecx\n");
    __asm("mov ebp, dword ptr [esp+0x2C]\n");
    __asm("lea ebx, dword ptr [ebp+0x6c]\n");
}

void (*real_humen_addr)();

void ex_humen_speed() {
    if (speed == 1) {
        __asm("push ebx\n");
        ex_chart();
        __asm("call_loop:\n");
        __asm("lea ebx, dword ptr [ebx+0xC]\n");
        __asm("mov edx, dword ptr [ebx]\n");
        __asm("lea edx, dword ptr [edx+edx*4]\n");
        __asm("shr edx, 2\n");
        __asm("mov dword ptr [ebx], edx\n");
        __asm("mov edx, dword ptr [ebx+8]\n");
        __asm("cmp edx, 0\n");
        __asm("je long_pop\n");
        __asm("lea edx, dword ptr [edx+edx*4]\n");
        __asm("shr edx, 2\n");
        __asm("mov dword ptr [ebx+8], edx\n");
        __asm("long_pop:\n");
        __asm("inc ecx\n");
        __asm("cmp eax, ecx\n");
        __asm("jnle call_loop\n");
        __asm("pop ebx\n");
    } else if (speed == 2) {
        __asm("push ebx\n");
        ex_chart();
        __asm("call_loop2:\n");
        __asm("lea ebx, dword ptr [ebx+0xC]\n");
        __asm("mov edx, dword ptr [ebx]\n");
        __asm("lea edx, dword ptr [edx+edx*2]\n");
        __asm("shr edx, 1\n");
        __asm("mov dword ptr [ebx], edx\n");
        __asm("mov edx, dword ptr [ebx+8]\n");
        __asm("cmp edx, 0\n");
        __asm("je long_pop2\n");
        __asm("lea edx, dword ptr [edx+edx*2]\n");
        __asm("shr edx, 1\n");
        __asm("mov dword ptr [ebx+8], edx\n");
        __asm("long_pop2:\n");
        __asm("inc ecx\n");
        __asm("cmp eax, ecx\n");
        __asm("jnle call_loop2\n");
        __asm("pop ebx\n");
    }
    real_humen_addr();
}

/*
void (*add_stage_ext)();
void clear_flg(){
    disp = 0;
    use_sp_flg = 0;
    add_stage_ext();
}
*/
#define COLOR_WHITE        0x00
#define COLOR_GREEN        0x10
#define COLOR_DARK_GREY    0x20
#define COLOR_YELLOW       0x30
#define COLOR_RED          0x40
#define COLOR_BLUE         0x50
#define COLOR_LIGHT_GREY   0x60
#define COLOR_LIGHT_PURPLE 0x70
#define COLOR_BLACK        0xA0
#define COLOR_ORANGE       0xC0

void enhanced_polling_stats_disp_sub();

const char menu_1[] = "--- Practice Mode ---";
//const char menu_2[] = "DJAUTO (numpad5) >> %s";
const char menu_2[] = "Scores are not recorded."; //NO CONTEST 表現がわからん
const char menu_3[] = "REGUL SPEED (numpad4) >> %s";
const char menu_4[] = "R-RANDOM (numpad6) >> %s";
const char menu_5[] = "SPEED (numpad5) >> %s";
//const char menu_6[] = "menu display on/off (numpad9)";
const char menu_7[] = "quick retry (numpad8)";
const char menu_8[] = "quick retire (numpad9)";
const char menu_6[] = "quit pfree mode (numpad9)";
const char menu_on[] = "ON";
const char menu_off[] = "OFF";
const char sp100[] = "100%";
const char sp80[] = "80%"; //1.25倍
const char sp67[] = "66.6%";//1.5倍
const char *flg_1; //dj_auto
const char *flg_2; //regul speed
const char *flg_3; //r-random
void (*real_aging_loop)();

void new_menu() {
    __asm("mov ecx, 4\n");
    __asm("call %0\n"::"a"(input_func));
    __asm("test al, al\n");
    __asm("je SW_4\n");
    regul_flg ^= 1;
    __asm("SW_4:\n");
    flg_2 = menu_off;
    if (regul_flg) {
        flg_2 = menu_on;
    }

    __asm("mov ecx, 6\n");
    __asm("call %0\n"::"a"(input_func));
    __asm("test al, al\n");
    __asm("je SW_6\n");
    r_ran ^= 1;
    __asm("SW_6:\n");
    flg_3 = menu_off;
    if (r_ran) {
        flg_3 = menu_on;
    }

    __asm("mov ecx, 5\n");
    __asm("call %0\n"::"a"(input_func));
    __asm("test al, al\n");
    __asm("je SW_5\n");
    speed++;
    __asm("SW_5:\n");

    __asm("mov eax, [%0]\n"::"a"(*g_rend_addr));
    __asm("cmp eax, 0\n");
    __asm("je call_menu\n");
    __asm("mov dword ptr [eax], 2\n");
    __asm("mov dword ptr [eax+4], 1\n");
    __asm("mov dword ptr [eax+8], 0\n");
    __asm("mov dword ptr [eax+0x34], 1\n");


//Practice Mode--
    __asm("call_menu:\n");
    __asm("push %0\n"::"a"(menu_1));
    __asm("push 0x150\n");
    __asm("push 0x2C0\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_BLUE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

//NO CONTEST
    if (use_sp_flg) {
        __asm("push %0\n"::"a"(menu_2));
        __asm("push 0x164\n");
        __asm("push 0x2C0\n");
        __asm("mov esi, %0\n"::"a"(*font_color + COLOR_YELLOW));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");
    }

//REGUL SPEED no-soflan
    __asm("push %0\n"::"D"(flg_2));
    __asm("push %0\n"::"a"(menu_3));
    __asm("push 0x178\n");
    __asm("push 0x2C0\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

//R-RANDOM
    __asm("push %0\n"::"D"(flg_3));
    __asm("push %0\n"::"a"(menu_4));
    __asm("push 0x18C\n");
    __asm("push 0x2C0\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

//SPEED 変更
    __asm("nop\n"::"a"(sp100));
    if (speed == 1) {
        __asm("nop\n"::"a"(sp80));
    } else if (speed == 2) {
        __asm("nop\n"::"a"(sp67));
    } else if (speed >= 3) {
        speed = 0;
    }
    __asm("push eax\n");
    __asm("push %0\n"::"a"(menu_5));
    __asm("push 0x1A0\n");
    __asm("push 0x2C0\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

/* quick menu on/off */
    if (disp) {

//quick retry
        __asm("push %0\n"::"a"(menu_7));
        __asm("push 0x1B4\n");
        __asm("push 0x2C0\n");
        __asm("mov esi, %0\n"::"a"(*font_color + COLOR_GREEN));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");

//quick retire & exit
        __asm("nop\n"::"a"(menu_8));
        if (ex_res_flg) {
            __asm("nop\n"::"a"(menu_6));
        }
        __asm("push eax\n");
        __asm("push 0x1C8\n");
        __asm("push 0x2C0\n");
        //    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_GREEN));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");
    }

    if (config.enhanced_polling_stats)
        enhanced_polling_stats_disp_sub();

    real_aging_loop();
}

static bool patch_practice_mode() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* AGING MODE to Practice Mode */
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xEC\x40\x53\x56\x57", 6, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve aging loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 6;
        if (!get_rendaddr()) {
            LOG("popnhax: Cannot find address for drawing\n");
            return false;
        }
        if (!get_addr_numkey()) {
            LOG("popnhax: Cannot find address for number pad\n");
            return false;
        }

        MH_CreateHook((LPVOID) patch_addr, (LPVOID) new_menu,
                      (void **) &real_aging_loop);

        uint32_t addr_pldata;
        if (!get_addr_pldata(&addr_pldata)) {
            LOG("popnhax: cannot retrieve pldata address for guidese\n");
            return false;
        }
    }

    {
        if (!get_speedaddr()) {
            LOG("popnhax: Cannot find address for speed change\n");
            return false;
        }
        MH_CreateHook((LPVOID) g_2dx_addr, (LPVOID) ex_2dx_speed,
                      (void **) &real_2dx_addr);
        MH_CreateHook((LPVOID) g_humen_addr, (LPVOID) ex_humen_speed,
                      (void **) &real_humen_addr);
        MH_CreateHook((LPVOID) g_soflan_addr, (LPVOID) regul_speed,
                      (void **) &real_get_bpm);
    }

    LOG("popnhax: speed hook enabled\n");

    {
        if (!get_addr_random()) {
            LOG("popnhax: Random LANE addr was not found\n");
            return false;
        }

        MH_CreateHook((LPVOID) g_ran_addr, (LPVOID) r_random,
                      (void **) &real_get_random);
    }
    {
        int64_t pattern_offset = search(data, dllSize,
                                        "\x5E\x8B\xE5\x5D\xC2\x04\x00\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x55\x8B\xEC\x83\xE4\xF8\x51\x56\x8B\xF1\x8B",
                                        32, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find address for restore addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 11;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) restore_plop,
                      (void **) &restore_op);
    }

    LOG("popnhax: R-Random hook enabled\n");
/*
    //add_stage に フラグオフ追加 (char *)"\x31\xC0\x40\xC3", 4
    {
        MH_CreateHook((LPVOID)(add_stage_addr+0x12), (LPVOID)clear_flg,
                      (void **)&add_stage_ext);
    }
*/
    return true;
}

void (*real_render_loop)();

void enhanced_polling_stats_disp() {
    __asm("mov eax, [%0]\n"::"a"(*g_rend_addr));
    __asm("cmp eax, 0\n");
    __asm("je call_stat_menu\n");
    __asm("mov dword ptr [eax], 2\n");
    __asm("mov dword ptr [eax+4], 1\n");
    __asm("mov dword ptr [eax+8], 0\n");
    __asm("mov dword ptr [eax+0x34], 1\n");

    __asm("call_stat_menu:\n");
    enhanced_polling_stats_disp_sub();
    real_render_loop();
}

/* enhanced_polling_stats */
const char stats_disp_header[] = " - 1000Hz Polling - ";
const char stats_disp_lastpoll[] = "last input: 0x%06x";
const char stats_disp_avgpoll[] = "100 polls in %dms";
const char stats_disp_offset_header[] = "- Latest correction -";
const char stats_disp_offset_top[] = "%s";
char stats_disp_offset_top_str[15] = "";
const char stats_disp_offset_bottom[] = "%s";
char stats_disp_offset_bottom_str[19] = "";

void enhanced_polling_stats_disp_sub() {
    __asm("push %0\n"::"a"(stats_disp_header));
    __asm("push 0x2A\n"); //Y coord
    __asm("push 0x160\n"); //X coord
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_LIGHT_GREY));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

    uint8_t color = COLOR_GREEN;
    if (g_poll_rate_avg > 100)
        color = COLOR_RED;
    __asm("push %0\n"::"D"(g_poll_rate_avg));
    __asm("push %0\n"::"a"(stats_disp_avgpoll));
    __asm("push 0x5C\n");
    __asm("push 0x160\n");
    __asm("mov esi, %0\n"::"a"(*font_color + color));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("push %0\n"::"D"(g_last_button_state));
    __asm("push %0\n"::"a"(stats_disp_lastpoll));
    __asm("push 0x48\n");
    __asm("push 0x160\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_LIGHT_PURPLE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("push %0\n"::"a"(stats_disp_offset_header));
    __asm("push 0x2A\n");
    __asm("push 0x200\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_LIGHT_GREY));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

    sprintf(stats_disp_offset_top_str, "%02u  %02u  %02u  %02u", g_offset_fix[1], g_offset_fix[3], g_offset_fix[5],
            g_offset_fix[7]);
    __asm("push %0\n"::"D"(stats_disp_offset_top_str));
    __asm("push %0\n"::"a"(stats_disp_offset_top));
    __asm("push 0x48\n");
    __asm("push 0x200\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_ORANGE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    sprintf(stats_disp_offset_bottom_str, "%02u  %02u  %02u  %02u  %02u", g_offset_fix[0], g_offset_fix[2],
            g_offset_fix[4], g_offset_fix[6], g_offset_fix[8]);
    __asm("push %0\n"::"D"(stats_disp_offset_bottom_str));
    __asm("push %0\n"::"a"(stats_disp_offset_top));
    __asm("push 0x5C\n");
    __asm("push 0x1FD\n");
    __asm("mov esi, %0\n"::"a"(*font_color + COLOR_ORANGE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");
}

static bool patch_enhanced_polling_stats() {
    if (config.practice_mode) {
        LOG("popnhax: enhanced polling stats displayed\n");
        return false;
    }
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xEC\x40\x53\x56\x57", 6, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: enhanced_polling_stats: cannot retrieve aging loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 6;
        if (!get_rendaddr()) {
            LOG("popnhax: enhanced_polling_stats: cannot find address for drawing\n");
            return false;
        }

        MH_CreateHook((LPVOID) patch_addr, (LPVOID) enhanced_polling_stats_disp,
                      (void **) &real_render_loop);

    }

    LOG("popnhax: enhanced polling stats displayed\n");
    return true;
}

/*
 * auto hi-speed
 */
bool g_longest_bpm_old_chart = false;
bool g_bypass_hispeed = false; //bypass target update for mystery bpm and soflan songs
bool g_mystery_bpm = false;
uint32_t g_hispeed = 0;
uint32_t g_hispeed_addr = 0;
uint32_t g_target_bpm = 0;
uint16_t *g_base_bpm_ptr = nullptr; //will point to g_low_bpm or g_hi_bpm according to mode
uint16_t g_low_bpm = 0;
uint16_t g_hi_bpm = 0;
uint16_t g_longest_bpm = 0;
unsigned char *g_chart_addr = nullptr;

typedef struct chart_chunk_s {
    uint32_t timestamp;
    uint16_t operation;
    uint16_t data;
    uint32_t duration;
} chart_chunk_t;
#define CHART_OP_BPM 0x0445
#define CHART_OP_END 0x0645

typedef struct bpm_list_s {
    uint16_t bpm;
    uint32_t duration;
    struct bpm_list_s *next;
} bpm_list_t;

static uint32_t add_duration(bpm_list_t *list, uint16_t bpm, uint32_t duration) {
    if (list->bpm == 0) {
        list->bpm = bpm;
        list->duration = duration;
        return duration;
    }

    while (list->next != nullptr) {
        if (list->bpm == bpm) {
            list->duration += duration;
            return list->duration;
        }
        list = list->next;
    }
    //new entry
    auto *entry = (bpm_list_t *) malloc(sizeof(bpm_list_t));
    entry->bpm = bpm;
    entry->duration = duration;
    entry->next = nullptr;
    list->next = entry;

    return duration;
}

static void destroy_list(bpm_list_t *list) {
    while (list != nullptr) {
        bpm_list_t *tmp = list;
        list = list->next;
        free(tmp);
    }
}

/* the goal is to set g_longest_bpm to the most used bpm from the chart present in memory at address g_chart_addr
 */
void compute_longest_bpm() {
    chart_chunk_t *chunk = nullptr;
    unsigned char *chart_ptr = g_chart_addr;
    int chunk_size = g_longest_bpm_old_chart ? 8 : 12;
    uint32_t prev_timestamp = 0x64; //game adds 0x64 to every timestamp of a chart upon loading
    uint16_t prev_bpm = 0;

    auto *list = (bpm_list_t *) malloc(sizeof(bpm_list_t));
    list->bpm = 0;
    list->duration = 0;
    list->next = nullptr;

    uint32_t max_duration = 0;
    uint16_t longest_bpm = 0;
    do {
        chunk = (chart_chunk_t *) chart_ptr;

        if (chunk->operation == CHART_OP_BPM || chunk->operation == CHART_OP_END) {
            if (prev_bpm) {
                uint32_t bpm_dur = add_duration(list, prev_bpm, chunk->timestamp -
                                                                prev_timestamp); //will add to existing entry or create a new one if not present
                if (bpm_dur > max_duration) {
                    max_duration = bpm_dur;
                    longest_bpm = prev_bpm;
                }
            }
            prev_bpm = chunk->data;
            prev_timestamp = chunk->timestamp;
        }
        chart_ptr += chunk_size;
    } while (chunk->operation != CHART_OP_END);

    destroy_list(list);
    g_longest_bpm = longest_bpm;
}

void (*real_set_hispeed)();

void hook_set_hispeed() {
    g_bypass_hispeed = false;
    __asm("mov %0, eax\n":"=r"(g_hispeed_addr): :);
    real_set_hispeed();
}

void (*real_retrieve_chart_addr)();

void hook_retrieve_chart_addr() {
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov %0, esi\n":"=a"(g_chart_addr): :);
    __asm("call %0\n"::"r"(compute_longest_bpm));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    real_retrieve_chart_addr();
}

void (*real_retrieve_chart_addr_old)();

void hook_retrieve_chart_addr_old() {
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov %0, edi\n":"=a"(g_chart_addr): :);
    __asm("call %0\n"::"r"(compute_longest_bpm));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    real_retrieve_chart_addr_old();
}

/*
 * This is called a lot of times: when arriving on option select, and when changing/navigating *any* option
 * I'm hooking here to set hi-speed to the target BPM
 */
double g_hispeed_double = 0;

void (*real_read_hispeed)();

void hook_read_hispeed() {
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");

    __asm("mov %0, word ptr [ebp+0xA1A]\n":"=a"(g_low_bpm): :);
    __asm("mov %0, word ptr [ebp+0xA1C]\n":"=a"(g_hi_bpm): :);
    __asm("mov %0, byte ptr [ebp+0xA1E]\n":"=a"(g_mystery_bpm): :);

    if (g_bypass_hispeed || g_target_bpm ==
                            0) //bypass for mystery BPM and soflan songs (to avoid hi-speed being locked since target won't change)
    {
        __asm("jmp leave_read_hispeed\n");
    }

    g_hispeed_double = (double) g_target_bpm / (double) (*g_base_bpm_ptr / 10.0);
    g_hispeed = (uint32_t) (g_hispeed_double + 0.5); //rounding to nearest
    if (g_hispeed > 0x64) g_hispeed = 0x64;
    if (g_hispeed < 0x0A) g_hispeed = 0x0A;

    __asm("and edi, 0xFFFF0000\n");                   //keep existing popkun and hidden status values
    __asm("or edi, dword ptr[%0]\n"::"m"(g_hispeed)); //fix hispeed initial display on option screen
    __asm("mov eax, dword ptr[%0]\n"::"m"(g_hispeed_addr));
    __asm("mov dword ptr[eax], edi\n");

    __asm("leave_read_hispeed:\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    real_read_hispeed();
}

void (*real_increase_hispeed)();

void hook_increase_hispeed() {
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("push ecx\n");

    if (g_mystery_bpm || g_low_bpm != g_hi_bpm) {
        g_bypass_hispeed = true;
        __asm("jmp leave_increase_hispeed\n");
    }

    //increase hispeed
    __asm("movzx ecx, word ptr[edi]\n");
    __asm("inc ecx\n");
    __asm("cmp ecx, 0x65\n");
    __asm("jb skip_hispeed_rollover_high\n");
    __asm("mov ecx, 0x0A\n");
    __asm("skip_hispeed_rollover_high:\n");

    //compute resulting bpm using exact same formula as game (base bpm in eax, multiplier in ecx)
    __asm("movsx eax, %0\n"::"m"(g_hi_bpm));
    __asm("cwde\n");
    __asm("movsx ecx,cx\n");
    __asm("imul ecx,eax\n");
    __asm("mov eax, 0x66666667\n");
    __asm("imul ecx\n");
    __asm("sar edx,2\n");
    __asm("mov eax,edx\n");
    __asm("shr eax,0x1F\n");
    __asm("add eax,edx\n");

    __asm("mov %0, eax\n":"=m"(g_target_bpm): :);

    __asm("leave_increase_hispeed:\n");
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("pop eax\n");
    real_increase_hispeed();
}

void (*real_decrease_hispeed)();

void hook_decrease_hispeed() {
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("push ecx\n");
    if (g_mystery_bpm || g_low_bpm != g_hi_bpm) {
        g_bypass_hispeed = true;
        __asm("jmp leave_decrease_hispeed\n");
    }

    //decrease hispeed
    __asm("movzx ecx, word ptr[edi]\n");
    __asm("dec ecx\n");
    __asm("cmp ecx, 0x0A\n");
    __asm("jge skip_hispeed_rollover_low\n");
    __asm("mov ecx, 0x64\n");
    __asm("skip_hispeed_rollover_low:\n");

    //compute resulting bpm using exact same formula as game (base bpm in eax, multiplier in ecx)
    __asm("movsx eax, %0\n"::"m"(g_hi_bpm));
    __asm("cwde\n");
    __asm("movsx ecx,cx\n");
    __asm("imul ecx,eax\n");
    __asm("mov eax, 0x66666667\n");
    __asm("imul ecx\n");
    __asm("sar edx,2\n");
    __asm("mov eax,edx\n");
    __asm("shr eax,0x1F\n");
    __asm("add eax,edx\n");

    __asm("mov %0, eax\n":"=m"(g_target_bpm): :);

    __asm("leave_decrease_hispeed:\n");
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("pop eax\n");
    real_decrease_hispeed();
}

bool patch_hispeed_auto(uint8_t mode, uint16_t default_bpm) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    g_base_bpm_ptr = &g_hi_bpm;
    if (mode == 2)
        g_base_bpm_ptr = &g_low_bpm;
    else if (mode == 3)
        g_base_bpm_ptr = &g_longest_bpm;

    g_target_bpm = default_bpm;
    /* retrieve hi-speed address */
    {
        int64_t pattern_offset = search(data, dllSize, "\x66\x89\x0C\x07\x0F\xB6\x45\x04", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x04;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_set_hispeed,
                      (void **) &real_set_hispeed);
    }
    /* write new hispeed according to target bpm */
    {
        int64_t pattern_offset = search(data, dllSize, "\x98\x50\x66\x8B\x85\x1A\x0A\x00\x00\x8B\xCF", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed apply address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset - 0x07;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_read_hispeed,
                      (void **) &real_read_hispeed);
    }
    /* update target bpm on hispeed increase */
    {
        int64_t pattern_offset = search(data, dllSize, "\x66\xFF\x07\x0F\xB7\x07\x66\x83\xF8\x64", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed increase\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_increase_hispeed,
                      (void **) &real_increase_hispeed);
    }
    /* update target bpm on hispeed decrease */
    {
        int64_t pattern_offset = search(data, dllSize, "\x66\xFF\x0F\x0F\xB7\x07\x66\x83\xF8\x0A", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed decrease\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_decrease_hispeed,
                      (void **) &real_decrease_hispeed);
    }

    /* compute longest bpm for mode 3 */
    if (mode == 3) {
        int64_t pattern_offset = search(data, dllSize, "\x00\x00\x72\x05\xB9\xFF", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find chart address\n");
            return false;
        }

        /* detect if usaneko+ */
        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x0F;
        uint8_t check_byte = *((uint8_t *) (patch_addr + 1));

        if (check_byte == 0x04) {
            patch_addr += 9;
            g_longest_bpm_old_chart = true;
            LOG("popnhax: auto hi-speed: old game version\n");
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_retrieve_chart_addr_old,
                          (void **) &real_retrieve_chart_addr_old);
        } else {
            patch_addr += 11;
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_retrieve_chart_addr,
                          (void **) &real_retrieve_chart_addr);
        }
    }

    LOG("popnhax: auto hi-speed enabled");
    if (g_target_bpm != 0)
        LOG(" with default %hubpm", g_target_bpm);
    if (mode == 2)
        LOG(" (lower bpm target)");
    else if (mode == 3)
        LOG(" (longest bpm target)");
    else
        LOG(" (higher bpm target)");

    LOG("\n");

    return true;
}

/* HARD GAUGE SURVIVAL*/
uint8_t g_hard_gauge_selected = false;

void (*real_survival_flag_hard_gauge)();

void hook_survival_flag_hard_gauge() {
    __asm("cmp bl, 0\n");
    __asm("jne no_hard_gauge\n");
    g_hard_gauge_selected = false;
    __asm("cmp cl, 2\n");
    __asm("jne no_hard_gauge\n");
    g_hard_gauge_selected = true;
    __asm("no_hard_gauge:\n");
    real_survival_flag_hard_gauge();
}

void (*real_survival_flag_hard_gauge_old)();

void hook_survival_flag_hard_gauge_old() {
    __asm("cmp bl, 0\n");
    __asm("jne no_hard_gauge_old\n");
    g_hard_gauge_selected = false;
    __asm("cmp dl, 2\n"); //dl is used instead of cl in older games
    __asm("jne no_hard_gauge_old\n");
    g_hard_gauge_selected = true;
    __asm("no_hard_gauge_old:\n");
    real_survival_flag_hard_gauge_old();
}

void (*real_check_survival_gauge)();

void hook_check_survival_gauge() {
    if (g_hard_gauge_selected) {
        __asm("mov al, 1\n");
        __asm("ret\n");
    }
    real_check_survival_gauge();
}

void (*real_survival_gauge_medal_clear)();

void (*real_survival_gauge_medal)();

void hook_survival_gauge_medal() {
    if (g_hard_gauge_selected) {
        __asm("cmp eax, 0\n"); //empty gauge should still fail
        __asm("jz skip_force_clear\n");

        /* fix gauge ( [0;1023] -> [725;1023] ) */
        __asm("push eax");
        __asm("push ebx");
        __asm("push edx");
        __asm("xor edx,edx");
        __asm("mov eax, dword ptr [edi+4]");

        /* bigger interval for first bar */
        __asm("cmp eax, 42");
        __asm("jge skip_lower");
        __asm("mov eax, 42");
        __asm("skip_lower:");

        /* tweak off by one/two values */
        __asm("cmp eax, 297");
        __asm("je decrease_once");
        __asm("cmp eax, 298");
        __asm("je decrease_twice");
        __asm("cmp eax, 426");
        __asm("je decrease_once");
        __asm("cmp eax, 681");
        __asm("je decrease_once");
        __asm("cmp eax, 936");
        __asm("je decrease_once");
        __asm("cmp eax, 937");
        __asm("je decrease_twice");

        __asm("jmp no_decrease");

        __asm("decrease_twice:");
        __asm("dec eax");
        __asm("decrease_once:");
        __asm("dec eax");

        __asm("no_decrease:");

        /* perform ((gauge+99)/3) + 678 */
        __asm("add eax, 99");
        __asm("mov bx, 3");
        __asm("idiv bx");
        __asm("add eax, 678");

        /* higher cap value */
        __asm("cmp eax, 1023");
        __asm("jle skip_trim");
        __asm("mov eax, 1023");
        __asm("skip_trim:");

        __asm("mov dword ptr [edi+4], eax");
        __asm("pop edx");
        __asm("pop ebx");
        __asm("pop eax");

        __asm("jmp %0\n"::"m"(real_survival_gauge_medal_clear));
    }
    __asm("skip_force_clear:\n");
    real_survival_gauge_medal();
}

void (*real_get_retire_timer)();

void hook_get_retire_timer() {
    if (g_hard_gauge_selected) {
        __asm("mov eax, 0xFFFF\n");
        __asm("ret\n");
    }
    real_get_retire_timer();
}

bool patch_hard_gauge_survival(uint8_t severity) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* refill gauge at each stage */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x84\xC0\x75\x0B\xB8\x00\x04\x00\x00", 9, 0, "\x90\x90\x90\x90", 4)) {
            LOG("popnhax: survival gauge: cannot patch gauge refill\n");
        }
    }

    /* change is_survival_gauge function behavior */
    {
        int64_t pattern_offset = search(data, dllSize, "\x33\xC9\x83\xF8\x04\x0F\x94\xC1\x8A\xC1", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find survival gauge check function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x02;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_check_survival_gauge,
                      (void **) &real_check_survival_gauge);
    }

    /* change get_retire_timer function behavior (fix bug with song not exiting on empty gauge when paseli is on) */
    {
        int64_t pattern_offset = search(data, dllSize, "\x3D\xB0\x04\x00\x00\x7C", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find get retire timer function\n");
            return false;
        }

        int64_t fun_rel = *(int32_t *) (data + pattern_offset - 0x04); // function call is just before our pattern
        uint64_t patch_addr = (int64_t) data + pattern_offset + fun_rel;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_get_retire_timer,
                      (void **) &real_get_retire_timer);
    }

    /* hook commit option to flag hard gauge being selected */
    {
        /* find option commit function (unilab) */
        int64_t pattern_offset = search(data, dllSize,
                                        "\x89\x48\x0C\x8B\x56\x10\x89\x50\x10\x66\x8B\x4E\x14\x66\x89\x48\x14\x5B\xC3\xCC",
                                        20, 0);
        if (pattern_offset == -1) {
            /* wasn't found, look for older function */
            int64_t first_loc = search(data, dllSize, "\x0F\xB6\xC3\x03\xCF\x8D", 6, 0);

            if (first_loc == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (1)\n");
                return false;
            }

            pattern_offset = search(data, 0x50, "\x89\x50\x0C", 3, first_loc);

            if (pattern_offset == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (2)\n");
                return false;
            }

            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_survival_flag_hard_gauge_old,
                          (void **) &real_survival_flag_hard_gauge_old);
        } else {
            uint64_t patch_addr = (int64_t) data + pattern_offset;
            MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_survival_flag_hard_gauge,
                          (void **) &real_survival_flag_hard_gauge);
        }

    }

    /* Fix medal calculation */
    {
        int64_t addr = search(data, dllSize, "\x0F\xB7\x47\x12\x66\x83\xF8\x14", 8, 0);
        if (addr == -1) {
            LOG("popnhax: survival gauge: cannot find medal computation\n");
            return false;
        }

        uint64_t function_addr = (int64_t) data + addr;
        real_survival_gauge_medal_clear = (void (*)()) function_addr;

        int64_t pattern_offset = search(data, dllSize, "\x0F\x9F\xC1\x5E\x8B\xD0\x3B\xC1\x7F\x02", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find medal computation hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x04;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_survival_gauge_medal,
                      (void **) &real_survival_gauge_medal);
    }

    /* gauge severity */
    const char *severity_str[5] = {"", "COURSE (-35)", "NORMAL (-51)", "IIDX (-92)", "HARD (-204)"};
    uint32_t severity_val[5] = {0, 0xFFFFFFDD, 0xFFFFFFCD, 0xFFFFFFA4, 0xFFFFFF34};

    if (severity != 1) {
        if (severity > 4)
            severity = 4;
        uint32_t decrease_amount = severity_val[severity];
        char *as_hex = (char *) &decrease_amount;

        if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xDD\xFF\xFF\xFF", 5, 1, as_hex, 4)) {
            LOG("\n");
            LOG("popnhax: survival gauge: cannot patch severity\n");
        }
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: enabled (decrease rate : %s)\n", severity_str[severity]);

    return true;
}

void (*real_survival_iidx_prepare_gauge)();

void hook_survival_iidx_prepare_gauge() {
    //this code is specific to survival gauge, so no additional check is required
    __asm("cmp esi, 2\n");
    __asm("jne skip_iidx_prepare_gauge\n");
    __asm("shr eax, 1\n");

    __asm("skip_iidx_prepare_gauge:\n");
    real_survival_iidx_prepare_gauge();
}

void (*real_survival_iidx_apply_gauge)();

void hook_survival_iidx_apply_gauge() {
    __asm("cmp byte ptr %0, 0\n"::"m"(g_hard_gauge_selected));
    __asm("je skip_iidx_apply_gauge\n"); //skip if not in survival gauge mode
    __asm("cmp ecx, 2\n");
    __asm("jb skip_iidx_apply_gauge\n"); //skip if gauge is not decreasing
    __asm("mov ecx, 3\n");
    __asm("cmp ax, 308\n");
    __asm("jge skip_iidx_apply_gauge\n"); //skip if gauge is above 30%
    __asm("mov ecx, 2\n");

    __asm("skip_iidx_apply_gauge:\n");
    real_survival_iidx_apply_gauge();
}

bool patch_survival_iidx() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* put half the decrease value in the first slot */
    {
        int64_t pattern_offset = search(data, dllSize, "\xE9\x8C\x00\x00\x00\x8B\xC6", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: iidx survival gauge: cannot find survival gauge prepare function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_survival_iidx_prepare_gauge,
                      (void **) &real_survival_iidx_prepare_gauge);
    }

    /* switch slot depending on gauge value (get halved value when 30% or less) */
    {
        int64_t pattern_offset = search(data, dllSize, "\x66\x83\xF8\x01\x75\x5E\x66\xA1", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: iidx survival gauge: cannot find survival gauge update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x0C;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_survival_iidx_apply_gauge,
                      (void **) &real_survival_iidx_apply_gauge);
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: IIDX-like <=30%% adjustment\n");

    return true;
}

bool patch_survival_spicy() {
    if (!find_and_patch_hex(g_game_dll_fn, "\xB9\x02\x00\x00\x00\x66\x89\x0C\x75", 9, 1, "\x00\x00\x00\x00", 4)) {
        LOG("\n");
        LOG("popnhax: spicy survival gauge: cannot patch gauge increment\n");
        return false;
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: spicy gauge\n");
    return true;
}

void (*skip_convergence_value_get_score)();

void (*real_convergence_value_compute)();

void hook_convergence_value_compute() {
    __asm("push eax\n");
    __asm("mov eax, dword ptr [eax]\n"); // music id (edi-0x38 or edi-0x34 depending on game)
    __asm("cmp ax, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jae force_convergence_value\n");
    __asm("pop eax\n");
    __asm("jmp %0\n"::"m"(real_convergence_value_compute));
    __asm("force_convergence_value:\n");
    __asm("pop eax\n");
    __asm("xor eax, eax\n");
    __asm("jmp %0\n"::"m"(skip_convergence_value_get_score));
}

void (*skip_pp_list_elem)();

void (*real_pp_increment_compute)();

void hook_pp_increment_compute() {
    __asm("cmp ecx, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jb process_pp_elem\n");
    __asm("jmp %0\n"::"m"(skip_pp_list_elem));
    __asm("process_pp_elem:\n");
    __asm("jmp %0\n"::"m"(real_pp_increment_compute));
}

static bool patch_db_fix_cursor() {
    /* bypass song id sanitizer */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB7\x06\x66\x85\xC0\x7C\x1C", 8, -5, "\x90\x90\x90\x90\x90", 5)) {
            LOG("popnhax: patch_db: cannot fix cursor\n");
            return false;
        }
    }
    /* skip 2nd check */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB7\x06\x66\x85\xC0\x7C\x1C", 8, 0x1A, "\xEB", 1)) {
            LOG("popnhax: patch_db: cannot fix cursor (2)\n");
            return false;
        }
    }
    return true;
}

bool patch_db_power_points() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t child_fun_loc = 0;
    {
        int64_t offset = search(data, dllSize, "\x8D\x46\xFF\x83\xF8\x0A\x0F", 7, 0);
        if (offset == -1) {
#if DEBUG == 1
            LOG("popnhax: patch_db: failed to retrieve struct size and offset\n");
#endif
            return false;
        }
        uint32_t child_fun_rel = *(uint32_t *) ((int64_t) data + offset - 0x04);
        child_fun_loc = offset + child_fun_rel;
    }

    {
        int64_t pattern_offset = search(data, 0x40, "\x8d\x74\x01", 3, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: failed to retrieve offset from base\n");
            g_pfree_song_offset = 0x54; // best effort
            return false;
        }

        g_pfree_song_offset = *(uint8_t *) ((int64_t) data + pattern_offset + 0x03);
    }

    /* Adapt convergence value computation (skip cs_omni and customs) */
    {
        int64_t pattern_offset = search(data, dllSize, "\x84\xC0\x75\x11\x8D\x44\x24\x38", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find convergence value computation\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x08;

        skip_convergence_value_get_score = (void (*)()) (patch_addr + 0x05);
        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_convergence_value_compute,
                      (void **) &real_convergence_value_compute);
    }

    /* skip cs_omni and customs in new stages pplist */
    {
        int64_t pattern_offset = search(data, dllSize, "\x8A\x1E\x6A\x00\x51\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find pp increment computation\n");
            return false;
        }

        uint64_t patch_addr = (int64_t) data + pattern_offset + 0x02;

        MH_CreateHook((LPVOID) (patch_addr), (LPVOID) hook_pp_increment_compute,
                      (void **) &real_pp_increment_compute);

        int64_t jump_addr_offset = search(data, dllSize, "\x8B\x54\x24\x5C\x0F\xB6\x42\x0E\x45", 9, 0);
        if (jump_addr_offset == -1) {
            LOG("popnhax: patch_db: cannot find pp increment computation next iter\n");
            return false;
        }
        skip_pp_list_elem = (void (*)()) ((int64_t) data + jump_addr_offset);

    }

    return true;
}

static bool option_full() {
    /* patch default values in memory init function */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x88\x48\x1A\x88\x48\x1B\x88\x48\x1C", 9, 0,
                                "\xC7\x40\x1A\x00\x00\x01\x00\x90\x90", 9)) {
            LOG("popnhax: cannot set full options by default\n");
            return false;
        }
    }

    LOG("popnhax: always display full options\n");
    return true;
}

static bool option_guide_se_off() {
    /* set guide SE OFF by default in all modes */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xC6\x40\x24\x01\x88\x48\x25", 7, 3, "\x00", 1)   /* unilab */
            && !find_and_patch_hex(g_game_dll_fn, "\x89\x48\x20\x88\x48\x24\xC3\xCC", 8, 3, "\xC6\x40\x24\x01\xC3",
                                   5)) /* usaneko-kaimei */
        {
            LOG("popnhax: guidese_off: cannot set guide SE off by default\n");
            return false;
        }
    }
    LOG("popnhax: guidese_off: Guide SE OFF by default\n");
    return true;
}

static bool option_net_ojama_off() {
    /* set netvs ojama OFF by default */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xC6\x40\xFD\x00\xC6\x00\x01", 7, 6, "\x00", 1)) {
            LOG("popnhax: netvs_off: cannot set net ojama off by default\n");
            return false;
        }
    }
    LOG("popnhax: netvs_off: net ojama OFF by default\n");
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            g_log_fp = fopen("popnhax.log", "w");
            if (g_log_fp == nullptr) {
                LOG("cannot open popnhax.log for write, output to stderr only\n");
            }
            LOG("popnhax: Initializing\n");
            if (MH_Initialize() != MH_OK) {
                LOG("Failed to initialize minhook\n");
                exit(1);
                return TRUE;
            }

            bool force_trans_debug = false;
            bool force_no_omni = false;

            LPWSTR *szArglist;
            int nArgs;

            szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
            if (szArglist == nullptr) {
                LOG("popnhax: Failed to get cmdline\n");
                return 0;
            }

            for (int i = 0; i < nArgs; i++) {
                /* game dll */
                if (wcsstr(szArglist[i], L".dll") != nullptr && wcsstr(szArglist[i], L"popn2") == szArglist[i]) {
                    char *resultStr = new char[wcslen(szArglist[i]) + 1];
                    wsprintfA(resultStr, "%S", szArglist[i]);
                    g_game_dll_fn = strdup(resultStr);
                }
                    /* config xml */
                else if (wcscmp(szArglist[i], L"--popnhax-config") == 0) {
                    char *resultStr = new char[wcslen(szArglist[i + 1]) + 1];
                    wsprintfA(resultStr, "%S", szArglist[i + 1]);
                    g_config_fn = strdup(resultStr);
                } else if (wcscmp(szArglist[i], L"--translation-debug") == 0) {
                    LOG("--translation-debug: turning on translation-related dumps\n");
                    force_trans_debug = true;
                } else if (wcscmp(szArglist[i], L"--no-omni") == 0) {
                    LOG("--no-omni: force disable patch_db\n");
                    force_no_omni = true;
                }
            }
            LocalFree(szArglist);

            if (g_game_dll_fn == nullptr)
                g_game_dll_fn = strdup("popn22.dll");

            if (g_config_fn == nullptr) {
                /* if there's an xml named like the custom game dll, it takes priority */
                char *tmp_name = strdup(g_game_dll_fn);
                strcpy(tmp_name + strlen(tmp_name) - 3, "xml");
                if (access(tmp_name, F_OK) == 0)
                    g_config_fn = strdup(tmp_name);
                else
                    g_config_fn = strdup("popnhax.xml");
                free(tmp_name);
            }

            LOG("popnhax: game dll: %s\n", g_game_dll_fn);
            LOG("popnhax: config file: %s\n", g_config_fn);

            if (!_load_config(g_config_fn, &config, config_psmap)) {
                LOG("FATAL ERROR: Could not parse %s\n", g_config_fn);
                return FALSE;
            }

            if (force_trans_debug)
                config.translation_debug = true;

            if (force_no_omni)
                config.patch_db = false;

            if (!config.disable_multiboot) {
                /* automatically force datecode based on dll name when applicable (e.g. popn22_2022061300.dll and no force_datecode) */
                if ((strlen(g_game_dll_fn) == 21)
                    && (config.force_datecode[0] == '\0')) {
                    LOG("popnhax: multiboot autotune activated (custom game dll, force_datecode off)\n");
                    memcpy(config.force_datecode, g_game_dll_fn + 7, 10);
                    LOG("popnhax: multiboot: auto set datecode to %s\n", config.force_datecode);
                    if (config.score_challenge && (strcmp(config.force_datecode, "2020092800") <= 0)) {
                        LOG("popnhax: multiboot: auto disable score challenge patch (already ingame)\n");
                        config.score_challenge = false;
                    }
                    if (config.patch_db && (strcmp(config.force_datecode, "2016121400") < 0)) {
                        LOG("popnhax: multiboot: auto disable omnimix patch (not compatible)\n");
                        config.patch_db = false;
                    }
                    if (config.guidese_off && (strcmp(config.force_datecode, "2016121400") < 0)) {
                        LOG("popnhax: multiboot: auto disable Guide SE patch (not compatible)\n");
                        config.guidese_off = false;
                    }
                }
            }

            if (config.force_datecode[0] != '\0') {
                if (strlen(config.force_datecode) != 10)
                    LOG("popnhax: force_datecode: Invalid datecode %s, should be 10 digits (e.g. 2022061300)\n",
                        config.force_datecode);
                else
                    patch_datecode(config.force_datecode);
            }

            /* look for possible translation patch folder ("_yyyymmddrr_tr" for multiboot, or simply "_translation") */
            if (!config.disable_translation) {
                char translation_folder[16] = "";
                char translation_path[64] = "";

                /* parse */
                if (config.force_datecode[0] != '\0') {

                    sprintf(translation_folder, "_%s%s", config.force_datecode, "_tr");
                    sprintf(translation_path, "%s%s", "data_mods\\", translation_folder);
                    if (access(translation_path, F_OK) != 0) {
                        translation_folder[0] = '\0';
                    }
                }

                if (translation_folder[0] == '\0') {
                    sprintf(translation_folder, "%s", "_translation");
                    sprintf(translation_path, "%s%s", "data_mods\\", translation_folder);
                    if (access(translation_path, F_OK) != 0) {
                        translation_folder[0] = '\0';
                    }
                }

                if (translation_folder[0] != '\0') {
                    LOG("popnhax: translation: using folder \"%s\"\n", translation_folder);
                    patch_translate(g_game_dll_fn, translation_folder, config.translation_debug);
                } else if (config.translation_debug) {
                    DWORD dllSize = 0;
                    char *data = getDllData(g_game_dll_fn, &dllSize);
                    LOG("popnhax: translation debug: no translation applied, dump prepatched dll\n");
                    FILE *dllrtp = fopen("dllruntime_prepatched.dll", "wb");
                    fwrite(data, 1, dllSize, dllrtp);
                    fclose(dllrtp);
                }
            }

            if (config.practice_mode) {
                patch_practice_mode();
            }

            if (config.audio_source_fix) {
                patch_audio_source_fix();
            }

            if (config.unset_volume) {
                patch_unset_volume();
            }

            if (config.pfree) {
                g_pfree_mode = true; /* used by asm hook */
                patch_pfree();
            }

            if (config.quick_retire) {
                patch_quick_retire(config.pfree);
            }

            if (config.score_challenge) {
                patch_score_challenge();
            }

            if (config.force_hd_timing) {
                patch_hd_timing();
            }

            if (config.force_hd_resolution) {
                patch_hd_resolution(config.force_hd_resolution);
            }

            if (config.iidx_hard_gauge) {
                if (config.survival_gauge || config.survival_spicy || config.survival_iidx) {
                    LOG("popnhax: iidx_hard_gauge cannot be used when other survival options are already set\n");
                    config.iidx_hard_gauge = false;
                } else {
                    config.survival_gauge = 3;
                    config.survival_spicy = true;
                    config.survival_iidx = true;
                }
            }

            if (config.survival_gauge) {
                bool res = true;
                res &= patch_hard_gauge_survival(config.survival_gauge);

                if (config.survival_spicy) {
                    res &= patch_survival_spicy();
                }

                if (config.survival_iidx) {
                    res &= patch_survival_iidx();
                }

                if (config.iidx_hard_gauge && res)
                    LOG("popnhax: iidx_hard_gauge: HARD gauge is now IIDX-like\n");
            }

            if (config.hidden_is_offset) {
                patch_hidden_is_offset();
                if (config.show_offset) {
                    patch_show_hidden_adjust_result_screen();
                }
            }

            if (config.show_fast_slow) {
                force_show_fast_slow();
            }

            if (config.show_details) {
                force_show_details_result();
            }

            if (config.audio_offset) {
                if (config.keysound_offset) {
                    LOG("popnhax: audio_offset cannot be used when keysound_offset is already set\n");
                } else {
                    config.disable_keysounds = true;
                    config.keysound_offset = -1 * config.audio_offset;
                    LOG("popnhax: audio_offset: disable keysounds then offset timing by %d ms\n",
                        config.keysound_offset);
                }
            }

            if (config.disable_keysounds) {
                patch_disable_keysound();
            }

            if (config.keysound_offset) {
                /* must be called _after_ force_hd_timing */
                patch_keysound_offset(config.keysound_offset);
            }

            if (config.beam_brightness) {
                /* must be called _after_ force_hd_resolution */
                patch_add_to_beam_brightness(config.beam_brightness);
            }

            if (config.event_mode) {
                patch_event_mode();
            }

            if (config.remove_timer) {
                patch_remove_timer();
            }

            if (config.freeze_timer) {
                patch_freeze_timer();
            }

            if (config.skip_tutorials) {
                patch_skip_tutorials();
            }

            if (config.patch_db) {
                LOG("popnhax: patching songdb\n");
                /* must be called after force_datecode */
                patch_db_power_points();
                patch_db_fix_cursor();
                patch_database(config.force_unlocks);
            }

            if (config.force_unlocks) {
                if (!config.patch_db) {
                    /* Only unlock using these methods if it's not done directly through the database hooks */
                    force_unlock_songs();
                    force_unlock_charas();
                }
                patch_unlocks_offline();
                force_unlock_deco_parts();
            }

            if (config.force_full_opt)
                option_full();

            if (config.netvs_off)
                option_net_ojama_off();

            if (config.guidese_off)
                option_guide_se_off();

            if (config.fps_uncap)
                patch_fps_uncap();

            if (config.enhanced_polling) {
                patch_enhanced_polling(config.debounce, config.enhanced_polling_stats);
                if (config.enhanced_polling_stats) {
                    patch_enhanced_polling_stats();
                }
            }

            if (config.hispeed_auto) {
                patch_hispeed_auto(config.hispeed_auto, config.hispeed_default_bpm);
            }

#if DEBUG == 1
            patch_get_time();
#endif

            MH_EnableHook(MH_ALL_HOOKS);

            LOG("popnhax: done patching game, enjoy!\n");

            if (g_log_fp != stderr)
                fclose(g_log_fp);

            break;
        }

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}
