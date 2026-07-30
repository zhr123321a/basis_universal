// basisu_uastc_enc.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "basisu_etc.h"

#include "../transcoder/basisu_transcoder_uastc.h"

namespace basisu
{
	const uint32_t TOTAL_PACK_UASTC_LEVELS = 5;

	enum
	{
		// Fastest is the lowest quality, although it's stil substantially higher quality vs. BC1/ETC1. It supports 5 modes.
		// The output may be somewhat blocky because this setting doesn't support 2/3-subset UASTC modes, but it should be less blocky vs. BC1/ETC1.
		// This setting doesn't write BC1 hints, so BC1 transcoding will be slower.
		// Transcoded ETC1 quality will be lower because it only considers 2 hints out of 32.
		// Avg. 43.45 dB
		cPackUASTCLevelFastest = 0,

		// Faster is ~3x slower than fastest. It supports 9 modes.
		// Avg. 46.49 dB
		cPackUASTCLevelFaster = 1,

		// Default is ~5.5x slower than fastest. It supports 14 modes.
		// Avg. 47.47 dB
		cPackUASTCLevelDefault = 2,

		// Slower is ~14.5x slower than fastest. It supports all 18 modes.
		// Avg. 48.01 dB
		cPackUASTCLevelSlower = 3,

		// VerySlow is ~200x slower than fastest.
		// The best quality the codec is capable of, but you'll need to be patient or have a lot of cores.
		// Avg. 48.24 dB
		cPackUASTCLevelVerySlow = 4,

		cPackUASTCLevelMask = 0xF,

		// By default the encoder tries to strike a balance between UASTC and transcoded BC7 quality.
		// These flags allow you to favor only optimizing for lowest UASTC error, or lowest BC7 error.
		cPackUASTCFavorUASTCError = 8,
		cPackUASTCFavorBC7Error = 16,

		cPackUASTCETC1FasterHints = 64,
		cPackUASTCETC1FastestHints = 128,
		cPackUASTCETC1DisableFlipAndIndividual = 256,

		// Favor UASTC modes 0 and 10 more than the others (this is experimental, it's useful for RDO compression)
		cPackUASTCFavorSimplerModes = 512,
	};

	// pRGBAPixels: Pointer to source 4x4 block of RGBA pixels (R first in memory).
	// block: Reference to destination UASTC block.
	// level: Controls compression speed vs. performance tradeoff.
	struct uastc_encode_profile
	{
		double input_analysis_ms = 0.0;
		double candidate_generation_ms = 0.0;
		double candidate_scoring_ms = 0.0;
		double hint_generation_ms = 0.0;
		double pack_ms = 0.0;
		double total_ms = 0.0;
	};

	// Optional per-block search control used by DASTC-U1's memory-of-past
	// training policy. The override is intersected with the modes enabled by
	// the selected UASTC effort level and the block's RGB/RGBA/LA class. If the
	// intersection is empty, the encoder safely falls back to the normal full
	// search for that block.
	struct uastc_encode_options
	{
		bool use_mode_mask_override = false;
		uint32_t mode_mask_override = UINT32_MAX;

		// ASTC-only consumers do not use the BC1, ETC1, or EAC
		// transcoding hints embedded in the UASTC intermediate. Keep all
		// selected ASTC data unchanged while writing deterministic legal
		// placeholder hint bits.
		bool skip_transcoding_hints = false;
	};

	struct uastc_encode_feedback
	{
		struct mode_config
		{
			bool valid = false;
			uint8_t common_pattern = 0;
			uint8_t ccs = 0;
			uint16_t partition_seed = 0;
			uint8_t endpoints[18] = {};
			uint8_t weights[32] = {};
			uint8_t solid_color[4] = {};
		};

		uint32_t selected_mode = UINT32_MAX;
		uint32_t evaluated_mode_mask = 0;
		bool mode_mask_fallback = false;
		uint64_t mode_astc_error[basist::TOTAL_UASTC_MODES];
		mode_config mode_configs[basist::TOTAL_UASTC_MODES];

		uastc_encode_feedback()
		{
			for (uint32_t mode = 0; mode < basist::TOTAL_UASTC_MODES; ++mode)
			{
				mode_astc_error[mode] = UINT64_MAX;
				mode_configs[mode] = mode_config{};
			}
		}
	};

	void encode_uastc(
		const uint8_t* pRGBAPixels,
		basist::uastc_block& output_block,
		uint32_t flags = cPackUASTCLevelDefault,
		uastc_encode_profile* profile = nullptr,
		const uastc_encode_options* options = nullptr,
		uastc_encode_feedback* feedback = nullptr);

