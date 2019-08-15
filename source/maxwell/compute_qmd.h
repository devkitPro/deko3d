#pragma once
#include <stdint.h>

namespace maxwell
{

// Compute Queue Metadata, version 01.07
// https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/compute/clb1c0qmd.h#L236
struct ComputeQmd
{
	// 0x00
	uint32_t outer_put      : 31;
	uint32_t outer_overflow : 1;

	// 0x04
	uint32_t outer_get             : 31;
	uint32_t outer_sticky_overflow : 1;

	// 0x08
	uint32_t inner_get      : 31;
	uint32_t inner_overflow : 1;

	// 0x0C
	uint32_t inner_put             : 31;
	uint32_t inner_sticky_overflow : 1;

	// 0x10
	uint32_t qmd_reserved_a_a;

	// 0x14
	uint32_t dependent_qmd_pointer;

	// 0x18
	uint32_t qmd_group_id                         : 6;
	uint32_t sm_global_caching_enable             : 1;
	uint32_t run_cta_in_one_sm_partition          : 1;
	uint32_t is_queue                             : 1;
	uint32_t add_to_head_of_qmd_group_linked_list : 1;
	uint32_t semaphore_release_enable0            : 1;
	uint32_t semaphore_release_enable1            : 1;
	uint32_t require_scheduling_pcas              : 1;
	uint32_t dependent_qmd_schedule_enable        : 1;
	uint32_t dependent_qmd_type                   : 1; // 0=queue 1=grid
	uint32_t dependent_qmd_field_copy             : 1;
	uint32_t qmd_reserved_b                       : 16;

	// 0x1C
	uint32_t circular_queue_size              : 25;
	uint32_t qmd_reserved_c                   : 1;
	uint32_t invalidate_texture_header_cache  : 1;
	uint32_t invalidate_texture_sampler_cache : 1;
	uint32_t invalidate_texture_data_cache    : 1;
	uint32_t invalidate_shader_data_cache     : 1;
	uint32_t invalidate_instruction_cache     : 1;
	uint32_t invalidate_shader_constant_cache : 1;

	// 0x20
	uint32_t program_offset;

	// 0x24
	uint32_t circular_queue_addr_lower;

	// 0x28
	uint32_t circular_queue_addr_upper : 8;
	uint32_t qmd_reserved_d            : 8;
	uint32_t circular_queue_entry_size : 16;

	// 0x2C
	uint32_t cwd_reference_count_id              : 6;
	uint32_t cwd_reference_count_delta_minus_one : 8;
	uint32_t release_membar_type                 : 1; // 0=fe_none 1=fe_sysmembar
	uint32_t cwd_reference_count_incr_enable     : 1;
	uint32_t cwd_membar_type                     : 2; // 0=l1_none 1=l1_sysmembar 3=l1_membar
	uint32_t sequentially_run_ctas               : 1;
	uint32_t cwd_reference_count_decr_enable     : 1;
	uint32_t throttled                           : 1;
	uint32_t _missing_a                          : 3;
	uint32_t fp32_nan_behavior                   : 1; // 0=legacy 1=fp64_compatible
	uint32_t fp32_f2i_nan_behavior               : 1; // 0=pass_zero 1=pass_indefinite
	uint32_t api_visible_call_limit              : 1; // 0=32 1=no_limit
	uint32_t shared_memory_bank_mapping          : 1; // 0=four_bytes_per_bank 1=eight_bytes_per_bank
	uint32_t _missing_b                          : 2;
	uint32_t sampler_index                       : 1; // 0=independently 1=via_header_index
	uint32_t fp32_narrow_instruction             : 1; // 0=keep_denorms 1=flush_denorms

	// 0x30
	uint32_t cta_raster_width;

	// 0x34
	uint32_t cta_raster_height : 16;
	uint32_t cta_raster_depth  : 16;

	// 0x38
	uint32_t cta_raster_width_resume;

