/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_CTRL_HW_H_
#define _DSI_CTRL_HW_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>

#include "dsi_defs.h"

/**
 * Modifier flag for command transmission. If this flag is set, command
 * information is programmed to hardware and transmission is not triggered.
 * Caller should call the trigger_command_dma() to start the transmission. This
 * flag is valed for kickoff_command() and kickoff_fifo_command() operations.
 */
#define DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER            0x1

/**
 * enum dsi_ctrl_version - version of the dsi host controller
 * @DSI_CTRL_VERSION_UNKNOWN: Unknown controller version
 * @DSI_CTRL_VERSION_1_4:     DSI host v1.4 controller
 * @DSI_CTRL_VERSION_2_0:     DSI host v2.0 controller
 * @DSI_CTRL_VERSION_MAX:     max version
 */
enum dsi_ctrl_version {
	DSI_CTRL_VERSION_UNKNOWN,
	DSI_CTRL_VERSION_1_4,
	DSI_CTRL_VERSION_2_0,
	DSI_CTRL_VERSION_MAX
};

/**
 * enum dsi_ctrl_hw_features - features supported by dsi host controller
 * @DSI_CTRL_VIDEO_TPG:               Test pattern support for video mode.
 * @DSI_CTRL_CMD_TPG:                 Test pattern support for command mode.
 * @DSI_CTRL_VARIABLE_REFRESH_RATE:   variable panel timing
 * @DSI_CTRL_DYNAMIC_REFRESH:         variable pixel clock rate
 * @DSI_CTRL_NULL_PACKET_INSERTION:   NULL packet insertion
 * @DSI_CTRL_DESKEW_CALIB:            Deskew calibration support
 * @DSI_CTRL_DPHY:                    Controller support for DPHY
 * @DSI_CTRL_CPHY:                    Controller support for CPHY
 * @DSI_CTRL_MAX_FEATURES:
 */
enum dsi_ctrl_hw_features {
	DSI_CTRL_VIDEO_TPG,
	DSI_CTRL_CMD_TPG,
	DSI_CTRL_VARIABLE_REFRESH_RATE,
	DSI_CTRL_DYNAMIC_REFRESH,
	DSI_CTRL_NULL_PACKET_INSERTION,
	DSI_CTRL_DESKEW_CALIB,
	DSI_CTRL_DPHY,
	DSI_CTRL_CPHY,
	DSI_CTRL_MAX_FEATURES
};

/**
 * enum dsi_test_pattern - test pattern type
 * @DSI_TEST_PATTERN_FIXED:     Test pattern is fixed, based on init value.
 * @DSI_TEST_PATTERN_INC:       Incremental test pattern, base on init value.
 * @DSI_TEST_PATTERN_POLY:      Pattern generated from polynomial and init val.
 * @DSI_TEST_PATTERN_MAX:
 */
enum dsi_test_pattern {
	DSI_TEST_PATTERN_FIXED = 0,
	DSI_TEST_PATTERN_INC,
	DSI_TEST_PATTERN_POLY,
	DSI_TEST_PATTERN_MAX
};

/**
 * enum dsi_status_int_type - status interrupts generated by DSI controller
 * @DSI_CMD_MODE_DMA_DONE:        Command mode DMA packets are sent out.
 * @DSI_CMD_STREAM0_FRAME_DONE:   A frame of command mode stream0 is sent out.
 * @DSI_CMD_STREAM1_FRAME_DONE:   A frame of command mode stream1 is sent out.
 * @DSI_CMD_STREAM2_FRAME_DONE:   A frame of command mode stream2 is sent out.
 * @DSI_VIDEO_MODE_FRAME_DONE:    A frame of video mode stream is sent out.
 * @DSI_BTA_DONE:                 A BTA is completed.
 * @DSI_CMD_FRAME_DONE:           A frame of selected command mode stream is
 *                                sent out by MDP.
 * @DSI_DYN_REFRESH_DONE:         The dynamic refresh operation has completed.
 * @DSI_DESKEW_DONE:              The deskew calibration operation has completed
 * @DSI_DYN_BLANK_DMA_DONE:       The dynamic blank in DMA operation has
 *                                completed.
 */
enum dsi_status_int_type {
	DSI_CMD_MODE_DMA_DONE = BIT(0),
	DSI_CMD_STREAM0_FRAME_DONE = BIT(1),
	DSI_CMD_STREAM1_FRAME_DONE = BIT(2),
	DSI_CMD_STREAM2_FRAME_DONE = BIT(3),
	DSI_VIDEO_MODE_FRAME_DONE = BIT(4),
	DSI_BTA_DONE = BIT(5),
	DSI_CMD_FRAME_DONE = BIT(6),
	DSI_DYN_REFRESH_DONE = BIT(7),
	DSI_DESKEW_DONE = BIT(8),
	DSI_DYN_BLANK_DMA_DONE = BIT(9)
};