	struct uastc_encode_results
	{
		uint32_t m_uastc_mode;
		uint32_t m_common_pattern;
		basist::astc_block_desc m_astc;
		color_rgba m_solid_color;
		uint64_t m_astc_err;
	};

	void pack_uastc(basist::uastc_block& blk, const uastc_encode_results& result, const etc_block& etc1_blk, uint32_t etc1_bias, const eac_a8_block& etc_eac_a8_blk, bool bc1_hint0, bool bc1_hint1);

	const uint32_t UASCT_RDO_DEFAULT_LZ_DICT_SIZE = 4096;

	const float UASTC_RDO_DEFAULT_MAX_ALLOWED_RMS_INCREASE_RATIO = 10.0f;
	const float UASTC_RDO_DEFAULT_SKIP_BLOCK_RMS_THRESH = 8.0f;

	// The RDO encoder computes a smoothness factor, from [0,1], for each block. To do this it computes each block's maximum component variance, then it divides this by this factor and clamps the result.
	// Larger values will result in more blocks being protected from too much distortion.
	const float UASTC_RDO_DEFAULT_MAX_SMOOTH_BLOCK_STD_DEV = 18.0f;

	// The RDO encoder can artifically boost the error of smooth blocks, in order to suppress distortions on smooth areas of the texture.
	// The encoder will use this value as the maximum error scale to use on smooth blocks. The larger this value, the better smooth bocks will look. Set to 1.0 to disable this completely.
	const float UASTC_RDO_DEFAULT_SMOOTH_BLOCK_MAX_ERROR_SCALE = 10.0f;

	struct uastc_rdo_params
	{
		uastc_rdo_params()
		{
			clear();
		}

		void clear()
		{
			m_lz_dict_size = UASCT_RDO_DEFAULT_LZ_DICT_SIZE;
			m_lambda = 0.5f;
			m_max_allowed_rms_increase_ratio = UASTC_RDO_DEFAULT_MAX_ALLOWED_RMS_INCREASE_RATIO;
			m_skip_block_rms_thresh = UASTC_RDO_DEFAULT_SKIP_BLOCK_RMS_THRESH;
			m_endpoint_refinement = true;
			m_lz_literal_cost = 100;

			m_max_smooth_block_std_dev = UASTC_RDO_DEFAULT_MAX_SMOOTH_BLOCK_STD_DEV;
			m_smooth_block_max_error_scale = UASTC_RDO_DEFAULT_SMOOTH_BLOCK_MAX_ERROR_SCALE;
		}

		// m_lz_dict_size: Size of LZ dictionary to simulate in bytes. The larger this value, the slower the encoder but the higher the quality per LZ compressed bit.
		uint32_t m_lz_dict_size;

		// m_lambda: The post-processor tries to reduce distortion+rate*lambda (rate is approximate LZ bits and distortion is scaled MS error).
		// Larger values push the postprocessor towards optimizing more for lower rate, and smaller values more for distortion. 0=minimal distortion.
		float m_lambda;

		// m_max_allowed_rms_increase_ratio: How much the RMS error of a block is allowed to increase before a trial is rejected. 1.0=no increase allowed, 1.05=5% increase allowed, etc.
		float m_max_allowed_rms_increase_ratio;

		// m_skip_block_rms_thresh: Blocks with this much RMS error or more are completely skipped by the RDO encoder.
		float m_skip_block_rms_thresh;

		// m_endpoint_refinement: If true, the post-process will attempt to refine the endpoints of blocks with modified selectors.
		bool m_endpoint_refinement;

		float m_max_smooth_block_std_dev;
		float m_smooth_block_max_error_scale;

		uint32_t m_lz_literal_cost;
	};

	// num_blocks, pBlocks: Number of blocks and pointer to UASTC blocks to process.
	// pBlock_pixels: Pointer to an array of 4x4 blocks containing the original texture pixels. This is NOT a raster image, but a pointer to individual 4x4 blocks.
	// flags: Pass in the same flags used to encode the UASTC blocks. The flags are used to reencode the transcode hints in the same way.
	bool uastc_rdo(uint32_t num_blocks, basist::uastc_block* pBlocks, const color_rgba* pBlock_pixels, const uastc_rdo_params &params, uint32_t flags = cPackUASTCLevelDefault, job_pool* pJob_pool = nullptr, uint32_t total_jobs = 0);
} // namespace basisu
