#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_WPA_SUPP)

zephyr_library()

set(CMAKE_CURRENT_SOURCE_DIR ../freertos)
set(HOSTAP_BASE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(WPA_SUPPLICANT_BASE ${HOSTAP_BASE}/wpa_supplicant)
set(COMMON_SRC_BASE ${HOSTAP_BASE}/src)
set(MBEDTLS_PORT_BASE ${HOSTAP_BASE}/port/mbedtls)
set(HOSTAPD_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../hostapd)

set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs -lnosys")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_SYSCALL_NAMES")

zephyr_include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${HOSTAP_BASE}/
	${WPA_SUPPLICANT_BASE}/
	${COMMON_SRC_BASE}/
	${COMMON_SRC_BASE}/common
	${COMMON_SRC_BASE}/utils
	${COMMON_SRC_BASE}/eap_common
	${COMMON_SRC_BASE}/eap_server
	${COMMON_SRC_BASE}/radius
	${COMMON_SRC_BASE}/ap
	${COMMON_SRC_BASE}/crypto
	${HOSTAPD_BASE}/
  ${MBEDTLS_PORT_BASE}/
	${COMMON_SRC_BASE}/rsn_supp
)

zephyr_include_directories_ifdef(CONFIG_KSDK_MBEDTLS
	${MBEDTLS_PORT_BASE}/ksdk
)

zephyr_library_compile_definitions(
	TLS_DEFAULT_CIPHERS=\""DEFAULT:!EXP:!LOW"\"
	CONFIG_WPA_S_ZEPHYR_L2_WIFI_MGMT
)

zephyr_library_include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${HOSTAP_BASE}/
	${COMMON_SRC_BASE}/utils
	${COMMON_SRC_BASE}/drivers
	${HOSTAP_BASE}/src
	${ZEPHYR_BASE}/include
	${ZEPHYR_BASE}/include/net
	${HOSTAPD_BASE}/
)

