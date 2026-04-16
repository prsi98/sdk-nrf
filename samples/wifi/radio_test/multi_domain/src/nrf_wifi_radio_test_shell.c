/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* @file
 * @brief NRF Wi-Fi radio test shell module
 */

#include <fmac_main.h>
#include <nrf_wifi_radio_test_shell.h>
#include <util.h>
#include "common/fmac_api_common.h"
#ifdef CONFIG_NRF70_SR_COEX
#include <coex.h>
#endif

extern struct nrf_wifi_drv_priv_zep rpu_drv_priv_zep;
struct nrf_wifi_ctx_zep *ctx = &rpu_drv_priv_zep.rpu_ctx_zep;

#ifdef WIFI_NRF71
#ifdef PHY_RF_PARAM_GDRAM
static unsigned int vtf_voltage = 243;
static unsigned int vtf_temp = 25;
static unsigned int vtf_x0 = 0;
#endif
#endif

#define NRF_WIFI_RADIO_TEST_INIT_TIMEOUT_MS 5000

static bool check_test_in_prog(const struct shell *shell)
{
	if (ctx->conf_params.rx) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Disable RX\n");
		return false;
	}

	if (ctx->conf_params.tx) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Disable TX\n");
		return false;
	}

	if (ctx->rf_test_run) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Disable RF Test (%d)\n",
			      ctx->rf_test);
		return false;
	}
	return true;
}


static bool check_valid_data_rate(const struct shell *shell,
				  unsigned char tput_mode,
				  unsigned int nss,
				  int dr)
{
	bool is_mcs = dr & 0x80;
	bool ret = false;

	if (dr == -1)
		return true;

	if (is_mcs) {
		dr = dr & 0x7F;

		if (tput_mode & RPU_TPUT_MODE_HT) {
			if (nss == 2) {
				if ((dr >= 8) && (dr <= 15)) {
					ret = true;
				} else {
					shell_fprintf(shell,
						      SHELL_ERROR,
						      "Invalid MIMO HT MCS: %d\n",
						       dr);
				}
			}

			if (nss == 1) {
				if ((dr >= 0) && (dr <= 7)) {
					ret = true;
				} else {
					shell_fprintf(shell,
						      SHELL_ERROR,
						      "Invalid SISO HT MCS: %d\n",
						      dr);
				}
			}

		} else if (tput_mode == RPU_TPUT_MODE_VHT) {
			if ((dr >= 0 && dr <= 9)) {
				ret = true;
			} else {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "Invalid VHT MCS value: %d\n",
					      dr);
			}
		} else if (tput_mode == RPU_TPUT_MODE_HE_SU) {
			if ((dr >= 0 && dr <= 7)) {
				ret = true;
			} else {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "Invalid HE_SU MCS value: %d\n",
					      dr);
			}
		} else if (tput_mode == RPU_TPUT_MODE_HE_ER_SU) {
			if ((dr >= 0 && dr <= 2)) {
				ret = true;
			} else {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "Invalid HE_ER_SU MCS value: %d\n",
					      dr);
			}
		} else {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "%s: Invalid throughput mode: %d\n", __func__,
				      dr);
		}
	} else {
		if (tput_mode != RPU_TPUT_MODE_LEGACY) {
			ret = false;

			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid rate_flags for legacy: %d\n",
				      dr);
		}

		if ((dr == 1) ||
		    (dr == 2) ||
		    (dr == 55) ||
		    (dr == 11) ||
		    (dr == 6) ||
		    (dr == 9) ||
		    (dr == 12) ||
		    (dr == 18) ||
		    (dr == 24) ||
		    (dr == 36) ||
		    (dr == 48) ||
		    (dr == 54) ||
		    (dr == -1)) {
			ret = true;
		} else {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid Legacy Rate value: %d\n",
				      dr);
		}
	}

	return ret;
}


static bool check_valid_chan_2g(unsigned char chan_num)
{
	if ((chan_num >= 1) && (chan_num <= 14)) {
		return true;
	}

	return false;
}


#ifndef CONFIG_NRF70_2_4G_ONLY
static bool check_valid_chan_5g(unsigned char chan_num)
{
	if ((chan_num == 32) ||
	    (chan_num == 36) ||
	    (chan_num == 40) ||
	    (chan_num == 44) ||
	    (chan_num == 48) ||
	    (chan_num == 52) ||
	    (chan_num == 56) ||
	    (chan_num == 60) ||
	    (chan_num == 64) ||
	    (chan_num == 68) ||
	    (chan_num == 96) ||
	    (chan_num == 100) ||
	    (chan_num == 104) ||
	    (chan_num == 108) ||
	    (chan_num == 112) ||
	    (chan_num == 116) ||
	    (chan_num == 120) ||
	    (chan_num == 124) ||
	    (chan_num == 128) ||
	    (chan_num == 132) ||
	    (chan_num == 136) ||
	    (chan_num == 140) ||
	    (chan_num == 144) ||
	    (chan_num == 149) ||
	    (chan_num == 153) ||
	    (chan_num == 157) ||
	    (chan_num == 159) ||
	    (chan_num == 161) ||
	    (chan_num == 163) ||
	    (chan_num == 165) ||
	    (chan_num == 167) ||
	    (chan_num == 169) ||
	    (chan_num == 171) ||
	    (chan_num == 173) ||
	    (chan_num == 175) ||
	    (chan_num == 177)) {
		return true;
	}

	return false;
}
#endif /* CONFIG_NRF70_2_4G_ONLY */


static bool check_valid_channel(unsigned char chan_num)
{
	bool ret = false;

	ret = check_valid_chan_2g(chan_num);

	if (ret) {
		goto out;
	}

#ifndef CONFIG_NRF70_2_4G_ONLY
	ret = check_valid_chan_5g(chan_num);
#endif /* CONFIG_NRF70_2_4G_ONLY */

out:
	return ret;
}

static bool check_valid_chan_6g(unsigned char chan_num)
{
	if ((chan_num == 1) ||
	    (chan_num == 5) ||
	    (chan_num == 9) ||
	    (chan_num == 13) ||
	    (chan_num == 17) ||
	    (chan_num == 21) ||
	    (chan_num == 25) ||
	    (chan_num == 29) ||
	    (chan_num == 33) ||
	    (chan_num == 37) ||
	    (chan_num == 41) ||
	    (chan_num == 45) ||
	    (chan_num == 49) ||
	    (chan_num == 53) ||
	    (chan_num == 57) ||
	    (chan_num == 61) ||
	    (chan_num == 65) ||
	    (chan_num == 69) ||
	    (chan_num == 73) ||
	    (chan_num == 77) ||
	    (chan_num == 81) ||
	    (chan_num == 85) ||
	    (chan_num == 89) ||
	    (chan_num == 93)) {
		return true;
	}

	return false;
}

static int check_channel_settings(const struct shell *shell,
				  unsigned char tput_mode,
				  struct chan_params *chan)
{
	if (tput_mode == RPU_TPUT_MODE_LEGACY) {
		if (chan->bw != RPU_CH_BW_20) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid bandwidth setting for legacy channel = %d\n",
				      chan->primary_num);
			return -1;
		}
	} else if (tput_mode == RPU_TPUT_MODE_HT) {
		if (chan->bw != RPU_CH_BW_20) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid bandwidth setting for HT channel = %d\n",
				      chan->primary_num);
			return -1;
		}
	} else if (tput_mode == RPU_TPUT_MODE_VHT) {
		if ((chan->primary_num >= 1) &&
		    (chan->primary_num <= 14)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "VHT setting not allowed in 2.4GHz band\n");
			return -1;
		}

		if (chan->bw != RPU_CH_BW_20) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid bandwidth setting for VHT channel = %d\n",
				      chan->primary_num);
			return -1;
		}
	} else if ((tput_mode == RPU_TPUT_MODE_HE_SU) ||
		   (tput_mode == RPU_TPUT_MODE_HE_TB) ||
		   (tput_mode == RPU_TPUT_MODE_HE_ER_SU)) {
		if (chan->bw != RPU_CH_BW_20) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid bandwidth setting for HE channel = %d\n",
				      chan->primary_num);
			return -1;
		}
	} else {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid throughput mode = %d\n",
			      tput_mode);
		return -1;
	}

	return 0;
}


enum nrf_wifi_status nrf_wifi_radio_test_conf_init(struct rpu_conf_params *conf_params)
{
	int ret;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned char country_code[NRF_WIFI_COUNTRY_CODE_LEN] = {0};

#ifdef WIFI_NRF71
	unsigned int rx_bss_color = 0;
	unsigned int rx_station_id = 0;
#endif /* !WIFI_NRF71 */

	/* Check and save regulatory country code currently set */
	if (strlen(conf_params->country_code)) {
		memcpy(country_code,
		       conf_params->country_code,
		       NRF_WIFI_COUNTRY_CODE_LEN);
	}

#ifdef WIFI_NRF71

	if(conf_params->rx_bss_color != 0) {
		rx_bss_color = conf_params->rx_bss_color;
	}
	if(conf_params->rx_station_id != 0) {
		rx_station_id = conf_params->rx_station_id;
	}

#endif /* !WIFI_NRF71 */

	memset(conf_params,
	       0,
	       sizeof(*conf_params));

	/* Initialize values which are other than 0 */
	conf_params->op_mode = RPU_OP_MODE_RADIO_TEST;

#ifndef NRF71_ON_IPC
#ifdef WIFI_NRF71
#ifdef PHY_RF_PARAM_GDRAM
	status = nrf_wifi_fmac_config_rf_params(ctx->rpu_ctx,
                                                &conf_params->rf_params_addr[0]);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                goto out;
        }

        status = nrf_wifi_fmac_config_vtf_params(ctx->rpu_ctx,
                                                 vtf_voltage,
                                                 vtf_temp,
                                                 vtf_x0,
					 	 &conf_params->vtf_buffer_addr);
        if (status != NRF_WIFI_STATUS_SUCCESS) {
                goto out;
        }
#else
	nrf_wifi_osal_mem_set(conf_params->rf_params,
							0x00,
							NRF_WIFI_RF_PARAMS_SIZE);

	ret = nrf_wifi_utils_hex_str_to_val(
			conf_params->rf_params,
			NRF_WIFI_RF_PARAMS_SIZE,
			NRF_WIFI_DEF_RF_PARAMS);
	if (ret == -1) {

		/*
		shell_fprintf(shell,
					  SHELL_ERROR,
					  "%s: Initialization of RF params with default values failed\n",
					  __func__);
		*/
		status = NRF_WIFI_STATUS_FAIL;
		goto out;
	}

	status = NRF_WIFI_STATUS_SUCCESS;
#endif /* PHY_RF_PARAM_GDRAM */
#else /* WIFI_NRF71 */
	status = nrf_wifi_rt_fmac_rf_params_get(
			ctx->rpu_ctx,
			(struct nrf_wifi_phy_rf_params *)conf_params->rf_params);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		goto out;
	}
#endif /* !WIFI_NRF71 */
#endif /* !NRF71_ON_IPC */

	conf_params->tx_pkt_nss = 1;
	conf_params->tx_pkt_gap_us = 0;

	conf_params->tx_power = MAX_TX_PWR_SYS_TEST;

	conf_params->chan.primary_num = 1;
	conf_params->tx_mode = 1;
	conf_params->tx_pkt_num = -1;
	conf_params->tx_pkt_len = 1400;
	conf_params->tx_pkt_preamble = 0;
	conf_params->tx_pkt_rate = 6;
	conf_params->he_ltf = 2;
	conf_params->he_gi = 2;
	conf_params->aux_adc_input_chain_id = 1;
	conf_params->ru_tone = 26;
	conf_params->ru_index = 1;
	conf_params->tx_pkt_cw = 15;
	conf_params->phy_calib = NRF_WIFI_DEF_PHY_CALIB;
#ifdef WIFI_NRF71
	conf_params->tx_tone_type = 0;
	conf_params->tx_tone_dc_offset_i = 0;
	conf_params->tx_tone_dc_offset_q = 0;
#endif

#ifdef WIFI_NRF71
	conf_params->rx_bss_color = rx_bss_color;

	conf_params->rx_station_id = rx_station_id;


#endif /* !WIFI_NRF71 */

	/* Store back the currently set country code */
	if (strlen(country_code)) {
		memcpy(conf_params->country_code,
		       country_code,
		       NRF_WIFI_COUNTRY_CODE_LEN);
	} else {
		memcpy(conf_params->country_code,
		       "00",
		       NRF_WIFI_COUNTRY_CODE_LEN);
	}
out:
	return status;
}


static int nrf_wifi_radio_test_set_defaults(const struct shell *shell,
					    size_t argc,
					    const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Configuration init failed\n");
		return -ENOEXEC;
	}

	return 0;
}


static int nrf_wifi_radio_test_set_phy_calib_rxdc(const struct shell *shell,
						  size_t argc,
						  const char *argv[])
{
	char *ptr = NULL;
	unsigned long phy_calib_rxdc = 0;

	phy_calib_rxdc = strtoul(argv[1], &ptr, 10);

	if (phy_calib_rxdc > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PHY RX DC calibration value(%lu).\n",
			      phy_calib_rxdc);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (phy_calib_rxdc)
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib |
					     NRF_WIFI_PHY_CALIB_FLAG_RXDC);
	else
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib &
					     ~(NRF_WIFI_PHY_CALIB_FLAG_RXDC));

	return 0;
}


