/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for common testing functions.
*/

/*! @{ */

#ifndef COMMON_TEST_H
#define COMMON_TEST_H

/*!
 * \brief Check if requested Handset is connected over QHS or not.
 * 
 * \param handset_bd_addr BT address of the device.
 * 
 * \return TRUE if requested Handset is connected over QHS.
 *         FALSE if it is not connected over QHS or NULL BT address supplied.
 */
bool appTestIsHandsetQhsConnectedAddr(const bdaddr* handset_bd_addr);

/*!
 * \brief Check if requested Handset's SCO is active or not.
 * 
 * \param handset_bd_addr BT address of the device.
 * 
 * \return TRUE if requested Handset's SCO is active.
 *         FALSE if its SCO is not active or NULL BT address supplied.
 */
bool appTestIsHandsetHfpScoActiveAddr(const bdaddr* handset_bd_addr);

/*!
 * \brief Check if requested Handset is connected.
 *
 * \param handset_bd_addr BT address of the device.
 *
 * \return TRUE if the requested Handset has at least one profile connected.
 *         FALSE if it has no connected profiles.
 */
bool appTestIsHandsetAddrConnected(const bdaddr* handset_bd_addr);

/*!
 * \brief Enable the common output chain feature if it's been compiled in
 */
void appTestEnableCommonChain(void);

/*!
 * \brief Disable the common output chain feature if it's been compiled in
 */
void appTestDisableCommonChain(void);

/*!
 * \brief Get current RSSI of device.
 *
 * \param tp_bdaddr typed BT address, specifying address and transport type.
 *
 * \return RSSI if the connection to the device exists, else zero
 */
int16 appTestGetRssiOfTpAddr(tp_bdaddr *tpaddr);

/*!
 * \brief Get RSSI of current BREDR connection.
 *
 * \return RSSI if a connection exists, else zero
 */
int16 appTestGetBredrRssiOfConnectedHandset(void);

/*!
 * \brief Get RSSI of current LE connection.
 *
 * \return RSSI if a connection exists, else zero
 */
int16 appTestGetLeRssiOfConnectedHandset(void);

#endif /* COMMON_TEST_H */

/*! @} */