zephyr_library_sources(
	${COMMON_SRC_BASE}/common/wpa_common.c
	${COMMON_SRC_BASE}/common/ieee802_11_common.c
	${COMMON_SRC_BASE}/common/hw_features_common.c
	${COMMON_SRC_BASE}/common/wpa_ctrl.c
	${COMMON_SRC_BASE}/common/cli.c

	${COMMON_SRC_BASE}/drivers/driver_common.c
	${COMMON_SRC_BASE}/drivers/drivers.c
	${COMMON_SRC_BASE}/l2_packet/l2_packet_freertos.c
	${COMMON_SRC_BASE}/drivers/driver_freertos.c
	${COMMON_SRC_BASE}/utils/base64.c
	${COMMON_SRC_BASE}/utils/common.c
	${COMMON_SRC_BASE}/utils/wpabuf.c
	${COMMON_SRC_BASE}/utils/bitfield.c
	${COMMON_SRC_BASE}/utils/block_alloc.c
	${COMMON_SRC_BASE}/utils/eloop_freertos.c
	${COMMON_SRC_BASE}/utils/os_zephyr.c
	${COMMON_SRC_BASE}/utils/radiotap.c
	${COMMON_SRC_BASE}/utils/crc32.c
	${COMMON_SRC_BASE}/eapol_supp/eapol_supp_sm.c

	#${COMMON_SRC_BASE}/utils/edit_simple.c
	${WPA_SUPPLICANT_BASE}/config.c
	${WPA_SUPPLICANT_BASE}/notify.c
	${WPA_SUPPLICANT_BASE}/bss.c
	${WPA_SUPPLICANT_BASE}/eap_register.c
	${WPA_SUPPLICANT_BASE}/op_classes.c
	${WPA_SUPPLICANT_BASE}/rrm.c
	${WPA_SUPPLICANT_BASE}/wmm_ac.c
	${WPA_SUPPLICANT_BASE}/config_none.c
	${WPA_SUPPLICANT_BASE}/sme.c
	${WPA_SUPPLICANT_BASE}/wpa_supplicant.c
	${WPA_SUPPLICANT_BASE}/events.c
	${WPA_SUPPLICANT_BASE}/bssid_ignore.c
	${WPA_SUPPLICANT_BASE}/wpas_glue.c
	${WPA_SUPPLICANT_BASE}/scan.c
	${WPA_SUPPLICANT_BASE}/robust_av.c
	${WPA_SUPPLICANT_BASE}/ctrl_iface.c
	${WPA_SUPPLICANT_BASE}/ctrl_iface_udp.c
	#${WPA_SUPPLICANT_BASE}/wpa_cli.c
	${WPA_SUPPLICANT_BASE}/mbo.c
	${WPA_SUPPLICANT_BASE}/wnm_sta.c
	# Zephyr main
	${CMAKE_CURRENT_SOURCE_DIR}/src/supp_main.c
	${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/wpa_debug.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/supp_api.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/wpa_cli.c
	# hostapd
	${HOSTAPD_BASE}/eap_register.c
    ${HOSTAPD_BASE}/hlr_auc_gw.c
	# radius
	${COMMON_SRC_BASE}/radius/radius_server.c
	${COMMON_SRC_BASE}/radius/radius_das.c
    #${COMMON_SRC_BASE}/radius/radius_client.c
	${COMMON_SRC_BASE}/radius/radius.c

	# 11k/11v/11r
	${COMMON_SRC_BASE}/rsn_supp/wpa_ft.c
	${COMMON_SRC_BASE}/rsn_supp/wpa_ie.c
	${COMMON_SRC_BASE}/rsn_supp/wpa.c
)

zephyr_library_sources_ifdef(CONFIG_WPA_CLI
	src/wpa_cli.c
)

zephyr_library_sources_ifdef(CONFIG_HOSTAPD
	${WPA_SUPPLICANT_BASE}/ap.c
	${COMMON_SRC_BASE}/ap/ap_config.c
	${COMMON_SRC_BASE}/ap/ap_drv_ops.c
	${COMMON_SRC_BASE}/ap/ap_list.c
	${COMMON_SRC_BASE}/ap/ap_mlme.c
	${COMMON_SRC_BASE}/ap/authsrv.c
	${COMMON_SRC_BASE}/ap/beacon.c
	${COMMON_SRC_BASE}/ap/bss_load.c
	${COMMON_SRC_BASE}/ap/dfs.c
	${COMMON_SRC_BASE}/ap/acs.c
	${COMMON_SRC_BASE}/ap/drv_callbacks.c
	${COMMON_SRC_BASE}/ap/eap_user_db.c
	${COMMON_SRC_BASE}/ap/eth_p_oui.c
	${COMMON_SRC_BASE}/ap/hostapd.c
	${COMMON_SRC_BASE}/ap/hw_features.c
	${COMMON_SRC_BASE}/ap/ieee802_11_auth.c
	${COMMON_SRC_BASE}/ap/ieee802_11.c
	${COMMON_SRC_BASE}/ap/ieee802_11_he.c
	${COMMON_SRC_BASE}/ap/ieee802_11_ht.c
	${COMMON_SRC_BASE}/ap/ieee802_11_shared.c
	${COMMON_SRC_BASE}/ap/ieee802_11_vht.c
	${COMMON_SRC_BASE}/ap/ieee802_1x.c
	${COMMON_SRC_BASE}/ap/neighbor_db.c
	${COMMON_SRC_BASE}/ap/p2p_hostapd.c
	${COMMON_SRC_BASE}/ap/pmksa_cache_auth.c
	${COMMON_SRC_BASE}/ap/preauth_auth.c
	${COMMON_SRC_BASE}/ap/rrm.c
	${COMMON_SRC_BASE}/ap/sta_info.c
	${COMMON_SRC_BASE}/ap/tkip_countermeasures.c
	${COMMON_SRC_BASE}/ap/utils.c
	${COMMON_SRC_BASE}/ap/wmm.c
	${COMMON_SRC_BASE}/ap/ctrl_iface_ap.c
	${COMMON_SRC_BASE}/ap/mbo_ap.c
	${COMMON_SRC_BASE}/ap/wnm_ap.c
	${COMMON_SRC_BASE}/ap/eth_p_oui.c

	${COMMON_SRC_BASE}/ap/wpa_auth.c
	${COMMON_SRC_BASE}/ap/wpa_auth_ie.c
	${COMMON_SRC_BASE}/ap/wpa_auth_ft.c
	${COMMON_SRC_BASE}/ap/wpa_auth_glue.c

	${COMMON_SRC_BASE}/eap_common/eap_common.c
	${COMMON_SRC_BASE}/eap_server/eap_server.c
	${COMMON_SRC_BASE}/eap_server/eap_server_tls_common.c
	${COMMON_SRC_BASE}/eap_server/eap_server_tls.c
	${COMMON_SRC_BASE}/eap_server/eap_server_gtc.c
	${COMMON_SRC_BASE}/eap_server/eap_server_peap.c
	${COMMON_SRC_BASE}/eap_server/eap_server_ttls.c
	${COMMON_SRC_BASE}/eap_server/eap_server_mschapv2.c
	${COMMON_SRC_BASE}/eap_server/eap_server_wsc.c
	${COMMON_SRC_BASE}/eap_server/eap_server_identity.c
	${COMMON_SRC_BASE}/eap_server/eap_server_methods.c
	${COMMON_SRC_BASE}/eapol_auth/eapol_auth_sm.c
	${COMMON_SRC_BASE}/eap_server/eap_server_fast.c
	${COMMON_SRC_BASE}/eap_server/eap_server_sim.c
	${COMMON_SRC_BASE}/eap_server/eap_server_aka.c
	${COMMON_SRC_BASE}/eap_server/eap_sim_db.c
	${HOSTAPD_BASE}/ctrl_iface.c
)

zephyr_library_sources_ifndef(CONFIG_WPA_SUPP_CRYPTO
	${COMMON_SRC_BASE}/crypto/crypto_none.c
)

zephyr_library_compile_definitions_ifndef(CONFIG_WPA_SUPP_CRYPTO
	CONFIG_NO_WPA
	CONFIG_CRYPTO_INTERNAL
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_CRYPTO_MBEDTLS_PSA
	${MBEDTLS_PORT_BASE}/supp_psa_api.c
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_CRYPTO
	${COMMON_SRC_BASE}/common/wpa_common.c
	${COMMON_SRC_BASE}/rsn_supp/wpa.c
	${COMMON_SRC_BASE}/rsn_supp/wpa_ft.c
	${COMMON_SRC_BASE}/rsn_supp/preauth.c
	${COMMON_SRC_BASE}/rsn_supp/pmksa_cache.c
	${COMMON_SRC_BASE}/rsn_supp/wpa_ie.c

	#${COMMON_SRC_BASE}/crypto/crypto_mbedtls-bignum.c
	#${COMMON_SRC_BASE}/crypto/crypto_mbedtls-ec.c
	${COMMON_SRC_BASE}/crypto/crypto_mbedtls2.c
	${COMMON_SRC_BASE}/crypto/tls_mbedtls2.c
	${COMMON_SRC_BASE}/crypto/aes-wrap.c
	${COMMON_SRC_BASE}/crypto/aes-unwrap.c
	${COMMON_SRC_BASE}/crypto/rc4.c
	#${COMMON_SRC_BASE}/crypto/random.c
	${COMMON_SRC_BASE}/crypto/sha1-prf.c
	${COMMON_SRC_BASE}/crypto/sha256-prf.c
  # ${COMMON_SRC_BASE}/crypto/sha384-prf.c
	${COMMON_SRC_BASE}/crypto/aes-siv.c
)

zephyr_library_sources_ifdef(CONFIG_KSDK_MBEDTLS
	${MBEDTLS_PORT_BASE}/ksdk/ksdk_mbedtls.c
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_WPA3
	${COMMON_SRC_BASE}/common/sae.c
	${COMMON_SRC_BASE}/common/dragonfly.c

	${COMMON_SRC_BASE}/crypto/dh_groups.c
	${COMMON_SRC_BASE}/crypto/sha256-kdf.c
	${COMMON_SRC_BASE}/crypto/sha384-kdf.c
	${COMMON_SRC_BASE}/crypto/sha512-kdf.c
)

zephyr_library_include_directories_ifdef(CONFIG_WPA_SUPP_CRYPTO
	${CMAKE_SOURCE_DIR}
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_P2P
	${WPA_SUPPLICANT_BASE}/p2p_supplicant.c
	${WPA_SUPPLICANT_BASE}/p2p_supplicant_sd.c
	${COMMON_SRC_BASE}/p2p/p2p.c
	${COMMON_SRC_BASE}/p2p/p2p_utils.c
	${COMMON_SRC_BASE}/p2p/p2p_parse.c
	${COMMON_SRC_BASE}/p2p/p2p_build.c
	${COMMON_SRC_BASE}/p2p/p2p_go_neg.c
	${COMMON_SRC_BASE}/p2p/p2p_sd.c
	${COMMON_SRC_BASE}/p2p/p2p_pd.c
	${COMMON_SRC_BASE}/p2p/p2p_invitation.c
	${COMMON_SRC_BASE}/p2p/p2p_dev_disc.c
	${COMMON_SRC_BASE}/p2p/p2p_group.c
	${COMMON_SRC_BASE}/ap/p2p_hostapd.c
	${COMMON_SRC_BASE}/common/gas.c
	${WPA_SUPPLICANT_BASE}/gas_query.c
	${WPA_SUPPLICANT_BASE}/offchannel.c
)
zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_WPS
	${WPA_SUPPLICANT_BASE}/wps_supplicant.c
	${COMMON_SRC_BASE}/utils/uuid.c
	${COMMON_SRC_BASE}/eap_peer/eap_wsc.c
	${COMMON_SRC_BASE}/eap_common/eap_wsc_common.c
	${COMMON_SRC_BASE}/wps/wps.c
	${COMMON_SRC_BASE}/ap/wps_hostapd.c
	${COMMON_SRC_BASE}/wps/wps_common.c
	${COMMON_SRC_BASE}/wps/wps_attr_parse.c
	${COMMON_SRC_BASE}/wps/wps_attr_build.c
	${COMMON_SRC_BASE}/wps/wps_attr_process.c
	${COMMON_SRC_BASE}/wps/wps_dev_attr.c
	${COMMON_SRC_BASE}/wps/wps_enrollee.c
	${COMMON_SRC_BASE}/wps/wps_registrar.c
	${COMMON_SRC_BASE}/eapol_supp/eapol_supp_sm.c
	${COMMON_SRC_BASE}/eap_peer/eap.c
	${COMMON_SRC_BASE}/eap_peer/eap_gtc.c
	${COMMON_SRC_BASE}/eap_peer/eap_methods.c
	${COMMON_SRC_BASE}/eap_common/eap_common.c
	${COMMON_SRC_BASE}/crypto/dh_groups.c
        #${COMMON_SRC_BASE}/crypto/dh_group5.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WPA_SUPP_P2P
	CONFIG_P2P
	CONFIG_GAS
	CONFIG_OFFCHANNEL
)