static int nrf_wifi_radio_test_set_phy_calib_txdc(const struct shell *shell,
						  size_t argc,
						  const char *argv[])
{
	char *ptr = NULL;
	unsigned long phy_calib_txdc = 0;

	phy_calib_txdc = strtoul(argv[1], &ptr, 10);

	if (phy_calib_txdc > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PHY TX DC calibration value(%lu).\n",
			      phy_calib_txdc);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (phy_calib_txdc)
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib |
					     NRF_WIFI_PHY_CALIB_FLAG_TXDC);
	else
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib &
					     ~(NRF_WIFI_PHY_CALIB_FLAG_TXDC));

	return 0;
}


static int nrf_wifi_radio_test_set_phy_calib_txpow(const struct shell *shell,
						   size_t argc,
						   const char *argv[])
{
	char *ptr = NULL;
	unsigned long phy_calib_txpow = 0;

	phy_calib_txpow = strtoul(argv[1], &ptr, 10);

	if (phy_calib_txpow > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PHY TX power calibration value(%lu).\n",
			      phy_calib_txpow);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (phy_calib_txpow)
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib |
					     NRF_WIFI_PHY_CALIB_FLAG_TXPOW);
	else
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib &
					     ~(NRF_WIFI_PHY_CALIB_FLAG_TXPOW));

	return 0;
}


static int nrf_wifi_radio_test_set_phy_calib_rxiq(const struct shell *shell,
						  size_t argc,
						  const char *argv[])
{
	char *ptr = NULL;
	unsigned long phy_calib_rxiq = 0;

	phy_calib_rxiq = strtoul(argv[1], &ptr, 10);

	if (phy_calib_rxiq > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PHY RX IQ calibration value(%lu).\n",
			      phy_calib_rxiq);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (phy_calib_rxiq)
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib |
					     NRF_WIFI_PHY_CALIB_FLAG_RXIQ);
	else
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib &
					     ~(NRF_WIFI_PHY_CALIB_FLAG_RXIQ));

	return 0;
}


static int nrf_wifi_radio_test_set_phy_calib_txiq(const struct shell *shell,
						  size_t argc,
						  const char *argv[])
{
	char *ptr = NULL;
	unsigned long phy_calib_txiq = 0;

	phy_calib_txiq = strtoul(argv[1], &ptr, 10);

	if (phy_calib_txiq > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PHY TX IQ calibration value(%lu).\n",
			      phy_calib_txiq);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (phy_calib_txiq)
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib |
					     NRF_WIFI_PHY_CALIB_FLAG_TXIQ);
	else
		ctx->conf_params.phy_calib = (ctx->conf_params.phy_calib &
					     ~(NRF_WIFI_PHY_CALIB_FLAG_TXIQ));
	return 0;
}


static int nrf_wifi_radio_test_set_he_ltf(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	char *ptr = NULL;
	unsigned long he_ltf = 0;

	he_ltf = strtoul(argv[1], &ptr, 10);

	if (he_ltf > 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid HE LTF value(%lu).\n",
			      he_ltf);
		shell_help(shell);
		return -ENOEXEC;
	}

	ctx->conf_params.he_ltf = he_ltf;

	return 0;
}


static int nrf_wifi_radio_test_set_he_gi(const struct shell *shell,
					 size_t argc,
					 const char *argv[])
{
	char *ptr = NULL;
	unsigned long he_gi = 0;

	he_gi = strtoul(argv[1], &ptr, 10);

	if (he_gi > 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid HE GI value(%lu).\n",
			      he_gi);
		shell_help(shell);
		return -ENOEXEC;
	}

	ctx->conf_params.he_gi = he_gi;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_tput_mode(const struct shell *shell,
						    size_t argc,
						    const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val >= RPU_TPUT_MODE_MAX) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_tput_mode = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_sgi(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_sgi = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_preamble(const struct shell *shell,
						   size_t argc,
						   const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val >= RPU_PKT_PREAMBLE_MAX) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (ctx->conf_params.tx_pkt_tput_mode == 0) {
		if ((val != RPU_PKT_PREAMBLE_SHORT) &&
		    (val != RPU_PKT_PREAMBLE_LONG)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid value %lu for legacy mode\n",
				      val);
			return -ENOEXEC;
		}
	} else {
		if (val != RPU_PKT_PREAMBLE_MIXED) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Only mixed preamble mode supported in HT, VHT and HE modes\n");
			return -ENOEXEC;
		}
	}

	ctx->conf_params.tx_pkt_preamble = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_mcs(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	long val = -1;

	val = strtol(argv[1], &ptr, 10);

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (!(check_valid_data_rate(shell,
				    ctx->conf_params.tx_pkt_tput_mode,
				    ctx->conf_params.tx_pkt_nss,
				    (val | 0x80)))) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_mcs = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_rate(const struct shell *shell,
					       size_t argc,
					       const char *argv[])
{
	char *ptr = NULL;
	long val = -1;

	val = strtol(argv[1], &ptr, 10);

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (!(check_valid_data_rate(shell,
				    ctx->conf_params.tx_pkt_tput_mode,
				    ctx->conf_params.tx_pkt_nss,
				    val))) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_rate = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_gap(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 200000) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_gap_us = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_pkt_num(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	long val = 0;

	val = strtol(argv[1], &ptr, 10);

	if ((val < -1) || (val == 0)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_num = val;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_len(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val == 0) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_MAX) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Throughput mode not set\n");
		return -ENOEXEC;
	}

	if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_LEGACY) {
		if (val > 4000) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "max 'tx_pkt_len' size for legacy is 4000 bytes\n");
			return -ENOEXEC;
		}
	} else if (ctx->conf_params.tx_pkt_tput_mode == RPU_TPUT_MODE_HE_TB) {
		if (ctx->conf_params.ru_tone == 26) {
			if (val >= 350) {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "'tx_pkt_len' has to be less than 350 bytes\n");
				return -ENOEXEC;
			}
		} else if (ctx->conf_params.ru_tone == 52) {
			if (val >= 800) {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "'tx_pkt_len' has to be less than 800 bytes\n");
				return -ENOEXEC;
			}
		} else if (ctx->conf_params.ru_tone == 106) {
			if (val >= 1800) {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "'tx_pkt_len' has to be less than 1800 bytes\n");
				return -ENOEXEC;
			}
		} else if (ctx->conf_params.ru_tone == 242) {
			if (val >= 4000) {
				shell_fprintf(shell,
					      SHELL_ERROR,
					      "'tx_pkt_len' has to be less than 4000 bytes\n");
				return -ENOEXEC;
			}
		} else {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "'ru_tone' not set\n");
			return -ENOEXEC;
		}
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_len = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_power(const struct shell *shell,
					    size_t argc,
					    const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (((val > MAX_TX_PWR_RADIO_TEST) && (val != MAX_TX_PWR_SYS_TEST))) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid TX power setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_power = val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_tone_freq(const struct shell *shell,
						size_t argc,
						const char *argv[])
{
	char *ptr = NULL;
	signed char val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val > 10) || (val < -10)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'tx_tone_freq' has to be in between -10 to +10\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_tone_freq = val;

	return 0;
}

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_set_tx_tone_type(const struct shell *shell,
						size_t argc,
						const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'tx_tone_type' has to be 0 (complex), 1 (real-only), or 2 (imag-only)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_tone_type = (unsigned char)val;

	return 0;
}


static int nrf_wifi_radio_test_set_tx_tone_dc_offset(const struct shell *shell,
						     size_t argc,
						     const char *argv[])
{
	char *ptr = NULL;
	unsigned long mode = 0;
	long i_val = 0;
	long q_val = 0;

	mode = strtoul(argv[1], &ptr, 10);
	if (mode > 3) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'mode' must be 0 (no offsets), 1 (I only), 2 (Q only), 3 (both)\n");
		return -ENOEXEC;
	}

	i_val = strtol(argv[2], &ptr, 10);
	q_val = strtol(argv[3], &ptr, 10);

	if ((i_val > 2047) || (i_val < -2048) || (q_val > 2047) || (q_val < -2048)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "DC offset values must be in Q.11 range -2048 to 2047\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	if (mode == 0) {
		ctx->conf_params.tx_tone_dc_offset_i = 0;
		ctx->conf_params.tx_tone_dc_offset_q = 0;
	} else if (mode == 1) {
		ctx->conf_params.tx_tone_dc_offset_i = (signed short int)i_val;
		ctx->conf_params.tx_tone_dc_offset_q = 0;
	} else if (mode == 2) {
		ctx->conf_params.tx_tone_dc_offset_i = 0;
		ctx->conf_params.tx_tone_dc_offset_q = (signed short int)q_val;
	} else {
		ctx->conf_params.tx_tone_dc_offset_i = (signed short int)i_val;
		ctx->conf_params.tx_tone_dc_offset_q = (signed short int)q_val;
	}

	return 0;
}

#endif /* WIFI_NRF71 */

static int nrf_wifi_radio_test_set_rx_lna_gain(const struct shell *shell,
					       size_t argc,
					       const char *argv[])
{
	char *ptr = NULL;
	signed char val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val > 4) || (val < 0)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'lna_gain' has to be in between 0 to 4\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.lna_gain = val;

	return 0;
}


static int nrf_wifi_radio_test_set_rx_bb_gain(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	signed char val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val > 31) || (val < 0)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'bb_gain' has to be in between 0 to 31\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.bb_gain = val;

	return 0;
}


static int nrf_wifi_radio_test_set_rx_capture_length(const struct shell *shell,
						     size_t argc,
						     const char *argv[])
{
	char *ptr = NULL;
	unsigned short int val = 0;

	val = strtoul(argv[1], &ptr, 10);

#ifdef WIFI_NRF71
	if ((val > NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES) || (val <= MIN_CAPTURE_LEN)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'capture_length' must be 1..%u "
			      "(NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES)\n",
			      (unsigned int)NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES);
		return -ENOEXEC;
	}
#else
	if ((val > MAX_CAPTURE_LEN) || (val <= MIN_CAPTURE_LEN)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'capture_length' has to be non-zero and less than 16384\n");
		return -ENOEXEC;
	}
#endif

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.capture_length = val;

	return 0;
}

static int nrf_wifi_radio_test_set_rx_capture_timeout(const struct shell *shell, size_t argc,
						      const char *argv[])
{
	char *ptr = NULL;
	unsigned short int val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > CAPTURE_DURATION_IN_SEC) {
#ifdef WIFI_NRF71
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'capture_timeout' must be 0..%u seconds "
			      "(CAPTURE_DURATION_IN_SEC)\n",
			      (unsigned int)CAPTURE_DURATION_IN_SEC);
#else
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'capture_timeout' must be 0..%u seconds\n",
			      (unsigned int)CAPTURE_DURATION_IN_SEC);
#endif
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.capture_timeout = val;

	return 0;
}

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_set_rx_capture_ed_thresh(const struct shell *shell,
							size_t argc,
							const char *argv[])
{
	char *ptr = NULL;
	long ofdm = 0;
	long dsss = 0;

	ofdm = strtol(argv[1], &ptr, 10);
	if (ofdm < -100 || ofdm > 0) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'ed_thresh_ofdm' must be in -100..0\n");
		return -ENOEXEC;
	}

	dsss = strtol(argv[2], &ptr, 10);
	if (dsss < -100 || dsss > 0) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "'ed_thresh_dsss' must be in -100..0\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.ed_thresh_ofdm = (unsigned char)((signed char)ofdm);
	ctx->conf_params.ed_thresh_dsss = (unsigned char)((signed char)dsss);

	return 0;
}
#endif /* WIFI_NRF71 */

static int nrf_wifi_radio_test_set_ru_tone(const struct shell *shell,
					   size_t argc,
					   const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val != 26) &&
	    (val != 52) &&
	    (val != 106) &&
	    (val != 242)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.ru_tone = val;

	return 0;
}


static int nrf_wifi_radio_test_set_ru_index(const struct shell *shell,
					    size_t argc,
					    const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (ctx->conf_params.ru_tone == 26) {
		if ((val < 1) || (val > 9)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid value %lu\n",
				      val);
			shell_help(shell);
			return -ENOEXEC;
		}
	} else if (ctx->conf_params.ru_tone == 52) {
		if ((val < 1) || (val > 4)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid value %lu\n",
				      val);
			shell_help(shell);
			return -ENOEXEC;
		}
	} else if (ctx->conf_params.ru_tone == 106) {
		if ((val < 1) || (val > 2)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid value %lu\n",
				      val);
			shell_help(shell);
			return -ENOEXEC;
		}
	} else if (ctx->conf_params.ru_tone == 242) {
		if ((val != 1)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid value %lu\n",
				      val);
			shell_help(shell);
			return -ENOEXEC;
		}
	} else {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "RU tone not set\n");
		shell_help(shell);
		return -ENOEXEC;
	}


	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.ru_index = val;

	return 0;
}

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_config_vtf_params(const struct shell *shell,
						 size_t argc,
						 const char *argv[])
{
	char *ptr = NULL;
	unsigned long voltage, temp;
	long x0_signed;

	if (argc < 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: config_vtf_params <voltage> <temp> <x0_freq>\n");
		return -ENOEXEC;
	}

#ifndef PHY_RF_PARAM_GDRAM
	shell_fprintf(shell,
		      SHELL_ERROR,
		      "config_vtf_params not available\n");
	return -ENOTSUP;
#else
	voltage = strtoul(argv[1], &ptr, 10);
	temp = strtoul(argv[2], &ptr, 10);
	x0_signed = strtol(argv[3], &ptr, 10);

	if (x0_signed < -128 || x0_signed > 127) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "x0_freq must be -128..127\n");
		return -EINVAL;
	}

	vtf_voltage = (unsigned int)voltage;
	vtf_temp = (unsigned int)temp;
	vtf_x0 = (unsigned int)(signed char)x0_signed;

	shell_fprintf(shell,
		      SHELL_INFO,
		      "VTF params set: voltage=%u temp=%u x0_freq=%ld\n",
		      vtf_voltage,
		      vtf_temp,
		      x0_signed);
	return 0;