	// 0x3C
	uint32_t cta_raster_height_resume : 16;
	uint32_t cta_raster_depth_resume  : 16;

	// 0x40
	uint32_t queue_entries_per_cta_minus_one : 7;
	uint32_t _missing_c                      : 3;
	uint32_t coalesce_waiting_period         : 8;
	uint32_t _missing_d                      : 14;

	// 0x44
	uint32_t shared_memory_size : 18;
	uint32_t qmd_reserved_g     : 14;

	// 0x48
	uint32_t qmd_version           : 4;
	uint32_t qmd_major_version     : 4;
	uint32_t qmd_reserved_h        : 8;
	uint32_t cta_thread_dimension0 : 16;

	// 0x4C
	uint32_t cta_thread_dimension1 : 16;
	uint32_t cta_thread_dimension2 : 16;

	// 0x50
	uint32_t constant_buffer_valid : 8; // bitmask
	uint32_t qmd_reserved_i        : 21;
	uint32_t l1_configuration      : 3; // 1=directly_adressable_memory_size_16KB 2='32KB 3='48KB

	// 0x54
	uint32_t sm_disable_mask_lower;

	// 0x58
	uint32_t sm_disable_mask_upper;

	// 0x5C
	struct
	{
		// 0x5C; 0x68
		uint32_t address_lower;

		// 0x60; 0x6C
		uint32_t address_upper    : 8;
		uint32_t qmd_reserved_j_l : 8;
		uint32_t _missing_e_g     : 4;
		uint32_t reduction_op     : 3; // 0=op_red_add 1='min 2='max 3='inc 4='dec 5='and 6='or 7='xor
		uint32_t qmd_reserved_k_m : 1;
		uint32_t reduction_format : 2; // 0=unsigned_32 1=signed_32
		uint32_t reduction_enable : 1;
		uint32_t _missing_f_h     : 4;
		uint32_t structure_size   : 1; // 0=four_words 1=one_word

		// 0x64; 0x70
		uint32_t payload;
	} release[2];

	// 0x74
	struct
	{
		// 0x74; 0x7C; 0x84; 0x8C; 0x94; 0x9C; 0xA4; 0xAC
		uint32_t addr_lower;

		// 0x78; 0x80; 0x88; 0x90; 0x98; 0xA0; 0xA8; 0xB0
		uint32_t addr_upper    : 8;
		uint32_t reserved_addr : 6;
		uint32_t invalidate    : 1;
		uint32_t size          : 17;
	} constant_buffer[8];

	// 0xB4
	uint32_t shader_local_memory_low_size : 24;
	uint32_t qmd_reserved_n               : 3;
	uint32_t barrier_count                : 5;

	// 0xB8
	uint32_t shader_local_memory_high_size : 24;
	uint32_t register_count                : 8;

	// 0xBC
	uint32_t shader_local_memory_crs_size : 24;
	uint32_t sass_version                 : 8;

	// 0xC0
	uint32_t hw_only_inner_get               : 31;
	uint32_t hw_only_require_scheduling_pcas : 1;

	// 0xC4
	uint32_t hw_only_inner_put : 31;
	uint32_t hw_only_scg_type  : 1;

	// 0xC8
	uint32_t hw_only_span_list_head_index       : 30;
	uint32_t qmd_reserved_q                     : 1;
	uint32_t hw_only_span_list_head_index_valid : 1;

	// 0xCC
	uint32_t hw_only_sked_next_qmd_pointer;

	// 0xD0; 0xD4; 0xD8; 0xDC; 0xE0; 0xE4; 0xE8; 0xEC; 0xF0; 0xF4
	uint32_t qmd_spare_efghijklmn[10];

	// 0xF8
	uint32_t debug_id_lower;

	// 0xFC
	uint32_t debug_id_upper;
};

static_assert(sizeof(ComputeQmd)==0x100, "Invalid definition of ComputeQmd");

}