/**
 * enum dsi_error_int_type - error interrupts generated by DSI controller
 * @DSI_RDBK_SINGLE_ECC_ERR:        Single bit ECC error in read packet.
 * @DSI_RDBK_MULTI_ECC_ERR:         Multi bit ECC error in read packet.
 * @DSI_RDBK_CRC_ERR:               CRC error in read packet.
 * @DSI_RDBK_INCOMPLETE_PKT:        Incomplete read packet.
 * @DSI_PERIPH_ERROR_PKT:           Error packet returned from peripheral.
 * @DSI_LP_RX_TIMEOUT:              Low power reverse transmission timeout.
 * @DSI_HS_TX_TIMEOUT:              High speed forward transmission timeout.
 * @DSI_BTA_TIMEOUT:                BTA timeout.
 * @DSI_PLL_UNLOCK:                 PLL has unlocked.
 * @DSI_DLN0_ESC_ENTRY_ERR:         Incorrect LP Rx escape entry.
 * @DSI_DLN0_ESC_SYNC_ERR:          LP Rx data is not byte aligned.
 * @DSI_DLN0_LP_CONTROL_ERR:        Incorrect LP Rx state sequence.
 * @DSI_PENDING_HS_TX_TIMEOUT:      Pending High-speed transfer timeout.
 * @DSI_INTERLEAVE_OP_CONTENTION:   Interleave operation contention.
 * @DSI_CMD_DMA_FIFO_UNDERFLOW:     Command mode DMA FIFO underflow.
 * @DSI_CMD_MDP_FIFO_UNDERFLOW:     Command MDP FIFO underflow (failed to
 *                                  receive one complete line from MDP).
 * @DSI_DLN0_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 0 overflows.
 * @DSI_DLN1_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 1 overflows.
 * @DSI_DLN2_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 2 overflows.
 * @DSI_DLN3_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 3 overflows.
 * @DSI_DLN0_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 0 underflows.
 * @DSI_DLN1_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 1 underflows.
 * @DSI_DLN2_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 2 underflows.
 * @DSI_DLN3_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 3 underflows.
 * @DSI_DLN0_LP0_CONTENTION:        PHY level contention while lane 0 is low.
 * @DSI_DLN1_LP0_CONTENTION:        PHY level contention while lane 1 is low.
 * @DSI_DLN2_LP0_CONTENTION:        PHY level contention while lane 2 is low.
 * @DSI_DLN3_LP0_CONTENTION:        PHY level contention while lane 3 is low.
 * @DSI_DLN0_LP1_CONTENTION:        PHY level contention while lane 0 is high.
 * @DSI_DLN1_LP1_CONTENTION:        PHY level contention while lane 1 is high.
 * @DSI_DLN2_LP1_CONTENTION:        PHY level contention while lane 2 is high.
 * @DSI_DLN3_LP1_CONTENTION:        PHY level contention while lane 3 is high.
 */
enum dsi_error_int_type {
	DSI_RDBK_SINGLE_ECC_ERR = BIT(0),
	DSI_RDBK_MULTI_ECC_ERR = BIT(1),
	DSI_RDBK_CRC_ERR = BIT(2),
	DSI_RDBK_INCOMPLETE_PKT = BIT(3),
	DSI_PERIPH_ERROR_PKT = BIT(4),
	DSI_LP_RX_TIMEOUT = BIT(5),
	DSI_HS_TX_TIMEOUT = BIT(6),
	DSI_BTA_TIMEOUT = BIT(7),
	DSI_PLL_UNLOCK = BIT(8),
	DSI_DLN0_ESC_ENTRY_ERR = BIT(9),
	DSI_DLN0_ESC_SYNC_ERR = BIT(10),
	DSI_DLN0_LP_CONTROL_ERR = BIT(11),
	DSI_PENDING_HS_TX_TIMEOUT = BIT(12),
	DSI_INTERLEAVE_OP_CONTENTION = BIT(13),
	DSI_CMD_DMA_FIFO_UNDERFLOW = BIT(14),
	DSI_CMD_MDP_FIFO_UNDERFLOW = BIT(15),
	DSI_DLN0_HS_FIFO_OVERFLOW = BIT(16),
	DSI_DLN1_HS_FIFO_OVERFLOW = BIT(17),
	DSI_DLN2_HS_FIFO_OVERFLOW = BIT(18),
	DSI_DLN3_HS_FIFO_OVERFLOW = BIT(19),
	DSI_DLN0_HS_FIFO_UNDERFLOW = BIT(20),
	DSI_DLN1_HS_FIFO_UNDERFLOW = BIT(21),
	DSI_DLN2_HS_FIFO_UNDERFLOW = BIT(22),
	DSI_DLN3_HS_FIFO_UNDERFLOW = BIT(23),
	DSI_DLN0_LP0_CONTENTION = BIT(24),
	DSI_DLN1_LP0_CONTENTION = BIT(25),
	DSI_DLN2_LP0_CONTENTION = BIT(26),
	DSI_DLN3_LP0_CONTENTION = BIT(27),
	DSI_DLN0_LP1_CONTENTION = BIT(28),
	DSI_DLN1_LP1_CONTENTION = BIT(29),
	DSI_DLN2_LP1_CONTENTION = BIT(30),
	DSI_DLN3_LP1_CONTENTION = BIT(31),
};