#endif
}

#ifdef PHY_RF_PARAM_GDRAM
static int nrf_wifi_radio_test_config_phy_rf_param(const struct shell *shell,
						   size_t argc,
						   const char *argv[])
{
	enum nrf_wifi_status st;
	unsigned long n;
	char *end = NULL;

	if (argc < 3) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: config_phy_rf_param <1..%u> <hex>\n",
			      NUM_WIFI_PARAMS);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	n = strtoul(argv[1], &end, 10);
	if (end == argv[1] || *end != '\0' || n < 1UL || n > (unsigned long)NUM_WIFI_PARAMS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "param no must be 1..%u\n",
			      NUM_WIFI_PARAMS);
		return -ENOEXEC;
	}

	st = nrf_wifi_fmac_set_phy_rf_param_hex((unsigned int)n, argv[2]);
	if (st != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "config_phy_rf_param failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "config_phy_rf_param %lu stored\n", n);
	return 0;
}

static int nrf_wifi_radio_test_config_edge_ceilings(const struct shell *shell,
						     size_t argc,
						     const char *argv[])
{
	enum nrf_wifi_status st;
	size_t hexlen;

	if (argc < 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: config_edge_ceilings <hex> (70 hex chars = 35 bytes, PARAM9)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	hexlen = strlen(argv[1]);
	if (hexlen != 70U) {
		shell_fprintf(shell, SHELL_ERROR, "hex length must be 70\n");
		return -ENOEXEC;
	}

	st = nrf_wifi_fmac_set_phy_rf_param_hex(9U, argv[1]);
	if (st != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "config_edge_ceilings failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "config_edge_ceilings stored\n");
	return 0;
}

static int nrf_wifi_radio_test_config_antenna_gain(const struct shell *shell,
						   size_t argc,
						   const char *argv[])
{
	enum nrf_wifi_status st;
	size_t hexlen;

	if (argc < 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: config_antenna_gain <hex> (10 hex chars = 5 bytes, PARAM10)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	hexlen = strlen(argv[1]);
	if (hexlen != 10U) {
		shell_fprintf(shell, SHELL_ERROR, "hex length must be 10\n");
		return -ENOEXEC;
	}

	st = nrf_wifi_fmac_set_phy_rf_param_hex(10U, argv[1]);
	if (st != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "config_antenna_gain failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "config_antenna_gain stored\n");
	return 0;
}
#endif
#endif

static int nrf_wifi_radio_test_init(const struct shell *shell,
				    size_t argc,
				    const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	//unsigned long val = 0;

	unsigned long band, chan_num = 0;

	band = strtoul(argv[1], &ptr, 10);
	chan_num = strtoul(argv[2], &ptr, 10);

	switch (band) {
	case WIFI_FREQ_BAND_2_4_GHZ:
		if (!check_valid_chan_2g(chan_num)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid channel number %lu on 2G band\n",
				      chan_num);
			return -ENOEXEC;
		}
	break;
#ifndef CONFIG_NRF70_2_4G_ONLY
	case WIFI_FREQ_BAND_5_GHZ:
		if (!check_valid_chan_5g(chan_num)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid channel number %lu on 5G band\n",
				      chan_num);
			return -ENOEXEC;
		}
	break;
	case WIFI_FREQ_BAND_6_GHZ:
		if (!check_valid_chan_6g(chan_num)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid channel number %lu on 5G band\n",
				      chan_num);
			return -ENOEXEC;
		}
	break;
#endif /* CONFIG_NRF70_2_4G_ONLY */
	default:
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid band %lu\n",
			      band);
		return -ENOEXEC;
	}

	if (ctx->conf_params.rx) {
		shell_fprintf(shell,
			      SHELL_INFO,
			      "Disabling ongoing RX test\n");

		ctx->conf_params.rx = 0;

		status = nrf_wifi_rt_fmac_prog_rx(ctx->rpu_ctx,
							  &ctx->conf_params);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Disabling RX failed\n");
			return -ENOEXEC;
		}
	}

	if (ctx->conf_params.tx) {
		shell_fprintf(shell,
			      SHELL_INFO,
			      "Disabling ongoing TX test\n");

		ctx->conf_params.tx = 0;

		status = nrf_wifi_rt_fmac_prog_tx(ctx->rpu_ctx,
							  &ctx->conf_params);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Disabling TX failed\n");
			return -ENOEXEC;
		}
	}

	if (ctx->rf_test_run) {
		if (ctx->rf_test != NRF_WIFI_RF_TEST_TX_TONE) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Unexpected: RF Test (%d) running\n",
				      ctx->rf_test);

			return -ENOEXEC;
		}

		shell_fprintf(shell,
			      SHELL_INFO,
			      "Disabling ongoing TX tone test\n");

		status = nrf_wifi_rt_fmac_rf_test_tx_tone(ctx->rpu_ctx,
						       0,
						       ctx->conf_params.tx_tone_freq,
						       ctx->conf_params.tx_power
#ifdef WIFI_NRF71
						       , ctx->conf_params.tx_tone_type
						       , ctx->conf_params.tx_tone_dc_offset_i
						       , ctx->conf_params.tx_tone_dc_offset_q
#endif
						       );

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Disabling TX tone test failed\n");
			return -ENOEXEC;
		}

		ctx->rf_test_run = false;
		ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	}

	status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Configuration init failed\n");
		return -ENOEXEC;
	}
#ifdef WIFI_NRF71
	ctx->conf_params.chan.op_band = (1 << band);
#else
	ctx->conf_params.chan.op_band = band;
#endif /* WIFI_NRF71 */
	ctx->conf_params.chan.primary_num = chan_num;

	status = nrf_wifi_rt_fmac_radio_test_init(ctx->rpu_ctx,
					       &ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Programming init failed\n");
		return -ENOEXEC;
	}

	return 0;
}

static int nrf_wifi_radio_test_set_tx_pkt_cw(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	long val = 0;

	val = strtol(argv[1], &ptr, 10);

	if (!((val == 0) ||
		  (val == 3) ||
		  (val == 7) ||
		  (val == 15) ||
		  (val == 31) ||
		  (val == 63) ||
		  (val == 127) ||
		  (val == 255) ||
		  (val == 511) ||
		  (val == 1023))) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_cw = val;

	return 0;
}

static int nrf_wifi_radio_test_set_tx(const struct shell *shell,
				      size_t argc,
				      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (val == 1) {
		if (!check_test_in_prog(shell)) {
			return -ENOEXEC;
		}

		if (check_channel_settings(shell,
					   ctx->conf_params.tx_pkt_tput_mode,
					   &ctx->conf_params.chan) != 0) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid channel settings\n");
			return -ENOEXEC;
		}
#ifndef WIFI_NRF71
		/** Max TX power values differ based on the test being performed.
		 * For TX EVM Vs Power, Max TX power required is
		 * "MAX_TX_PWR_RADIO_TEST" (24dB) whereas for testing the
		 * Max TX power for which both EVM and spectrum mask are passing
		 * for specific band and MCS/rate, TX power values will be read from
		 * RF params string.
		 */
		if (ctx->conf_params.tx_power != MAX_TX_PWR_SYS_TEST) {
			/** Max TX power is represented in 0.25dB resolution
			 * So,multiply 4 to MAX_TX_PWR_RADIO_TEST and
			 * configure the RF params corresponding to Max TX power.
			 */
			memset(&ctx->conf_params.rf_params[NRF_WIFI_TX_PWR_CEIL_BYTE_OFFSET],
			(MAX_TX_PWR_RADIO_TEST << 2),
			sizeof(struct nrf_wifi_tx_pwr_ceil));
		}
#endif /* !WIFI_NRF71 */
	}

	ctx->conf_params.tx = val;

	status = nrf_wifi_rt_fmac_prog_tx(ctx->rpu_ctx,
						  &ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Programming TX failed\n");
		return -ENOEXEC;
	}

	return 0;
}


static int nrf_wifi_radio_test_set_rx(const struct shell *shell,
				      size_t argc,
				      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (val == 1) {
		if (!check_test_in_prog(shell)) {
			return -ENOEXEC;
		}
	}

	ctx->conf_params.rx = val;

	status = nrf_wifi_rt_fmac_prog_rx(ctx->rpu_ctx,
						  &ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Programming RX failed\n");
		return -ENOEXEC;
	}

	return 0;
}

#ifdef CONFIG_NRF70_SR_COEX_RF_SWITCH
static int nrf_wifi_radio_test_sr_ant_switch_ctrl(const struct shell *shell,
					     size_t argc,
					     const char *argv[])
{
	unsigned int val;

	if (argc < 2) {
		shell_fprintf(shell, SHELL_ERROR, "invalid # of args : %d\n", argc);
		return -ENOEXEC;
	}

	val  = strtoul(argv[1], NULL, 0);

	ctx->conf_params.sr_ant_switch_ctrl = val;

	return sr_ant_switch(val);
}
#endif /* CONFIG_NRF70_SR_COEX_RF_SWITCH */

#ifdef CONFIG_NRF70_SR_COEX
static int nrf_wifi_radio_test_config_pta(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	bool wlan_band;
	bool separate_antennas;
	bool is_sr_protocol_ble;
	int result_non_pta = 0;
	int result_pta = 0;
	int result = 0;

	if (argc < 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "invalid # of args : %d\n", argc);
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: config_pta wlan_band");
		shell_fprintf(shell,
			      SHELL_ERROR,
			      " separate_antennas is_sr_protocol_ble\n");
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "wlan_band: 0 for 2.4GHz, 1 for 5GHz\n");
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "is_sep_antennas: 0 for shared antenna,");
		shell_fprintf(shell,
			      SHELL_ERROR,
			      " 1 for separate antennas\n");
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "is_sr_ble: 0 for Thread, 1 for Bluetooth\n");
		return -ENOEXEC;
	}

	wlan_band  = strtoul(argv[1], NULL, 0);
	separate_antennas  = strtoul(argv[2], NULL, 0);
	is_sr_protocol_ble  = strtoul(argv[3], NULL, 0);

	if ((int)wlan_band > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid wlan_band(%d).\n",
			      (int)wlan_band);
		shell_help(shell);
		return -ENOEXEC;
	}

	if ((int)separate_antennas > 1) {
		shell_fprintf(shell,
			     SHELL_ERROR,
			    "Invalid separate_antennas(%d).\n",
			    (int)separate_antennas);
		shell_help(shell);
		return -ENOEXEC;
	}

	if ((int)is_sr_protocol_ble > 1) {
		shell_fprintf(shell,
			     SHELL_ERROR,
			    "Invalid is_sr_protocol_ble(%d).\n",
			    (int)is_sr_protocol_ble);
		shell_help(shell);
		return -ENOEXEC;
	}

	result_non_pta = nrf_wifi_coex_config_non_pta(separate_antennas, is_sr_protocol_ble);
	result_pta = nrf_wifi_coex_config_pta(wlan_band, separate_antennas, is_sr_protocol_ble);
	result = result_non_pta & result_pta;
	return result;
}
#endif /* CONFIG_NRF70_SR_COEX */

static int nrf_wifi_radio_test_rx_cap(const struct shell *shell,
				      size_t argc,
				      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned long rx_cap_type = 0;
	unsigned char *rx_cap_buf = NULL;
	unsigned char capture_status = 0;
	char *ptr = NULL;
	unsigned int i = 0;
	int ret = -ENOEXEC;

	rx_cap_type = strtoul(argv[1], &ptr, 10);

	if ((rx_cap_type !=  NRF_WIFI_RF_TEST_RX_ADC_CAP) &&
	    (rx_cap_type != NRF_WIFI_RF_TEST_RX_STAT_PKT_CAP) &&
	    (rx_cap_type != NRF_WIFI_RF_TEST_RX_DYN_PKT_CAP)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      rx_cap_type);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!ctx->conf_params.capture_length) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "%s: Invalid rx_capture_length %d\n",
			      __func__,
			      ctx->conf_params.capture_length);
		goto out;
	}

	if (rx_cap_type == NRF_WIFI_RF_TEST_RX_DYN_PKT_CAP) {
		if (!ctx->conf_params.capture_timeout) {
			shell_fprintf(shell,
				SHELL_ERROR,
				"%s: Invalid rx_capture_timeout %d\n",
				__func__,
				ctx->conf_params.capture_timeout);
			goto out;
		}
	}

	if (!check_test_in_prog(shell)) {
		goto out;
	}

#ifdef WIFI_NRF71

	rx_cap_buf = nrf_wifi_osal_mem_zalloc((size_t)ctx->conf_params.capture_length * 4U);
#else
	rx_cap_buf = nrf_wifi_osal_mem_zalloc((size_t)ctx->conf_params.capture_length * 3U);
#endif

	if (!rx_cap_buf) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "%s: Unable to allocate (%u) bytes for RX capture\n",
			      __func__,
#ifdef WIFI_NRF71
			      (unsigned int)((size_t)ctx->conf_params.capture_length * 4U)
#else
			      (unsigned int)((size_t)ctx->conf_params.capture_length * 3U)
