/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Map of user ps keys.
 
Add a ps key here if it is going to be used.
If there is possibility that a ps key would need to be populated during deployment
then it should also be added to subsys7_psflash.htf.
 
*/

#ifndef PS_KEY_MAP_H_
#define PS_KEY_MAP_H_


/*@{*/

/*  \brief Macro that applies offset of 100 to user ps keys 50 to 99.
    
    The second group of user ps key is not located
    immediately after the first group, hence the offset.
*/
#define PS_MAPPED_USER_KEY(key) (((key)<50)?(key):100+(key))

typedef enum
{
    /* \brief Reserved ps key, don't use*/
    PS_KEY_RESERVED = PS_MAPPED_USER_KEY(0),

    /* \brief HFP volumes
       HFP won't use this key anymore,
       but don't use it for other purposes to not affect upgrade
       from the previous ADKs.
    */
    PS_KEY_HFP_CONFIG = PS_MAPPED_USER_KEY(1),

    /* \brief Fixed role setting */
    PS_KEY_FIXED_ROLE = PS_MAPPED_USER_KEY(2),

    /* \brief Device Test Service enable */
    PS_KEY_DTS_ENABLE = PS_MAPPED_USER_KEY(3),

    /* \brief Reboot action ps key */
    PS_KEY_REBOOT_ACTION = PS_MAPPED_USER_KEY(4),

    /* \brief Fast Pair Model Id */
    PS_KEY_FAST_PAIR_MODEL_ID = PS_MAPPED_USER_KEY(5),

    /* \brief Fast Pair Scrambled ASPK */
    PS_KEY_FAST_PAIR_SCRAMBLED_ASPK = PS_MAPPED_USER_KEY(6),

    /* \brief Upgrade ps key */
    PS_KEY_UPGRADE = PS_MAPPED_USER_KEY(7),

    /* \brief GAA Model Id */
    PS_KEY_GAA_MODEL_ID = PS_MAPPED_USER_KEY(8),

    /* \brief User EQ selected bank and gains */
    PS_KEY_USER_EQ = PS_MAPPED_USER_KEY(9),

    /* \brief GAA OTA control */
    PS_KEY_GAA_OTA_CONTROL = PS_MAPPED_USER_KEY(10),

    PS_KEY_BATTERY_STATE_OF_CHARGE = PS_MAPPED_USER_KEY(11),

    /* \brief ANC session data */
    PS_KEY_ANC_SESSION_DATA = PS_MAPPED_USER_KEY(12),

    /* \brief PS key used to store ANC delta gain (in DB) between
     * ANC golden gain configuration and calibrated gain during
     * production test: FFA, FFB and FB
     */
     PS_KEY_ANC_FINE_GAIN_TUNE_KEY = PS_MAPPED_USER_KEY(13),

    /* \brief ps keys used to store per device data */
    PS_KEY_DEVICE_PS_KEY_FIRST = PS_MAPPED_USER_KEY(20),
    PS_KEY_DEVICE_PS_KEY_LAST = PS_MAPPED_USER_KEY(29),

    /* \brief Version of ps key layout */
    PS_KEY_DATA_VERSION = PS_MAPPED_USER_KEY(50),

    /* \brief Setting for testing AV Codec in test mode */
    PS_KEY_TEST_AV_CODEC = PS_MAPPED_USER_KEY(80),

    /* \brief Setting for testing HFP Codec in test mode */
    PS_KEY_TEST_HFP_CODEC = PS_MAPPED_USER_KEY(81),
} ps_key_map_t;

/*@}*/

#endif /* PS_KEY_MAP_H_ */