zephyr_library_include_directories_ifdef(CONFIG_WPA_SUPP_CRYPTO_ENTERPRISE
	${COMMON_SRC_BASE}/eap_peer
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_CRYPTO_ENTERPRISE
	${COMMON_SRC_BASE}/eap_peer/eap_tls.c
	${COMMON_SRC_BASE}/eap_peer/eap_tls_common.c

	${COMMON_SRC_BASE}/eapol_supp/eapol_supp_sm.c
	${COMMON_SRC_BASE}/eap_peer/eap.c
	${COMMON_SRC_BASE}/eap_peer/eap_methods.c
	${COMMON_SRC_BASE}/eap_common/eap_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_peap.c
	${COMMON_SRC_BASE}/eap_common/eap_peap_common.c
	${COMMON_SRC_BASE}/eap_peer/eap_ttls.c
	${COMMON_SRC_BASE}/eap_peer/eap_md5.c
	${COMMON_SRC_BASE}/eap_peer/eap_mschapv2.c
	${COMMON_SRC_BASE}/eap_common/chap.c
	${COMMON_SRC_BASE}/eap_peer/mschapv2.c
	${COMMON_SRC_BASE}/eap_peer/eap_leap.c

	${COMMON_SRC_BASE}/eap_peer/eap_psk.c
	${COMMON_SRC_BASE}/eap_common/eap_psk_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_fast.c
	${COMMON_SRC_BASE}/eap_peer/eap_fast_pac.c
	${COMMON_SRC_BASE}/eap_common/eap_fast_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_pax.c
	${COMMON_SRC_BASE}/eap_common/eap_pax_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_sake.c
	${COMMON_SRC_BASE}/eap_common/eap_sake_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_gpsk.c
	${COMMON_SRC_BASE}/eap_common/eap_gpsk_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_pwd.c
	${COMMON_SRC_BASE}/eap_common/eap_pwd_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_eke.c
	${COMMON_SRC_BASE}/eap_common/eap_eke_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_ikev2.c
	${COMMON_SRC_BASE}/eap_peer/ikev2.c
	${COMMON_SRC_BASE}/eap_common/eap_ikev2_common.c
	${COMMON_SRC_BASE}/eap_common/ikev2_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_sim.c
	${COMMON_SRC_BASE}/eap_common/eap_sim_common.c

	${COMMON_SRC_BASE}/eap_peer/eap_aka.c

	# common
	# ${COMMON_SRC_BASE}/crypto/sha384-tlsprf.c
	# ${COMMON_SRC_BASE}/crypto/sha256-tlsprf.c
	${COMMON_SRC_BASE}/crypto/sha1-tlsprf.c
	#${COMMON_SRC_BASE}/crypto/sha1-tprf.c
	${COMMON_SRC_BASE}/crypto/sha1-internal.c
	${COMMON_SRC_BASE}/crypto/ms_funcs.c
	${COMMON_SRC_BASE}/crypto/aes-eax.c
	# MD4 removed from MbedTLS
	${COMMON_SRC_BASE}/crypto/md4-internal.c
	${COMMON_SRC_BASE}/crypto/aes-encblock.c
	${COMMON_SRC_BASE}/crypto/fips_prf_internal.c
	${COMMON_SRC_BASE}/crypto/milenage.c
)