#endif
			      );
		goto out;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = (enum nrf_wifi_rf_test)rx_cap_type;

	shell_fprintf(shell,
		      SHELL_INFO,
		      "RX capture RPU staging address: 0x%08X\n",
		      (unsigned int)RPU_MEM_RF_TEST_CAP_BASE);

	status = nrf_wifi_rt_fmac_rf_test_rx_cap(ctx->rpu_ctx,
					      rx_cap_type,
					      rx_cap_buf,
					      ctx->conf_params.capture_length,
					      ctx->conf_params.capture_timeout,
					      ctx->conf_params.lna_gain,
					      ctx->conf_params.bb_gain,
#ifdef WIFI_NRF71
					      ctx->conf_params.ed_thresh_ofdm,
					      ctx->conf_params.ed_thresh_dsss,
#endif
					      &capture_status);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "RX ADC capture programming failed\n");
		goto out;
	}

	if (capture_status == 0) {
		shell_fprintf(shell,
			      SHELL_INFO,
			      "\n************* RX capture data ***********\n");

#ifdef WIFI_NRF71

		for (i = 0; i < (ctx->conf_params.capture_length); i++) {
			unsigned char *p = &rx_cap_buf[i * 4];

			shell_fprintf(shell,
				      SHELL_INFO,
				      "%02X%02X%02X%02X\n",
				      p[3], p[2], p[1], p[0]);
		}
#else
		for (i = 0; i < (ctx->conf_params.capture_length); i++) {
			shell_fprintf(shell,
				      SHELL_INFO,
				      "%02X%02X%02X\n",
				      rx_cap_buf[i*3 + 2],
				      rx_cap_buf[i*3 + 1],
				      rx_cap_buf[i*3 + 0]);
		}
#endif
	} else if (capture_status == 1) {
		shell_fprintf(shell,
			      SHELL_INFO,
			      "\n************* Capture failed ***********\n");
	} else {
		shell_fprintf(shell,
			      SHELL_INFO,
			      "\n************* Packet detection failed ***********\n");
	}

	ret = 0;
out:
	if (rx_cap_buf)
		nrf_wifi_osal_mem_free(rx_cap_buf);

	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}


static int nrf_wifi_radio_test_tx_tone(const struct shell *shell,
				       size_t argc,
				       const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;
	int ret = -ENOEXEC;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		goto out;

	}

	if (val == 1) {
		if (!check_test_in_prog(shell)) {
			goto out;
		}

	}

	status = nrf_wifi_rt_fmac_rf_test_tx_tone(ctx->rpu_ctx,
					       (unsigned char)val,
					       ctx->conf_params.tx_tone_freq,
					       ctx->conf_params.tx_power
#ifdef WIFI_NRF71
					       , ctx->conf_params.tx_tone_type
					       , ctx->conf_params.tx_tone_dc_offset_i
					       , ctx->conf_params.tx_tone_dc_offset_q
#endif
					       );

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "TX tone programming failed\n");
		goto out;
	}

	if (val == 1) {
		ctx->rf_test_run = true;
		ctx->rf_test = NRF_WIFI_RF_TEST_TX_TONE;
	} else {
		ctx->rf_test_run = false;
		ctx->rf_test = NRF_WIFI_RF_TEST_MAX;
	}

	ret = 0;
out:
	return ret;
}


static int nrf_wifi_radio_set_dpd(const struct shell *shell,
				  size_t argc,
				  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;
	int ret = -ENOEXEC;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		goto out;
	}

	if (val == 1) {
		if (!check_test_in_prog(shell)) {
			goto out;
		}
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_DPD;

	status = nrf_wifi_rt_fmac_rf_test_dpd(ctx->rpu_ctx,
					   val);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "DPD programming failed\n");
		goto out;
	}

	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}

static int nrf_wifi_radio_get_temperature(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	int ret = -ENOEXEC;

	if (!check_test_in_prog(shell)) {
		goto out;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_GET_TEMPERATURE;

	status = nrf_wifi_rt_fmac_rf_get_temp(ctx->rpu_ctx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Temperature read failed\n");
		goto out;
	}

	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}

static int nrf_wifi_radio_get_bat_volt(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	int ret = -ENOEXEC;

	if (!check_test_in_prog(shell)) {
		goto out;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_GET_BAT_VOLT;

	status = nrf_wifi_rt_fmac_rf_get_bat_volt(ctx->rpu_ctx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Voltage read failed\n");
		goto out;
	}

	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}


static int nrf_wifi_radio_get_rf_rssi(const struct shell *shell,
				      size_t argc,
				      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	int ret = -ENOEXEC;

	if (!check_test_in_prog(shell)) {
		goto out;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_RF_RSSI;

	status = nrf_wifi_rt_fmac_rf_get_rf_rssi(ctx->rpu_ctx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "RF RSSI get failed\n");
		goto out;
	}

	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}


static int nrf_wifi_radio_set_xo_val(const struct shell *shell,
				     size_t argc,
				     const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	long val = 0;
	int ret = -ENOEXEC;

	val = strtol(argv[1], &ptr, 10);

#ifdef WIFI_NRF71
	if (val > 100 || val < -100) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "XO value must be in the range -100 to 100 (signed PPM)\n");
		shell_help(shell);
		goto out;
	}
#else
	if (val > 0x7f) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value XO value should be <= 0x7f\n");
		shell_help(shell);
		goto out;
	}

	if (val < 0) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value XO value should be >= 0\n");
		shell_help(shell);
		goto out;
	}
#endif

	if (val == 1) {
		if (!check_test_in_prog(shell)) {
			goto out;
		}
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_XO_CALIB;

	status = nrf_wifi_rt_fmac_set_xo_val(ctx->rpu_ctx,
#ifdef WIFI_NRF71
					       (signed char)val);
#else
					       (unsigned char)val);
#endif

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "XO value programming failed\n");
		goto out;
	}
#ifndef WIFI_NRF71
	ctx->conf_params.rf_params[NRF_WIFI_XO_FREQ_BYTE_OFFSET] = (unsigned char)val;
#endif /* !WIFI_NRF71 */
	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}

static int nrf_wifi_radio_comp_opt_xo_val(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	int ret = -ENOEXEC;

	if (!check_test_in_prog(shell)) {
		goto out;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_XO_TUNE;

	status = nrf_wifi_rt_fmac_rf_test_compute_xo(ctx->rpu_ctx);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "XO value computation failed\n");
		goto out;
	}

	ret = 0;
out:
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return ret;
}

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_perform_rf_calibration(const struct shell *shell,
						     size_t argc,
						     const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_calib calib_params;
	char *ptr = NULL;
	unsigned long calib_bitmap = 0;
	unsigned long sys_operating_mode = 0;
	unsigned long result_index = 0;
	int ret = -ENOEXEC;

	if (argc < 3) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: perform_rf_calibration <calib_bitmap> <sys_operating_mode> <result_index>\n");
		shell_fprintf(shell,
			      SHELL_INFO,
			      "  calib_bitmap: hex (e.g. 0x7F) or decimal; sys_operating_mode: 0=rx_only 1=trx_normal; result_index: 0 or 1 (default 0)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	calib_bitmap = strtoul(argv[1], &ptr, 0);
	if (calib_bitmap > 0xFF) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "calib_bitmap must be 0-0xFF\n");
		return -ENOEXEC;
	}

	sys_operating_mode = strtoul(argv[2], &ptr, 10);
	if (sys_operating_mode > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "sys_operating_mode must be 0 (rx_only) or 1 (trx_normal)\n");
		return -ENOEXEC;
	}

	if (argc >= 4) {
		result_index = strtoul(argv[3], &ptr, 10);
		if (result_index > 1) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "result_index must be 0 or 1\n");
			return -ENOEXEC;
		}
	}

	memset(&calib_params, 0, sizeof(calib_params));
	calib_params.calib_bitmap = (unsigned char)calib_bitmap;
	calib_params.sys_operating_mode = (unsigned char)sys_operating_mode;
	calib_params.index = (unsigned char)result_index;
	calib_params.rf_calib_results = NULL;

	status = nrf_wifi_rt_fmac_rf_test_perform_calib(ctx->rpu_ctx, &calib_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "perform_rf_calibration failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "RF calibration completed (bitmap=0x%02x mode=%lu result_index=%lu)\n",
		      (unsigned int)calib_bitmap,
		      sys_operating_mode,
		      result_index);
	ret = 0;
	return ret;
}

static unsigned char rf_calib_result_buf[CAL_MEM_SIZE];

static int nrf_wifi_radio_test_read_rf_comp_results(const struct shell *shell,
						   size_t argc,
						   const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_read_calib_results params;
	char *ptr = NULL;
	unsigned long mode = 0;
	unsigned long result_index = 0;

	if (argc < 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: read_rf_comp_results <mode> [result_index]\n");
		shell_fprintf(shell,
			      SHELL_INFO,
			      "  mode: 0=operating_channel 1=scan_channel; result_index: 0 or 1 (default 0)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	mode = strtoul(argv[1], &ptr, 10);
	if (mode > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "mode must be 0 (operating) or 1 (scan)\n");
		return -ENOEXEC;
	}

	if (argc >= 3) {
		result_index = strtoul(argv[2], &ptr, 10);
		if (result_index > 1) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "result_index must be 0 or 1\n");
			return -ENOEXEC;
		}
	}

	memset(&params, 0, sizeof(params));
	params.mode = (unsigned char)mode;
	params.index = (unsigned char)result_index;
	params.rf_calib_results = rf_calib_result_buf;

	status = nrf_wifi_rt_fmac_rf_test_read_comp_results(ctx->rpu_ctx, &params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "read_rf_comp_results failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "RF read comp results completed (mode=%lu result_index=%lu)\n",
		      mode, result_index);
	return 0;
}

static int nrf_wifi_radio_test_apply_rf_compensation(const struct shell *shell,
						     size_t argc,
						     const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_calib calib_params;
	char *ptr = NULL;
	unsigned long result_index = 0;

	if (argc < 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: apply_rf_compensation <result_index>\n");
		shell_fprintf(shell,
			      SHELL_INFO,
			      "  result_index: 0 or 1 (slot). Uses in-memory result buffer (%d bytes).\n",
			      (int)CAL_MEM_SIZE);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	result_index = strtoul(argv[1], &ptr, 10);
	if (result_index > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "result_index must be 0 or 1\n");
		return -ENOEXEC;
	}

	memset(&calib_params, 0, sizeof(calib_params));
	calib_params.index = (unsigned char)result_index;
	calib_params.rf_calib_results = NULL;

	status = nrf_wifi_rt_fmac_rf_test_apply_compensation(ctx->rpu_ctx,
							    &calib_params,
							    rf_calib_result_buf);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "apply_rf_compensation failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "RF apply compensation completed (result_index=%lu)\n",
		      result_index);
	return 0;
}

static int nrf_wifi_radio_test_set_calib_regs(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_set_cal_regs calib_params;
	char *ptr = NULL;
	unsigned long cal_id = 0;
	unsigned long num_regs = 0;
	unsigned long addr_val;
	size_t i;
	int ret = -ENOEXEC;

	/* set_calib_regs <cal_id> <num_regs> <addr0> <val0> [addr1 val1 ...] */
	if (argc < 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: set_calib_regs <cal_id> <num_regs> <addr0> <val0> [addr1 val1 ...]\n");
		shell_fprintf(shell,
			      SHELL_INFO,
			      "  cal_id: 0=RX_DC_CAL 1=TX_DC_CAL 2=RX_IQ_CAL 3=TX_IQ_CAL 4=DPD_CAL; num_regs: 1..8\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	cal_id = strtoul(argv[1], &ptr, 0);
	if (cal_id > 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "cal_id must be 0..4\n");
		return -ENOEXEC;
	}

	num_regs = strtoul(argv[2], &ptr, 0);
	if (num_regs == 0 || num_regs > MAX_REGS_CONF) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "num_regs must be 1..%d\n",
			      MAX_REGS_CONF);
		return -ENOEXEC;
	}

	if (argc < (size_t)(4 + num_regs * 2)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Need %lu addr/val pairs (got %d args)\n",
			      (unsigned long)num_regs, (int)argc);
		return -ENOEXEC;
	}

	memset(&calib_params, 0, sizeof(calib_params));
	calib_params.cal_id = (unsigned char)cal_id;
	calib_params.num_regs = (unsigned char)num_regs;
	for (i = 0; i < num_regs; i++) {
		addr_val = strtoul(argv[3 + i * 2], &ptr, 0);
		calib_params.reg_addr[i] = (unsigned int)addr_val;
		addr_val = strtoul(argv[4 + i * 2], &ptr, 0);
		calib_params.reg_val[i] = (unsigned int)addr_val;
	}

	status = nrf_wifi_rt_fmac_rf_test_set_calib_regs(ctx->rpu_ctx, &calib_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "set_calib_regs failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "set_calib_regs cal_id=%lu num_regs=%lu done\n",
		      cal_id, num_regs);
	return 0;
}

static int nrf_wifi_radio_test_enable_vt_calib(const struct shell *shell,
	size_t argc,
	const char *argv[])
{
enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
char *ptr = NULL;
unsigned long val = 0;
int ret = -ENOEXEC;

if (argc < 2) {
shell_fprintf(shell, SHELL_ERROR, "Usage: enable_vt_calib <0|1>\n");
return -ENOEXEC;
}

val = strtoul(argv[1], &ptr, 10);
if (val > 1) {
shell_fprintf(shell, SHELL_ERROR, "Invalid value %lu (use 0 or 1)\n", val);
return -ENOEXEC;
}

if (!check_test_in_prog(shell)) {
return -ENOEXEC;
}

status = nrf_wifi_rt_fmac_rf_test_enable_vt_calibration(ctx->rpu_ctx, (unsigned char)val);
if (status != NRF_WIFI_STATUS_SUCCESS) {
shell_fprintf(shell, SHELL_ERROR, "enable_vt_calib failed\n");
return -ENOEXEC;
}

shell_fprintf(shell, SHELL_INFO, "VT calibration %s\n", val ? "enabled" : "disabled");
return 0;
}