/**
 * struct dsi_ctrl_cmd_dma_info - command buffer information
 * @offset:        IOMMU VA for command buffer address.
 * @length:        Length of the command buffer.
 * @en_broadcast:  Enable broadcast mode if set to true.
 * @is_master:     Is master in broadcast mode.
 * @use_lpm:       Use low power mode for command transmission.
 */
struct dsi_ctrl_cmd_dma_info {
	u32 offset;
	u32 length;
	bool en_broadcast;
	bool is_master;
	bool use_lpm;
};

/**
 * struct dsi_ctrl_cmd_dma_fifo_info - command payload tp be sent using FIFO
 * @command:        VA for command buffer.
 * @size:           Size of the command buffer.
 * @en_broadcast:   Enable broadcast mode if set to true.
 * @is_master:      Is master in broadcast mode.
 * @use_lpm:        Use low power mode for command transmission.
 */
struct dsi_ctrl_cmd_dma_fifo_info {
	u32 *command;
	u32 size;
	bool en_broadcast;
	bool is_master;
	bool use_lpm;
};

struct dsi_ctrl_hw;

/**
 * struct dsi_ctrl_hw_ops - operations supported by dsi host hardware
 */
struct dsi_ctrl_hw_ops {

	/**
	 * host_setup() - Setup DSI host configuration
	 * @ctrl:          Pointer to controller host hardware.
	 * @config:        Configuration for DSI host controller
	 */
	void (*host_setup)(struct dsi_ctrl_hw *ctrl,
			   struct dsi_host_common_cfg *config);