zephyr_library_sources_ifdef(CONFIG_WPA_SUPP_DPP
	${WPA_SUPPLICANT_BASE}/interworking.c
	${WPA_SUPPLICANT_BASE}/dpp_supplicant.c
	${WPA_SUPPLICANT_BASE}/offchannel.c
	${WPA_SUPPLICANT_BASE}/gas_query.c

	${COMMON_SRC_BASE}/ap/dpp_hostapd.c
	${COMMON_SRC_BASE}/ap/gas_query_ap.c
	${COMMON_SRC_BASE}/ap/gas_serv.c

	${COMMON_SRC_BASE}/common/dpp_tcp.c
	${COMMON_SRC_BASE}/common/dpp.c
	${COMMON_SRC_BASE}/common/dpp_pkex.c
	${COMMON_SRC_BASE}/common/dpp_crypto.c
	${COMMON_SRC_BASE}/common/dpp_auth.c
	${COMMON_SRC_BASE}/common/dpp_reconfig.c
	${COMMON_SRC_BASE}/common/gas_server.c
	${COMMON_SRC_BASE}/common/gas.c
	${COMMON_SRC_BASE}/common/dpp_backup.c

	${COMMON_SRC_BASE}/utils/json.c
	${COMMON_SRC_BASE}/utils/ip_addr.c

	${COMMON_SRC_BASE}/tls/asn1.c
)

zephyr_library_link_libraries_ifdef(CONFIG_MBEDTLS mbedTLS)
endif()