static int nrf_wifi_radio_test_enable_vt_comp(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;

	if (argc < 2) {
		shell_fprintf(shell, SHELL_ERROR, "Usage: enable_vt_comp <0|1>\n");
		return -ENOEXEC;
	}

	val = strtoul(argv[1], &ptr, 10);
	if (val > 1) {
		shell_fprintf(shell, SHELL_ERROR, "Invalid value %lu (use 0 or 1)\n", val);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	status = nrf_wifi_rt_fmac_rf_test_enable_vt_compensation(ctx->rpu_ctx, (unsigned char)val);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "enable_vt_comp failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "VT compensation %s\n", val ? "enabled" : "disabled");
	return 0;
}

static int nrf_wifi_radio_test_set_reg(const struct shell *shell,
				       size_t argc,
				       const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_config_regs config_regs;
	char *ptr = NULL;
	unsigned long num_regs;
	size_t i;

	/* set_reg <addr1> <val1> [addr2 val2 ...] - max 8 pairs */
	if (argc < 3 || (argc - 1) % 2 != 0) {
		shell_fprintf(shell, SHELL_ERROR,
			      "Usage: set_reg <addr1> <val1> [addr2 val2 ...] (1..%d pairs)\n",
			      MAX_REGS_CONF);
		return -ENOEXEC;
	}

	num_regs = (argc - 1) / 2;
	if (num_regs > MAX_REGS_CONF) {
		shell_fprintf(shell, SHELL_ERROR, "Max %d regs\n", MAX_REGS_CONF);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&config_regs, 0, sizeof(config_regs));
	config_regs.num_regs = (unsigned char)num_regs;
	for (i = 0; i < num_regs; i++) {
		config_regs.reg_addr[i] = (unsigned int)strtoul(argv[1 + i * 2], &ptr, 0);
		config_regs.reg_val[i] = (unsigned int)strtoul(argv[2 + i * 2], &ptr, 0);
	}

	status = nrf_wifi_rt_fmac_rf_test_set_regs(ctx->rpu_ctx, &config_regs);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "set_reg failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "set_reg %lu regs done\n", (unsigned long)num_regs);
	return 0;
}

static int nrf_wifi_radio_test_read_reg(const struct shell *shell,
					size_t argc,
					const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_config_regs config_regs;
	char *ptr = NULL;
	size_t i;

	/* read_reg <addr1> [addr2 ...] - max 8 addrs */
	if (argc < 2) {
		shell_fprintf(shell, SHELL_ERROR,
			      "Usage: read_reg <addr1> [addr2 ...] (1..%d addrs)\n",
			      MAX_REGS_CONF);
		return -ENOEXEC;
	}

	if ((size_t)(argc - 1) > MAX_REGS_CONF) {
		shell_fprintf(shell, SHELL_ERROR, "Max %d regs\n", MAX_REGS_CONF);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&config_regs, 0, sizeof(config_regs));
	config_regs.num_regs = (unsigned char)(argc - 1);
	for (i = 0; i < (size_t)config_regs.num_regs; i++) {
		config_regs.reg_addr[i] = (unsigned int)strtoul(argv[1 + i], &ptr, 0);
	}

	status = nrf_wifi_rt_fmac_rf_test_read_regs(ctx->rpu_ctx, &config_regs);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "read_reg failed\n");
		return -ENOEXEC;
	}

	for (i = 0; i < (size_t)config_regs.num_regs; i++) {
		shell_fprintf(shell, SHELL_INFO, "0x%x = 0x%x\n",
			      config_regs.reg_addr[i], config_regs.reg_val[i]);
	}
	return 0;
}

static int nrf_wifi_radio_test_set_memory(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_config_mem config_mem;
	char *ptr = NULL;
	unsigned long num_loc;
	size_t i;

	/* set_memory <addr1> <val1> [addr2 val2 ...] - max 8 pairs */
	if (argc < 3 || (argc - 1) % 2 != 0) {
		shell_fprintf(shell, SHELL_ERROR,
			      "Usage: set_memory <addr1> <val1> [addr2 val2 ...] (1..%d pairs)\n",
			      MAX_MEM_CONF);
		return -ENOEXEC;
	}

	num_loc = (argc - 1) / 2;
	if (num_loc > MAX_MEM_CONF) {
		shell_fprintf(shell, SHELL_ERROR, "Max %d memory locations\n", MAX_MEM_CONF);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&config_mem, 0, sizeof(config_mem));
	config_mem.num_memory_loc = (unsigned char)num_loc;
	for (i = 0; i < num_loc; i++) {
		config_mem.mem_addr[i] = (unsigned int)strtoul(argv[1 + i * 2], &ptr, 0);
		config_mem.mem_val[i] = (unsigned int)strtoul(argv[2 + i * 2], &ptr, 0);
	}

	status = nrf_wifi_rt_fmac_rf_test_set_mem(ctx->rpu_ctx, &config_mem);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "set_memory failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell, SHELL_INFO, "set_memory %lu locations done\n", (unsigned long)num_loc);
	return 0;
}

static int nrf_wifi_radio_test_read_memory(const struct shell *shell,
					   size_t argc,
					   const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_config_mem config_mem;
	char *ptr = NULL;
	size_t i;

	/* read_memory <addr1> [addr2 ...] - max 8 addrs */
	if (argc < 2) {
		shell_fprintf(shell, SHELL_ERROR,
			      "Usage: read_memory <addr1> [addr2 ...] (1..%d addrs)\n",
			      MAX_MEM_CONF);
		return -ENOEXEC;
	}

	if ((size_t)(argc - 1) > MAX_MEM_CONF) {
		shell_fprintf(shell, SHELL_ERROR, "Max %d memory locations\n", MAX_MEM_CONF);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&config_mem, 0, sizeof(config_mem));
	config_mem.num_memory_loc = (unsigned char)(argc - 1);
	for (i = 0; i < (size_t)config_mem.num_memory_loc; i++) {
		config_mem.mem_addr[i] = (unsigned int)strtoul(argv[1 + i], &ptr, 0);
	}

	status = nrf_wifi_rt_fmac_rf_test_read_mem(ctx->rpu_ctx, &config_mem);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "read_memory failed\n");
		return -ENOEXEC;
	}

	for (i = 0; i < (size_t)config_mem.num_memory_loc; i++) {
		shell_fprintf(shell, SHELL_INFO, "0x%x = 0x%x\n",
			      config_mem.mem_addr[i], config_mem.mem_val[i]);
	}
	return 0;
}

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_adpll_cap(const struct shell *shell,
					 size_t argc,
					 const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned long enabled = 0;
	unsigned long enable_tracing = 0;
	unsigned long cap_len = 0;
	unsigned char *cap_buf = NULL;
	unsigned char capture_status = 0;
	char *ptr = NULL;
	unsigned int i = 0;
	int ret = -ENOEXEC;

	if (argc != 4) {
		shell_fprintf(shell, SHELL_ERROR,
			      "Usage: adpll_cap <enabled> <enable_tracing> <cap_len>\n"
			      "  enabled/tracing: 0|1; cap_len: 1..%u\n",
			      (unsigned int)NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES);
		return -ENOEXEC;
	}

	enabled = strtoul(argv[1], &ptr, 10);
	enable_tracing = strtoul(argv[2], &ptr, 10);
	cap_len = strtoul(argv[3], &ptr, 10);

	if (enabled > 1 || enable_tracing > 1) {
		shell_fprintf(shell, SHELL_ERROR, "enabled and enable_tracing must be 0 or 1\n");
		return -ENOEXEC;
	}
	if (cap_len == 0 || cap_len > NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES) {
		shell_fprintf(shell, SHELL_ERROR, "cap_len must be 1..%u\n",
			      (unsigned int)NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	cap_buf = nrf_wifi_osal_mem_zalloc((size_t)cap_len * 4U);
	if (!cap_buf) {
		shell_fprintf(shell, SHELL_ERROR, "Unable to allocate ADPLL capture buffer\n");
		return -ENOEXEC;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RF_TEST_ADPLL_CAP_NORMAL;

	shell_fprintf(shell, SHELL_INFO,
		      "ADPLL capture RPU staging address: 0x%08X\n",
		      (unsigned int)RPU_MEM_RF_TEST_CAP_BASE);

	status = nrf_wifi_rt_fmac_rf_test_adpll_cap(ctx->rpu_ctx,
						    cap_buf,
						    (unsigned short)cap_len,
						    (unsigned char)enabled,
						    (unsigned char)enable_tracing,
						    &capture_status);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell, SHELL_ERROR, "adpll_cap failed\n");
		goto out;
	}

	if (capture_status == 0) {
		shell_fprintf(shell, SHELL_INFO,
			      "\n************* ADPLL capture data ***********\n");
		for (i = 0; i < (unsigned int)cap_len; i++) {
			unsigned char *p = &cap_buf[i * 4U];

			shell_fprintf(shell, SHELL_INFO,
				      "%02X%02X%02X%02X\n",
				      p[3], p[2], p[1], p[0]);
		}
	} else {
		shell_fprintf(shell, SHELL_ERROR,
			      "ADPLL capture failed (status=%u)\n",
			      (unsigned int)capture_status);
	}
	ret = 0;
out:
	nrf_wifi_osal_mem_free(cap_buf);
	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;
	return ret;
}
#endif /* WIFI_NRF71 */

static int nrf_wifi_radio_test_rf_patch_settings(const struct shell *shell,
						size_t argc,
						const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_patch_settings params;
	char *ptr = NULL;
	unsigned long patch_type = 0;
	unsigned long index_val = 0;
	unsigned long slice_val = 0;
	unsigned long value_val = 0;
	unsigned long band_val = 0;
	unsigned long is_new = 0;

	/* rf_patch_settings <patch_type> <index> <slice> <value> [band] [is_new_setting] */
	if (argc < 5) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: rf_patch_settings <patch_type> <index> <slice> <value> [band] [is_new_setting]\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	patch_type = strtoul(argv[1], &ptr, 0);
	if (patch_type > 11) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "patch_type must be 0..11\n");
		return -ENOEXEC;
	}

	index_val = strtoul(argv[2], &ptr, 0);
	slice_val = strtoul(argv[3], &ptr, 0);
	value_val = strtoul(argv[4], &ptr, 0);
	band_val = (argc >= 6) ? strtoul(argv[5], &ptr, 0) : 0;
	is_new = (argc >= 7) ? strtoul(argv[6], &ptr, 0) : 0;

	if (band_val > 1 || is_new > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "band and is_new_setting must be 0 or 1\n");
		return -ENOEXEC;
	}

	memset(&params, 0, sizeof(params));
	params.patch_type = (unsigned char)patch_type;
	params.index = (unsigned char)index_val;
	params.slice = (unsigned char)slice_val;
	params.value = (unsigned int)value_val;
	params.band = (unsigned char)band_val;
	params.is_new_setting = (unsigned char)is_new;

	status = nrf_wifi_rt_fmac_rf_test_patch_settings(ctx->rpu_ctx, &params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "rf_patch_settings failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rf_patch_settings patch_type=%lu done\n",
		      patch_type);
	return 0;
}

static int nrf_wifi_radio_test_phy_debug_stats(const struct shell *shell,
					       size_t argc,
					       const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rf_get_rx_debug_stats stats;

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&stats, 0, sizeof(stats));
	status = nrf_wifi_rt_fmac_rf_test_phy_debug_stats(ctx->rpu_ctx, &stats);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "phy_debug_stats failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "************* PHY DEBUG STATS ***********\n");
	shell_fprintf(shell, SHELL_INFO, "ed_cnt=%u\n", stats.edCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_crc32_pass_cnt=%u\n", stats.ofdmCrc32PassCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_crc32_fail_cnt=%u\n", stats.ofdmCrc32FailCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_crc32_pass_cnt=%u\n", stats.dsssCrc32PassCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_crc32_fail_cnt=%u\n", stats.dsssCrc32FailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_corr_pass_cnt=%u\n", stats.ofdmCorrPassCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_corr_pass_cnt=%u\n", stats.dsssCorrPassCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_ltf_corr_fail_cnt=%u\n", stats.ofdmLtfCorrFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_lsig_fail_cnt=%u\n", stats.ofdmLsigFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_htsig_a_fail_cnt=%u\n", stats.ofdmHtsigAFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_vhtsig_a_fail_cnt=%u\n", stats.ofdmVhtsigAFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_vhtsig_b_fail_cnt=%u\n", stats.ofdmVhtsigBFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_hesig_a_fail_cnt=%u\n", stats.ofdmHesigAFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_hesig_b_fail_cnt=%u\n", stats.ofdmHesigBFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_usig_fail_cnt=%u\n", stats.ofdmUsigFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_ehtsig_fail_cnt=%u\n", stats.ofdmEhtsigFailCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_zero_len_mpdu_cnt=%u\n", stats.ofdmzeroLenMpduCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_invalid_delimiter_cnt=%u\n", stats.ofdminvalidDelimiterCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_sync_fail_cnt=%u\n", stats.dsssSyncFailCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_fsync_fail_cnt=%u\n", stats.dsssFsyncFailCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_sfd_fail_cnt=%u\n", stats.dsssSfdFailCnt);
	shell_fprintf(shell, SHELL_INFO, "dsss_hdr_fail_cnt=%u\n", stats.dsssHdrFailCnt);
	shell_fprintf(shell, SHELL_INFO, "lg_pkt_cnt=%u\n", stats.lgPktCnt);
	shell_fprintf(shell, SHELL_INFO, "ht_pkt_cnt=%u\n", stats.htPktCnt);
	shell_fprintf(shell, SHELL_INFO, "vht_pkt_cnt=%u\n", stats.vhtPktCnt);
	shell_fprintf(shell, SHELL_INFO, "he_su_pkt_cnt=%u\n", stats.heSuPktCnt);
	shell_fprintf(shell, SHELL_INFO, "he_mu_pkt_cnt=%u\n", stats.heMuPktCnt);
	shell_fprintf(shell, SHELL_INFO, "he_er_su_pkt_cnt=%u\n", stats.heErSuPktCnt);
	shell_fprintf(shell, SHELL_INFO, "he_tb_pkt_cnt=%u\n", stats.heTbPktCnt);
	shell_fprintf(shell, SHELL_INFO, "eht_mu_pkt_cnt=%u\n", stats.ehtMuPktCnt);
	shell_fprintf(shell, SHELL_INFO, "pop_cnt=%u\n", stats.popCnt);
	shell_fprintf(shell, SHELL_INFO, "mid_packet_cnt=%u\n", stats.midPacketCnt);
	shell_fprintf(shell, SHELL_INFO, "low_energy_event_cnt=%u\n", stats.lowEnergyEventCnt);
	shell_fprintf(shell, SHELL_INFO, "un_supported_cnt=%u\n", stats.unSupportedCnt);
	shell_fprintf(shell, SHELL_INFO, "other_sta_pkt_cnt=%u\n", stats.otherStaPktCnt);
	shell_fprintf(shell, SHELL_INFO, "vht_ndp_cnt=%u\n", stats.vhtNdpCnt);
	shell_fprintf(shell, SHELL_INFO, "hesu_ndp_cnt=%u\n", stats.hesuNdpCnt);
	shell_fprintf(shell, SHELL_INFO, "eht_ndp_cnt=%u\n", stats.ehtNdpCnt);
	shell_fprintf(shell, SHELL_INFO, "ofdm_s2l_timeout_fail_cnt=%u\n", stats.ofdmS2lTimeOutFailCnt);
	shell_fprintf(shell, SHELL_INFO, "spatial_reuse_cnt=%u\n", stats.spatialReuseCnt);
	return 0;
}