	/**
	 * video_engine_en() - enable DSI video engine
	 * @ctrl:          Pointer to controller host hardware.
	 * @on:            Enable/disabel video engine.
	 */
	void (*video_engine_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * video_engine_setup() - Setup dsi host controller for video mode
	 * @ctrl:          Pointer to controller host hardware.
	 * @common_cfg:    Common configuration parameters.
	 * @cfg:           Video mode configuration.
	 *
	 * Set up DSI video engine with a specific configuration. Controller and
	 * video engine are not enabled as part of this function.
	 */
	void (*video_engine_setup)(struct dsi_ctrl_hw *ctrl,
				   struct dsi_host_common_cfg *common_cfg,
				   struct dsi_video_engine_cfg *cfg);

	/**
	 * set_video_timing() - set up the timing for video frame
	 * @ctrl:          Pointer to controller host hardware.
	 * @mode:          Video mode information.
	 *
	 * Set up the video timing parameters for the DSI video mode operation.
	 */
	void (*set_video_timing)(struct dsi_ctrl_hw *ctrl,
				 struct dsi_mode_info *mode);

	/**
	 * cmd_engine_setup() - setup dsi host controller for command mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @common_cfg:    Common configuration parameters.
	 * @cfg:           Command mode configuration.
	 *
	 * Setup DSI CMD engine with a specific configuration. Controller and
	 * command engine are not enabled as part of this function.
	 */
	void (*cmd_engine_setup)(struct dsi_ctrl_hw *ctrl,
				 struct dsi_host_common_cfg *common_cfg,
				 struct dsi_cmd_engine_cfg *cfg);

	/**
	 * ctrl_en() - enable DSI controller engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @on:            turn on/off the DSI controller engine.
	 */
	void (*ctrl_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * cmd_engine_en() - enable DSI controller command engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @on:            Turn on/off the DSI command engine.
	 */
	void (*cmd_engine_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * phy_sw_reset() - perform a soft reset on the PHY.
	 * @ctrl:        Pointer to the controller host hardware.
	 */
	void (*phy_sw_reset)(struct dsi_ctrl_hw *ctrl);

	/**
	 * soft_reset() - perform a soft reset on DSI controller
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * The video, command and controller engines will be disable before the
	 * reset is triggered. These engines will not be enabled after the reset
	 * is complete. Caller must re-enable the engines.
	 *
	 * If the reset is done while MDP timing engine is turned on, the video
	 * enigne should be re-enabled only during the vertical blanking time.
	 */
	void (*soft_reset)(struct dsi_ctrl_hw *ctrl);

	/**
	 * setup_lane_map() - setup mapping between logical and physical lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lane_map:      Structure defining the mapping between DSI logical
	 *                 lanes and physical lanes.
	 */
	void (*setup_lane_map)(struct dsi_ctrl_hw *ctrl,
			       struct dsi_lane_mapping *lane_map);

	/**
	 * kickoff_command() - transmits commands stored in memory
	 * @ctrl:          Pointer to the controller host hardware.
	 * @cmd:           Command information.
	 * @flags:         Modifiers for command transmission.
	 *
	 * The controller hardware is programmed with address and size of the
	 * command buffer. The transmission is kicked off if
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
	 * set, caller should make a separate call to trigger_command_dma() to
	 * transmit the command.
	 */
	void (*kickoff_command)(struct dsi_ctrl_hw *ctrl,
				struct dsi_ctrl_cmd_dma_info *cmd,
				u32 flags);

	/**
	 * kickoff_fifo_command() - transmits a command using FIFO in dsi
	 *                          hardware.
	 * @ctrl:          Pointer to the controller host hardware.
	 * @cmd:           Command information.
	 * @flags:         Modifiers for command transmission.
	 *
	 * The controller hardware FIFO is programmed with command header and
	 * payload. The transmission is kicked off if
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
	 * set, caller should make a separate call to trigger_command_dma() to
	 * transmit the command.
	 */
	void (*kickoff_fifo_command)(struct dsi_ctrl_hw *ctrl,
				     struct dsi_ctrl_cmd_dma_fifo_info *cmd,
				     u32 flags);

	void (*reset_cmd_fifo)(struct dsi_ctrl_hw *ctrl);
	/**
	 * trigger_command_dma() - trigger transmission of command buffer.
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * This trigger can be only used if there was a prior call to
	 * kickoff_command() of kickoff_fifo_command() with
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag.
	 */
	void (*trigger_command_dma)(struct dsi_ctrl_hw *ctrl);

	/**
	 * get_cmd_read_data() - get data read from the peripheral
	 * @ctrl:           Pointer to the controller host hardware.
	 * @rd_buf:         Buffer where data will be read into.
	 * @total_read_len: Number of bytes to read.
	 */
	u32 (*get_cmd_read_data)(struct dsi_ctrl_hw *ctrl,
				 u8 *rd_buf,
				 u32 total_read_len);

	/**
	 * ulps_request() - request ulps entry for specified lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to enter ULPS.
	 *
	 * Caller should check if lanes are in ULPS mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_request)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	/**
	 * ulps_exit() - exit ULPS on specified lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to exit ULPS.
	 *
	 * Caller should check if lanes are in active mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_exit)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	/**
	 * clear_ulps_request() - clear ulps request once all lanes are active
	 * @ctrl:          Pointer to controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes).
	 *
	 * ULPS request should be cleared after the lanes have exited ULPS.
	 */
	void (*clear_ulps_request)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	/**
	 * get_lanes_in_ulps() - returns the list of lanes in ULPS mode
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns an ORed list of lanes (enum dsi_data_lanes) that are in ULPS
	 * state. If 0 is returned, all the lanes are active.
	 *
	 * Return: List of lanes in ULPS state.
	 */
	u32 (*get_lanes_in_ulps)(struct dsi_ctrl_hw *ctrl);

	/**
	 * clamp_enable() - enable DSI clamps to keep PHY driving a stable link
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes which need to be clamped.
	 * @enable_ulps:   TODO:??
	 */
	void (*clamp_enable)(struct dsi_ctrl_hw *ctrl,
			     u32 lanes,
			     bool enable_ulps);

	/**
	 * clamp_disable() - disable DSI clamps
	 * @ctrl:         Pointer to the controller host hardware.
	 * @lanes:        ORed list of lanes which need to have clamps released.
	 * @disable_ulps: TODO:??
	 */
	void (*clamp_disable)(struct dsi_ctrl_hw *ctrl,
			      u32 lanes,
			      bool disable_ulps);

	/**
	 * get_interrupt_status() - returns the interrupt status
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns the ORed list of interrupts(enum dsi_status_int_type) that
	 * are active. This list does not include any error interrupts. Caller
	 * should call get_error_status for error interrupts.
	 *
	 * Return: List of active interrupts.
	 */
	u32 (*get_interrupt_status)(struct dsi_ctrl_hw *ctrl);

	/**
	 * clear_interrupt_status() - clears the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @ints:          List of interrupts to be cleared.
	 */
	void (*clear_interrupt_status)(struct dsi_ctrl_hw *ctrl, u32 ints);

	/**
	 * enable_status_interrupts() - enable the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @ints:          List of interrupts to be enabled.
	 *
	 * Enables the specified interrupts. This list will override the
	 * previous interrupts enabled through this function. Caller has to
	 * maintain the state of the interrupts enabled. To disable all
	 * interrupts, set ints to 0.
	 */
	void (*enable_status_interrupts)(struct dsi_ctrl_hw *ctrl, u32 ints);

	/**
	 * get_error_status() - returns the error status
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns the ORed list of errors(enum dsi_error_int_type) that are
	 * active. This list does not include any status interrupts. Caller
	 * should call get_interrupt_status for status interrupts.
	 *
	 * Return: List of active error interrupts.
	 */
	u64 (*get_error_status)(struct dsi_ctrl_hw *ctrl);

	/**
	 * clear_error_status() - clears the specified errors
	 * @ctrl:          Pointer to the controller host hardware.
	 * @errors:          List of errors to be cleared.
	 */
	void (*clear_error_status)(struct dsi_ctrl_hw *ctrl, u64 errors);

	/**
	 * enable_error_interrupts() - enable the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @errors:        List of errors to be enabled.
	 *
	 * Enables the specified interrupts. This list will override the
	 * previous interrupts enabled through this function. Caller has to
	 * maintain the state of the interrupts enabled. To disable all
	 * interrupts, set errors to 0.
	 */
	void (*enable_error_interrupts)(struct dsi_ctrl_hw *ctrl, u64 errors);

	/**
	 * video_test_pattern_setup() - setup test pattern engine for video mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @type:          Type of test pattern.
	 * @init_val:      Initial value to use for generating test pattern.
	 */
	void (*video_test_pattern_setup)(struct dsi_ctrl_hw *ctrl,
					 enum dsi_test_pattern type,
					 u32 init_val);

	/**
	 * cmd_test_pattern_setup() - setup test patttern engine for cmd mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @type:          Type of test pattern.
	 * @init_val:      Initial value to use for generating test pattern.
	 * @stream_id:     Stream Id on which packets are generated.
	 */
	void (*cmd_test_pattern_setup)(struct dsi_ctrl_hw *ctrl,
				       enum dsi_test_pattern  type,
				       u32 init_val,
				       u32 stream_id);

	/**
	 * test_pattern_enable() - enable test pattern engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @enable:        Enable/Disable test pattern engine.
	 */
	void (*test_pattern_enable)(struct dsi_ctrl_hw *ctrl, bool enable);

	/**
	 * trigger_cmd_test_pattern() - trigger a command mode frame update with
	 *                              test pattern
	 * @ctrl:          Pointer to the controller host hardware.
	 * @stream_id:     Stream on which frame update is sent.
	 */
	void (*trigger_cmd_test_pattern)(struct dsi_ctrl_hw *ctrl,
					 u32 stream_id);
};

/*
 * struct dsi_ctrl_hw - DSI controller hardware object specific to an instance
 * @base:           VA for the DSI controller base address.
 * @length:         Length of the DSI controller register map.
 * @index:          Instance ID of the controller.
 * @feature_map:    Features supported by the DSI controller.
 * @ops:            Function pointers to the operations supported by the
 *                  controller.
 */
struct dsi_ctrl_hw {
	void __iomem *base;
	u32 length;
	void __iomem *mmss_misc_base;
	u32 mmss_misc_length;
	u32 index;

	/* features */
	DECLARE_BITMAP(feature_map, DSI_CTRL_MAX_FEATURES);
	struct dsi_ctrl_hw_ops ops;

	/* capabilities */
	u32 supported_interrupts;
	u64 supported_errors;
};

#endif /* _DSI_CTRL_HW_H_ */