static int nrf_wifi_radio_test_rh_oneshot(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_rh_test_params rh_params;
	char *ptr = NULL;
	unsigned long period = 0;
	long range_start = 0;
	long range_end = 0;

	if (argc < 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Usage: rh_oneshot <hist_type> <stat_type> <period> [<range_start> <range_end>]\n"
			      "  hist_type: unconditional | pkt_only | noise_only\n"
			      "  stat_type: all | max | range (range requires start/end dBm)\n"
			      "  period: 0-10 seconds\n");
		return -ENOEXEC;
	}
	if ((strcmp(argv[1], "unconditional") != 0) && (strcmp(argv[1], "pkt_only") != 0) &&
	    (strcmp(argv[1], "noise_only") != 0)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid hist_type. Use 'unconditional', 'pkt_only' or 'noise_only'\n");
		return -ENOEXEC;
	}
	if ((strcmp(argv[2], "all") != 0) && (strcmp(argv[2], "max") != 0) &&
	    (strcmp(argv[2], "range") != 0)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid stat_type. Use 'all', 'max' or 'range'\n");
		return -ENOEXEC;
	}

	period = strtoul(argv[3], &ptr, 10);

	if (period > 10) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid time period. value in seconds with max duration of 10 seconds\n");
		return -ENOEXEC;
	}
	if ((strcmp(argv[2], "range") == 0) && (argc < 6)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Range start and end values must be provided when stat_type is 'range'\n");
		return -ENOEXEC;
	}
	if (strcmp(argv[2], "range") == 0) {
		shell_fprintf(shell,
			      SHELL_INFO,
			      " command: %s %s %s %s %s\n",
			      argv[1],
			      argv[2],
			      argv[3],
			      argv[4],
			      argv[5]);
		range_start = strtol(argv[4], &ptr, 10);
		range_end = strtol(argv[5], &ptr, 10);
		if ((range_start < -100) || (range_start > 0) || (range_end < -100) || (range_end > 0) ||
		    (range_start >= range_end)) {
			shell_fprintf(shell,
				      SHELL_ERROR,
				      "Invalid range values. Range start and end should be in dBm with valid range from -100 to 0 and start value should be less than end value\n");
			return -ENOEXEC;
		}
	} else {
		shell_fprintf(shell,
			      SHELL_INFO,
			      " command: %s %s %s\n",
			      argv[1],
			      argv[2],
			      argv[3]);
	}
	memset(&rh_params, 0, sizeof(rh_params));
	rh_params.test = NRF_WIFI_RH_ONESHOT;
	/* enable rssi histogram logic */
	rh_params.rh_enable = 1;
	rh_params.mode = 0; /* oneshot mode */
	rh_params.hist_type = (strcmp(argv[1], "unconditional") == 0) ? 0 :
			      ((strcmp(argv[1], "pkt_only") == 0) ? 1 : 2);
	rh_params.stat_type = (strcmp(argv[2], "all") == 0) ? 0 :
			      ((strcmp(argv[2], "max") == 0) ? 1 : 2);
	rh_params.period = (unsigned char)period;
	rh_params.range_start = (signed char)range_start;
	rh_params.range_end = (signed char)range_end;
	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->rf_test_run = true;
	ctx->rf_test = NRF_WIFI_RH_ONESHOT;

	status = nrf_wifi_rt_fmac_rf_test_rh_oneshot(ctx->rpu_ctx,
						     &rh_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "RH test start failed\n");
		ctx->rf_test_run = false;
		ctx->rf_test = NRF_WIFI_RF_TEST_MAX;
		return -ENOEXEC;
	}

	ctx->rf_test_run = false;
	ctx->rf_test = NRF_WIFI_RF_TEST_MAX;

	return 0;
}
#endif /* WIFI_NRF71 */

#if (defined(WIFI_NRF71) && !defined(PHY_RF_PARAM_GDRAM)) || defined(WIFI_NRF70)

static int nrf_wifi_radio_test_set_ant_gain(const struct shell *shell,
					    size_t argc,
					    const char *argv[])
{
	char *ptr = NULL;
	unsigned long ant_gain = 0;

	ant_gain = strtoul(argv[1], &ptr, 10);

	if ((ant_gain < 0) || (ant_gain > 6)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid antenna gain setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&ctx->conf_params.rf_params[ANT_GAIN_2G_OFST],
	       ant_gain,
	       NUM_ANT_GAIN);

	return 0;
}

static int nrf_wifi_radio_test_set_edge_bo(const struct shell *shell,
					   size_t argc,
					   const char *argv[])
{
	char *ptr = NULL;
	unsigned long edge_bo = 0;

	edge_bo = strtoul(argv[1], &ptr, 10);

	if ((edge_bo < 0) || (edge_bo > 10)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid edge backoff setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	memset(&ctx->conf_params.rf_params[BAND_2G_LW_ED_BKF_DSSS_OFST],
	       edge_bo,
	       NUM_EDGE_BACKOFF);

	return 0;
}
#endif

#ifdef WIFI_NRF71
static int nrf_wifi_radio_test_set_rx_bss_color(const struct shell *shell,
						size_t argc,
						const char *argv[])
{
	char *ptr = NULL;
	unsigned long rx_bss_color = 0;

	rx_bss_color = strtoul(argv[1], &ptr, 10);

	if ((rx_bss_color < 0) || (rx_bss_color > 63)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid rx bss color setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.rx_bss_color = rx_bss_color;


	return 0;
}

static int nrf_wifi_radio_test_set_rx_station_id(const struct shell *shell,
						 size_t argc,
						 const char *argv[])
{
	char *ptr = NULL;
	unsigned long rx_station_id = 0;

	rx_station_id = strtoul(argv[1], &ptr, 10);

	if ((rx_station_id < 0) || (rx_station_id > 63)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid rx station id setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.rx_station_id = rx_station_id;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_dcm(const struct shell *shell,
					  size_t argc,
					  const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_dcm = 0;

	tx_dcm = strtoul(argv[1], &ptr, 10);

	if ((tx_dcm < 0) || (tx_dcm > 2)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx dcm setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_dcm = tx_dcm;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_doppler(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_doppler = 0;

	tx_doppler = strtoul(argv[1], &ptr, 10);

	if ((tx_doppler < 0) || (tx_doppler > 1)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx doppler setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_doppler = tx_doppler;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_midample_periodicity(const struct shell *shell,
							   size_t argc,
							   const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_midample_periodicity = 0;

	tx_midample_periodicity = strtoul(argv[1], &ptr, 10);

	if ((tx_midample_periodicity != 10) && (tx_midample_periodicity != 20)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx midample periodicity setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_midamble_periodicity = tx_midample_periodicity;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_106_tone(const struct shell *shell,
					       size_t argc,
					       const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_106_tone = 0;

	tx_106_tone = strtoul(argv[1], &ptr, 10);

	if ((tx_106_tone < 0) || (tx_106_tone > 2)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx 106 tone setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_106_tone = tx_106_tone;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_legacy_length(const struct shell *shell,
						    size_t argc,
						    const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_legacy_length = 0;

	tx_legacy_length = strtoul(argv[1], &ptr, 10);

	if ((tx_legacy_length < 0) || (tx_legacy_length > 4096)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx legacy length setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_legacy_length = tx_legacy_length;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_fec_padd_factor(const struct shell *shell,
						      size_t argc,
						      const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_fec_padd_factor = 0;

	tx_fec_padd_factor = strtoul(argv[1], &ptr, 10);

	if ((tx_fec_padd_factor < 0) || (tx_fec_padd_factor > 4)) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx fec padding factor setting\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_fec_padd_factor = tx_fec_padd_factor;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_num_he_ltf(const struct shell *shell,
						 size_t argc,
						 const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_num_he_ltf = 0;

	tx_num_he_ltf = strtoul(argv[1], &ptr, 10);

	/*
	 * Valid encoding for NUM-HE-LTF (802.11ax, Table 27-20):
	 * 0 -> 1 HE-LTF symbol
	 * 1 -> 2 HE-LTF symbols
	 * 2 -> 4 HE-LTF symbols
	 * 3 -> 6 HE-LTF symbols
	 * 4 -> 8 HE-LTF symbols
	 * Other values are reserved
	 */
	if (tx_num_he_ltf > 4) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid tx_num_he_ltf value "
			      "(0=1LTF, 1=2LTF, 2=4LTF, 3=6LTF, 4=8LTF)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.he_ltf =
		(unsigned char)tx_num_he_ltf;

	return 0;
}

static int nrf_wifi_radio_test_set_tx_fec_coding(const struct shell *shell,
						 size_t argc,
						 const char *argv[])
{
	char *ptr = NULL;
	unsigned long tx_fec_coding = 0;

	tx_fec_coding = strtoul(argv[1], &ptr, 10);

	/*
	 * FEC coding:
	 * 0 -> BCC
	 * 1 -> LDPC
	 */
	if (tx_fec_coding > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid FEC coding value "
			      "(0=BCC, 1=LDPC)\n");
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_fec_coding =
		(unsigned char)tx_fec_coding;

	return 0;
}
#endif

static int nrf_wifi_radio_test_show_cfg(const struct shell *shell,
					size_t argc,
					const char *argv[])
{
	struct rpu_conf_params *conf_params = NULL;

	conf_params = &ctx->conf_params;

	shell_fprintf(shell,
		      SHELL_INFO,
		      "************* Configured Parameters ***********\n");
	shell_fprintf(shell,
		      SHELL_INFO,
		      "\n");

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_tput_mode = %d\n",
		      conf_params->tx_pkt_tput_mode);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_sgi = %d\n",
		      conf_params->tx_pkt_sgi);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_preamble = %d\n",
		      conf_params->tx_pkt_preamble);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_mcs = %d\n",
		      conf_params->tx_pkt_mcs);

	if (conf_params->tx_pkt_rate == 5)
		shell_fprintf(shell,
			      SHELL_INFO,
			      "tx_pkt_rate = 5.5\n");
	else
		shell_fprintf(shell,
			      SHELL_INFO,
			      "tx_pkt_rate = %d\n",
			      conf_params->tx_pkt_rate);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_gap = %d\n",
		      conf_params->tx_pkt_gap_us);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "phy_calib_rxdc = %d\n",
		      (conf_params->phy_calib &
		       NRF_WIFI_PHY_CALIB_FLAG_RXDC) ? 1:0);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "phy_calib_txdc = %d\n",
		      (conf_params->phy_calib &
		       NRF_WIFI_PHY_CALIB_FLAG_TXDC) ? 1:0);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "phy_calib_txpow = %d\n",
		      (conf_params->phy_calib &
		       NRF_WIFI_PHY_CALIB_FLAG_TXPOW) ? 1:0);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "phy_calib_rxiq = %d\n",
		      (conf_params->phy_calib &
		       NRF_WIFI_PHY_CALIB_FLAG_RXIQ) ? 1:0);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "phy_calib_txiq = %d\n",
		      (conf_params->phy_calib &
		       NRF_WIFI_PHY_CALIB_FLAG_TXIQ) ? 1:0);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_num = %d\n",
		      conf_params->tx_pkt_num);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_len = %d\n",
		      conf_params->tx_pkt_len);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_power = %d\n",
		      conf_params->tx_power);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "he_ltf = %d\n",
		      conf_params->he_ltf);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "he_gi = %d\n",
		      conf_params->he_gi);
#ifndef WIFI_NRF71
	shell_fprintf(shell,
		      SHELL_INFO,
		      "xo_val = %d\n",
		      conf_params->rf_params[NRF_WIFI_XO_FREQ_BYTE_OFFSET]);
#endif /* WIFI_NRF71 */
	shell_fprintf(shell,
		      SHELL_INFO,
		      "init = op_band(%d) channel(%d)\n",
		      conf_params->chan.op_band,
		      conf_params->chan.primary_num);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx = %d\n",
		      conf_params->tx);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx = %d\n",
		      conf_params->rx);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_tone_freq = %d\n",
		      conf_params->tx_tone_freq);

#ifdef WIFI_NRF71
	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_tone_type = %u\n",
		      conf_params->tx_tone_type);
	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_tone_dc_offset_i = %d\n",
		      conf_params->tx_tone_dc_offset_i);
	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_tone_dc_offset_q = %d\n",
		      conf_params->tx_tone_dc_offset_q);
#endif

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_lna_gain = %d\n",
		      conf_params->lna_gain);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_bb_gain = %d\n",
		      conf_params->bb_gain);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_capture_length = %d\n",
		      conf_params->capture_length);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_capture_timeout = %d\n",
		      conf_params->capture_timeout);

#ifdef WIFI_NRF71
	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_capture_ed_thresh_ofdm = %d\n",
		      (signed char)conf_params->ed_thresh_ofdm);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_capture_ed_thresh_dsss = %d\n",
		      (signed char)conf_params->ed_thresh_dsss);
#endif

#if defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP_NRF7001) || \
	defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP)
	shell_fprintf(shell,
		      SHELL_INFO,
		      "sr_ant_switch_ctrl = %d\n",
		      conf_params->sr_ant_switch_ctrl);
#endif /* CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP_NRF7001 || CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP */

	shell_fprintf(shell,
		      SHELL_INFO,
		      "wlan_ant_switch_ctrl = %d\n",
		      conf_params->wlan_ant_switch_ctrl);
	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_pkt_cw = %d\n",
		      conf_params->tx_pkt_cw);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "reg_domain = %c%c\n",
		      conf_params->country_code[0],
		      conf_params->country_code[1]);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "bypass_reg_domain = %d\n",
		      conf_params->bypass_regulatory);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "ru_tone = %d\n",
		      conf_params->ru_tone);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "ru_index = %d\n",
		      conf_params->ru_index);
#ifdef WIFI_NRF71
	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_bss_color = %d\n",
		      conf_params->rx_bss_color);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rx_station_id = %d\n",
		      conf_params->rx_station_id);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_dcm = %d\n",
		      conf_params->tx_dcm);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_doppler = %d\n",
		      conf_params->tx_doppler);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_midample_periodicity = %d\n",
		      conf_params->tx_midamble_periodicity);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_106_tone = %d\n",
		      conf_params->tx_106_tone);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_legacy_length = %d\n",
		      conf_params->tx_legacy_length);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "tx_fec_padd_factor = %d\n",
		      conf_params->tx_fec_padd_factor);
#endif
	return 0;
}


static int nrf_wifi_radio_test_get_stats(const struct shell *shell,
					 size_t argc,
					 const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct rpu_rt_op_stats stats;

	memset(&stats,
	       0,
	       sizeof(stats));

	status = nrf_wifi_rt_fmac_stats_get(ctx->rpu_ctx,
					 ctx->conf_params.op_mode,
					 &stats);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "nrf_wifi_rt_fmac_stats_get failed\n");
		return -ENOEXEC;
	}

	shell_fprintf(shell,
		      SHELL_INFO,
		      "************* PHY STATS ***********\n");

	shell_fprintf(shell,
		      SHELL_INFO,
		      "rssi_avg = %d dBm\n",
		      stats.fw.phy.rssi_avg);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "ofdm_crc32_pass_cnt=%d\n",
		      stats.fw.phy.ofdm_crc32_pass_cnt);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "ofdm_crc32_fail_cnt=%d\n",
		      stats.fw.phy.ofdm_crc32_fail_cnt);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "dsss_crc32_pass_cnt=%d\n",
		      stats.fw.phy.dsss_crc32_pass_cnt);

	shell_fprintf(shell,
		      SHELL_INFO,
		      "dsss_crc32_fail_cnt=%d\n",
		      stats.fw.phy.dsss_crc32_fail_cnt);

	return 0;
}

/* See enum CD2CM_MSG_ID_T in RPU Coexistence Manager API */
#define CD2CM_UPDATE_SWITCH_CONFIG 0x7
static int nrf_wifi_radio_test_wlan_switch_ctrl(const struct shell *shell,
						size_t argc,
						const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	struct coex_wlan_switch_ctrl params = { 0 };

	if (argc < 2) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid number of parameters\n");
		shell_help(shell);
		return -ENOEXEC;
	}

	params.rpu_msg_id = CD2CM_UPDATE_SWITCH_CONFIG;
	params.switch_A = strtoul(argv[1], &ptr, 10);

	if (params.switch_A > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid WLAN switch config (%d)\n",
			      params.switch_A);
		shell_help(shell);
		return -ENOEXEC;
	}

	ctx->conf_params.wlan_ant_switch_ctrl = params.switch_A;

	status = nrf_wifi_fmac_conf_srcoex(ctx->rpu_ctx,
					   &params, sizeof(params));

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "WLAN switch configuration failed\n");
		return -ENOEXEC;
	}

	return 0;
}


static int nrf_wifi_radio_test_set_reg_domain(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	int ret = -ENOEXEC;
	struct nrf_wifi_fmac_reg_info reg_domain_info = {0};

	if (strlen(argv[1]) != 2) {
		shell_fprintf(shell, SHELL_WARNING,
			"Invalid reg domain: Length should be two letters/digits\n");
			goto out;
	}

	/* Two letter country code with special case of 00 for WORLD */
	if (((argv[1][0] < 'A' || argv[1][0] > 'Z') ||
	     (argv[1][1] < 'A' || argv[1][1] > 'Z')) &&
	     (argv[1][0] != '0' || argv[1][1] != '0')) {
		shell_fprintf(shell, SHELL_WARNING, "Invalid reg domain %c%c\n", argv[1][0],
			      argv[2][1]);
		goto out;
	}

	ctx->conf_params.country_code[0] = argv[1][0];
	ctx->conf_params.country_code[1] = argv[1][1];

	if (!check_test_in_prog(shell)) {
		goto out;
	}

	memcpy(reg_domain_info.alpha2, ctx->conf_params.country_code,
			NRF_WIFI_COUNTRY_CODE_LEN);

	status = nrf_wifi_fmac_set_reg(ctx->rpu_ctx, &reg_domain_info);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Regulatory programming failed\n");
		goto out;
	}

	ret = 0;
out:
	return ret;
}


static int nrf_wifi_radio_test_set_bypass_reg(const struct shell *shell,
					      size_t argc,
					      const char *argv[])
{
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if (val > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid value %lu\n",
			      val);
		shell_help(shell);
		return -ENOEXEC;
	}

	if (!check_test_in_prog(shell)) {
		return -ENOEXEC;
	}

	ctx->conf_params.bypass_regulatory = val;

	return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(
	nrf_wifi_radio_test_subcmds,
	SHELL_CMD_ARG(set_defaults,
		      NULL,
		      "Reset configuration parameter to their default values",
		      nrf_wifi_radio_test_set_defaults,
		      1,
		      0),
	SHELL_CMD_ARG(phy_calib_rxdc,
		      NULL,
		      "0 - Disable RX DC calibration\n"
		      "1 - Enable RX DC calibration",
		      nrf_wifi_radio_test_set_phy_calib_rxdc,
		      2,
		      0),
	SHELL_CMD_ARG(phy_calib_txdc,
		      NULL,
		      "0 - Disable TX DC calibration\n"
		      "1 - Enable TX DC calibration",
		      nrf_wifi_radio_test_set_phy_calib_txdc,
		      2,
		      0),
	SHELL_CMD_ARG(phy_calib_txpow,
		      NULL,
		      "0 - Disable TX power calibration\n"
		      "1 - Enable TX power calibration",
		      nrf_wifi_radio_test_set_phy_calib_txpow,
		      2,
		      0),
	SHELL_CMD_ARG(phy_calib_rxiq,
		      NULL,
		      "0 - Disable RX IQ calibration\n"
		      "1 - Enable RX IQ calibration",
		      nrf_wifi_radio_test_set_phy_calib_rxiq,
		      2,
		      0),
	SHELL_CMD_ARG(phy_calib_txiq,
		      NULL,
		      "0 - Disable TX IQ calibration\n"
		      "1 - Enable TX IQ calibration",
		      nrf_wifi_radio_test_set_phy_calib_txiq,
		      2,
		      0),
#ifdef WIFI_NRF71
	SHELL_CMD_ARG(perform_rf_calibration,
		      NULL,
		      "<calib_bitmap> <sys_operating_mode> <result_index> - Run RF calibration",
		      nrf_wifi_radio_test_perform_rf_calibration,
		      3,
		      1),
	SHELL_CMD_ARG(read_rf_comp_results,
		      NULL,
		      "<mode> [result_index] - Read comp results into buffer (mode: 0=operating 1=scan; index 0 or 1)",
		      nrf_wifi_radio_test_read_rf_comp_results,
		      2,
		      1),
	SHELL_CMD_ARG(apply_rf_compensation,
		      NULL,
		      "<result_index> - Apply calibration from in-memory result buffer (0 or 1)",
		      nrf_wifi_radio_test_apply_rf_compensation,
		      2,
		      0),
	SHELL_CMD_ARG(set_calib_regs,
		      NULL,
		      "<cal_id> <num_regs> <addr0> <val0> [addr1 val1 ...] - Set calibration registers",
		      nrf_wifi_radio_test_set_calib_regs,
		      4,
		      19),
	SHELL_CMD_ARG(rf_patch_settings,
		      NULL,
		      "<patch_type> <index> <slice> <value> [band] [is_new_setting] - RF patch settings (patch_type 0..11)",
		      nrf_wifi_radio_test_rf_patch_settings,
		      5,
		      7),
	SHELL_CMD_ARG(phy_debug_stats,
		      NULL,
		      "Get PHY RX debug stats",
		      nrf_wifi_radio_test_phy_debug_stats,
		      1,
		      0),
	SHELL_CMD_ARG(config_vtf_params,
		      NULL,
		      "<voltage> <temp> <x0_freq> - x0 signed 8-bit, use before init",
		      nrf_wifi_radio_test_config_vtf_params,
		      4,
		      0),
#ifdef PHY_RF_PARAM_GDRAM
	SHELL_CMD_ARG(config_phy_rf_param,
		      NULL,
		      "<1..22> <hex> - RF parameters string, before init",
		      nrf_wifi_radio_test_config_phy_rf_param,
		      3,
		      0),
	SHELL_CMD_ARG(config_edge_ceilings,
		      NULL,
		      "<hex> - edge ceilings, before init",
		      nrf_wifi_radio_test_config_edge_ceilings,
		      2,
		      0),
	SHELL_CMD_ARG(config_antenna_gain,
		      NULL,
		      "<hex> - antenna gain PARAM10, 10 hex chars, before init",
		      nrf_wifi_radio_test_config_antenna_gain,
		      2,
		      0),
#endif
	SHELL_CMD_ARG(enable_vt_calib,
		      NULL,
		      "<0|1> - Disable or enable VT calibration",
		      nrf_wifi_radio_test_enable_vt_calib,
		      2,
		      0),
	SHELL_CMD_ARG(enable_vt_comp,
		      NULL,
		      "<0|1> - Disable or enable VT compensation",
		      nrf_wifi_radio_test_enable_vt_comp,
		      2,
		      0),
	SHELL_CMD_ARG(set_reg,
		      NULL,
		      "<addr1> <val1> [addr2 val2 ...] - Write regs (max 8 pairs)",
		      nrf_wifi_radio_test_set_reg,
		      3,
		      16),
	SHELL_CMD_ARG(read_reg,
		      NULL,
		      "<addr1> [addr2 ...] - Read regs (max 8 addrs)",
		      nrf_wifi_radio_test_read_reg,
		      2,
		      8),
	SHELL_CMD_ARG(set_memory,
		      NULL,
		      "<addr1> <val1> [addr2 val2 ...] - Write memory (max 8 pairs)",
		      nrf_wifi_radio_test_set_memory,
		      3,
		      16),
	SHELL_CMD_ARG(read_memory,
		      NULL,
		      "<addr1> [addr2 ...] - Read memory (max 8 addrs)",
		      nrf_wifi_radio_test_read_memory,
		      2,
		      8),
	SHELL_CMD_ARG(adpll_cap,
		      NULL,
		      "<enabled> <enable_tracing> <cap_len> - ADPLL capture (NORMAL)",
		      nrf_wifi_radio_test_adpll_cap,
		      4,
		      0),
#endif
	SHELL_CMD_ARG(he_ltf,
		      NULL,
		      "0 - 1x HE LTF\n"
		      "1 - 2x HE LTF\n"
		      "2 - 4x HE LTF                                        ",
		      nrf_wifi_radio_test_set_he_ltf,
		      2,
		      0),
	SHELL_CMD_ARG(he_gi,
		      NULL,
		      "0 - 0.8 us\n"
		      "1 - 1.6 us\n"
		      "2 - 3.2 us                                           ",
		      nrf_wifi_radio_test_set_he_gi,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_tput_mode,
		      NULL,
		      "0 - Legacy mode\n"
		      "1 - HT mode\n"
#ifndef CONFIG_NRF70_2_4G_ONLY
		      "2 - VHT mode\n"
#endif /* CONFIG_NRF70_2_4G_ONLY */
		      "3 - HE(SU) mode\n"
		      "4 - HE(ER SU) mode\n"
		      "5 - HE (TB) mode                                   ",
		      nrf_wifi_radio_test_set_tx_pkt_tput_mode,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_sgi,
		      NULL,
		      "0 - Disable\n"
		      "1 - Enable",
		      nrf_wifi_radio_test_set_tx_pkt_sgi,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_preamble,
		      NULL,
		      "0 - Long preamble\n"
		      "1 - Short preamble\n"
		      "2 - Mixed preamble                                   ",
		      nrf_wifi_radio_test_set_tx_pkt_preamble,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_mcs,
		      NULL,
		      "-1    - Not being used\n"
		      "<val> - MCS index to be used",
		      nrf_wifi_radio_test_set_tx_pkt_mcs,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_rate,
		      NULL,
		      "-1    - Not being used\n"
		      "<val> - Legacy rate to be used in Mbps (1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54)",
		      nrf_wifi_radio_test_set_tx_pkt_rate,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_gap,
		      NULL,
		      "<val> - Interval between TX packets in us (Min: 200, Max: 200000, Default: 200)",
		      nrf_wifi_radio_test_set_tx_pkt_gap,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_num,
		      NULL,
		      "-1    - Transmit infinite packets\n"
		      "<val> - Number of packets to transmit",
		      nrf_wifi_radio_test_set_tx_pkt_num,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_len,
		      NULL,
		      "<val> - Length of the packet (in bytes) to be transmitted (Default: 1400)",
		      nrf_wifi_radio_test_set_tx_pkt_len,
		      2,
		      0),
	SHELL_CMD_ARG(tx_power,
		      NULL,
		      "<val> - Value in dBm",
		      nrf_wifi_radio_test_set_tx_power,
		      2,
		      0),
	SHELL_CMD_ARG(ru_tone,
		      NULL,
		      "<val> - Resource unit (RU) size (26,52,106 or 242)",
		      nrf_wifi_radio_test_set_ru_tone,
		      2,
		      0),
	SHELL_CMD_ARG(ru_index,
		      NULL,
		      "<val> - Location of resource unit (RU) in 20 MHz spectrum\n"
		      "        Valid Values:\n"
		      "           For 26 ru_tone: 1 to 9\n"
		      "           For 52 ru_tone:  1 to 4\n"
		      "           For 106 ru_tone:  1 to 2\n"
		      "           For 242 ru_tone:  1",
		      nrf_wifi_radio_test_set_ru_index,
		      2,
		      0),
	SHELL_CMD_ARG(init,
		      NULL,
		      "<val> - Band\n"
		      "<val> - Primary channel number",
		      nrf_wifi_radio_test_init,
		      3,
		      0),
	SHELL_CMD_ARG(tx,
		      NULL,
		      "0 - Disable TX\n"
		      "1 - Enable TX",
		      nrf_wifi_radio_test_set_tx,
		      2,
		      0),
	SHELL_CMD_ARG(rx,
		      NULL,
		      "0 - Disable RX\n"
		      "1 - Enable RX",
		      nrf_wifi_radio_test_set_rx,
		      2,
		      0),
#ifdef CONFIG_NRF70_SR_COEX_RF_SWITCH
	SHELL_CMD_ARG(sr_ant_switch_ctrl,
		      NULL,
		      "0 - Switch set to use the BLE antenna\n"
		      "1 - Switch set to use the shared Wi-Fi antenna",
		      nrf_wifi_radio_test_sr_ant_switch_ctrl,
		      2,
		      0),
#endif /* CONFIG_NRF70_SR_COEX_RF_SWITCH */

#ifdef CONFIG_NRF70_SR_COEX
	SHELL_CMD_ARG(config_pta,
		      NULL,
		      " - <val> - Wi-Fi operating band 0: 2.4GHz, 1: 5GHz\n"
		      " - <val> - Antenna mode 0: Shared, 1: Separate\n"
		      " - <val> - SR protocol  0: Thread, 1: Bluetooth LE\n",
		      nrf_wifi_radio_test_config_pta,
		      4,
		      0),
#endif /* CONFIG_NRF70_SR_COEX */

	SHELL_CMD_ARG(rx_lna_gain,
		      NULL,
		      "<val> - LNA gain to be configured.\n"
			  "0 = 24 dB\n"
			  "1 = 18 dB\n"
			  "2 = 12 dB\n"
			  "3 = 0 dB\n"
			  "4 = -12 dB                         ",
		      nrf_wifi_radio_test_set_rx_lna_gain,
		      2,
		      0),
	SHELL_CMD_ARG(rx_bb_gain,
		      NULL,
		      "<val> - Baseband gain to be configured\n"
			  "It is a 5 bit value. Supports 64dB range in steps of 2dB",
		      nrf_wifi_radio_test_set_rx_bb_gain,
		      2,
		      0),
	SHELL_CMD_ARG(rx_capture_length,
		      NULL,
		      "<val> - Number of RX samples to be captured (nRF71: NRF_WIFI_RF_TEST_RX_CAPTURE_MAX_SAMPLES)",
		      nrf_wifi_radio_test_set_rx_capture_length,
		      2,
		      0),
	SHELL_CMD_ARG(rx_capture_timeout,
		      NULL,
		      "<val> - Wait time allowed in seconds\n"
		      "Max timeout in seconds (see shell error for upper bound)",
		      nrf_wifi_radio_test_set_rx_capture_timeout,
		      2,
		      0),
#ifdef WIFI_NRF71
	SHELL_CMD_ARG(rx_capture_ed_thresh,
		      NULL,
		      "<ofdm> <dsss> - ED thresholds dynamic packet capture (-100..0)",
		      nrf_wifi_radio_test_set_rx_capture_ed_thresh,
		      3,
		      0),
#endif
	SHELL_CMD_ARG(rx_cap,
		      NULL,
		      "0 = ADC capture\n"
		      "1 = Filtered ADC capture\n"
		      "2 = Dynamic packet capture",
		      nrf_wifi_radio_test_rx_cap,
		      2,
		      0),
	SHELL_CMD_ARG(tx_tone_freq,
		      NULL,
		      "<val> - Frequency offset with respect to center frequency in the range of -10MHz to 10MHz (resolution 1MHz)",
		      nrf_wifi_radio_test_set_tx_tone_freq,
		      2,
		      0),
#ifdef WIFI_NRF71
	SHELL_CMD_ARG(tx_tone_type,
		      NULL,
		      "<val> - Tone type\n"
		      "0 = Complex\n"
		      "1 = Real-only\n"
		      "2 = Imag-only                                       ",
		      nrf_wifi_radio_test_set_tx_tone_type,
		      2,
		      0),
	SHELL_CMD_ARG(tx_tone_dc_offset,
		      NULL,
		      "<mode> <DC_offset_i> <DC_offset_q>\n"
		      "0 0 0 - No DC offsets\n"
		      "1 <DC_offset_i> 0 - I only\n"
		      "2 0 <DC_offset_q> - Q only\n"
		      "3 <DC_offset_i> <DC_offset_q> - Both",
		      nrf_wifi_radio_test_set_tx_tone_dc_offset,
		      4,
		      0),
#endif
	SHELL_CMD_ARG(tx_tone,
		      NULL,
		      "<TONE CONTROL>\n"
		      "0: Disable tone\n"
		      "1: Enable tone                                       ",
		      nrf_wifi_radio_test_tx_tone,
		      2,
		      0),
	SHELL_CMD_ARG(dpd,
		      NULL,
		      "0 - Bypass DPD\n"
		      "1 - Enable DPD",
		      nrf_wifi_radio_set_dpd,
		      2,
		      0),
	SHELL_CMD_ARG(get_temperature,
		      NULL,
		      "No arguments required",
		      nrf_wifi_radio_get_temperature,
		      1,
		      0),
	SHELL_CMD_ARG(get_voltage,
		      NULL,
		      "No arguments required",
		      nrf_wifi_radio_get_bat_volt,
		      1,
		      0),
	SHELL_CMD_ARG(get_rf_rssi,
		      NULL,
		      "No arguments required",
		      nrf_wifi_radio_get_rf_rssi,
		      1,
		      0),
	SHELL_CMD_ARG(set_xo_val,
		      NULL,
#ifdef WIFI_NRF71
		      "<val> - XO value in the range -100 to 100 (signed PPM)",
#else
		      "<val> - XO value in the range 0 to 127",
#endif
		      nrf_wifi_radio_set_xo_val,
		      2,
		      0),
	SHELL_CMD_ARG(compute_optimal_xo_val,
		      NULL,
		      "Compute optimal XO trim value",
		      nrf_wifi_radio_comp_opt_xo_val,
		      1,
		      0),
	SHELL_CMD_ARG(show_config,
		      NULL,
		      "Display the current configuration values",
		      nrf_wifi_radio_test_show_cfg,
		      1,
		      0),
	SHELL_CMD_ARG(get_stats,
		      NULL,
		      "Display statistics",
		      nrf_wifi_radio_test_get_stats,
		      1,
		      0),
	SHELL_CMD_ARG(wlan_ant_switch_ctrl,
		      NULL,
		      "Configure WLAN antenna switch (0-separate/1-shared)",
		      nrf_wifi_radio_test_wlan_switch_ctrl,
		      2,
		      0),
	SHELL_CMD_ARG(tx_pkt_cw,
		      NULL,
		      "<val> - Contention window value to be configured (0, 3, 7, 15, 31, 63, 127, 255, 511, 1023)",
		      nrf_wifi_radio_test_set_tx_pkt_cw,
		      2,
		      0),
	SHELL_CMD_ARG(reg_domain,
		      NULL,
		      "Configure WLAN regulatory domain country code",
		      nrf_wifi_radio_test_set_reg_domain,
		      2,
		      0),
	SHELL_CMD_ARG(bypass_reg_domain,
		      NULL,
		      "Configure WLAN to bypass regulatory\n"
		      "0 - TX power of the channel will be set to "
			   "minimum between user configured TX power & "
			   "maximum TX power of channel in the configured regulatory domain.\n"
		      "1 - Configured TX power value will be used for the channel.				",
		      nrf_wifi_radio_test_set_bypass_reg,
		      2,
		      0),
#if (defined(WIFI_NRF71) && !defined(PHY_RF_PARAM_GDRAM)) || defined(WIFI_NRF70)
	SHELL_CMD_ARG(set_ant_gain,
		      NULL,
		      "<val> - Antenna gain in dB (Min: 0, Max: 6)",
		      nrf_wifi_radio_test_set_ant_gain,
		      2,
		      0),
	SHELL_CMD_ARG(set_edge_bo,
		      NULL,
		      "<val> - Edge backoff in dB (Min: 0, Max: 10)",
		      nrf_wifi_radio_test_set_edge_bo,
		      2,
		      0),
#endif
#ifdef WIFI_NRF71
	SHELL_CMD_ARG(rx_bss_color,
		      NULL,
		      "<val> - bss color (1 to 63)",
		      nrf_wifi_radio_test_set_rx_bss_color,
		      2,
		      0),
	SHELL_CMD_ARG(rx_station_id,
		      NULL,
		      "<val> - station id (1 to 2047)",
		      nrf_wifi_radio_test_set_rx_station_id,
		      2,
		      0),
	SHELL_CMD_ARG(tx_dcm,
		      NULL,
		      "<val> - dcm (0 or 1)",
		      nrf_wifi_radio_test_set_tx_dcm,
		      2,
		      0),
	SHELL_CMD_ARG(tx_doppler,
		      NULL,
		      "<val> - doppler (0 or 1)",
		      nrf_wifi_radio_test_set_tx_doppler,
		      2,
		      0),
	SHELL_CMD_ARG(tx_midample_periodicity,
		      NULL,
		      "<val> - tx_midample_periodicity (10 or 20)",
		      nrf_wifi_radio_test_set_tx_midample_periodicity,
		      2,
		      0),
	SHELL_CMD_ARG(tx_106_tone,
		      NULL,
		      "<val> - 106_tone (0 or 1)",
		      nrf_wifi_radio_test_set_tx_106_tone,
		      2,
		      0),
	SHELL_CMD_ARG(tx_legacy_length,
		      NULL,
		      "<val> - legacy_length (upto 4095)",
		      nrf_wifi_radio_test_set_tx_legacy_length,
		      2,
		      0),
	SHELL_CMD_ARG(tx_fec_padd_factor,
		      NULL,
		      "<val> - fec padding factor(1,2,3,4)",
		      nrf_wifi_radio_test_set_tx_fec_padd_factor,
		      2,
		      0),
	SHELL_CMD_ARG(tx_num_he_ltf,
		      NULL,
		      "<val> - tx_num_he_ltf (0=1LTF, 1=2LTF, 2=4LTF, 3=6LTF, 4=8LTF)",
		      nrf_wifi_radio_test_set_tx_num_he_ltf,
		      2,
		      0),
	SHELL_CMD_ARG(tx_fec_coding,
		      NULL,
		      "<val> -Set TX FEC coding (0=BCC, 1=LDPC)",
		      nrf_wifi_radio_test_set_tx_fec_coding,
		      2,
		      0),
	SHELL_CMD_ARG(rh_oneshot,
		      NULL,
		      "RSSI histogram oneshot: <hist_type> <stat_type> <period_s> [range_start_dBm range_end_dBm]",
		      nrf_wifi_radio_test_rh_oneshot,
		      4,
		      2),
#endif
	SHELL_SUBCMD_SET_END);


SHELL_CMD_REGISTER(wifi_radio_test,
		   &nrf_wifi_radio_test_subcmds,
		   "nRF Wi-Fi radio test commands",
		   NULL);


static int nrf_wifi_radio_test_shell_init(void)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned int timeout = 0;

	while (!ctx->rpu_ctx && timeout < NRF_WIFI_RADIO_TEST_INIT_TIMEOUT_MS) {
		k_sleep(K_MSEC(100));
		timeout += 100;
	}

	if (!ctx->rpu_ctx) {
		printf("nRF Wi-Fi radio test shell init timedout waiting for driver: %d\n",
		       NRF_WIFI_RADIO_TEST_INIT_TIMEOUT_MS);
		return -ENOEXEC;
	}

	status = nrf_wifi_radio_test_conf_init(&ctx->conf_params);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		return -ENOEXEC;
	}

	return 0;
}


SYS_INIT(nrf_wifi_radio_test_shell_init,
	 APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
