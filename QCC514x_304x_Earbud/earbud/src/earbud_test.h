/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_test.h
\brief      Interface for specifc application testing functions
*/

#ifndef EARBUD_TEST_H
#define EARBUD_TEST_H

#include <bdaddr.h>
#include <earbud_sm.h>
#include <peer_find_role.h>
#include "aec_leakthrough.h"
#include <device_test_service.h>

#include <anc.h>
#include <device.h>
#include <hdma_utils.h>
#include <peer_pairing.h>

extern bool appTestHandsetPairingBlocked;
extern TaskData testTask;

bool appTestTransmitEnable(bool);

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void);

/*! \brief Sets the battery voltage to be returned by
    appBatteryGetVoltage().

    This test function also causes the battery module to read
    this new voltage level - which may be subjected to hysteresis.
 */
void appTestSetBatteryVoltage(uint16 new_level);

/*! \brief Unsets the test battery voltage.
 */
void appTestUnsetBatteryVoltage(void);

/*! \brief Sets the battery voltage to be used instead of ADC measurements.
      \param new_level battery voltage to be used.
 */
void appTestInjectBatteryTestVoltage(uint16 new_level);

/*! \brief Restart normal battery voltage measurements.
 */
void appTestRestartBatteryVoltageMeasurements(void);

/*! \brief Converts battery voltage to percent of battery charge.

    \param voltage_mv Battery voltage in mV.
    \return Percentage of battery charge.
*/
uint8 appTestConvertBatteryVoltageToPercentage(uint16 voltage_mv);

/*! \brief Return TRUE if the current battery operating region is battery_region_ok. */
bool appTestBatteryStateIsOk(void);

/*! \brief Return TRUE if the current battery operating region is battery_region_critical. */
bool appTestBatteryStateIsCritical(void);

/*! \brief Return TRUE if the current battery operating region is battery_region_unsafe. */
bool appTestBatteryStateIsUnsafe(void);

/*! \brief Return the number of milliseconds taken for the battery measurement
    filter to fully respond to a change in input battery voltage */
uint32 appTestBatteryFilterResponseTimeMs(void);

#ifdef HAVE_THERMISTOR
/*! \brief Returns the expected thermistor voltage at a specified temperature.
    \param temperature The specified temperature in degrees Celsius.
    \return The equivalent milli-voltage.
*/
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature);

/*! \brief Sets test temperature value to be returned back by
    appTestGetBatteryTemperature().
    \param new_temp The temperature in degrees Celsius.
*/
void appTestSetBatteryTemperature(int8 new_temp);

/*! \brief Unsets test temperature value and restarts normal monitoring.
*/
void appTestUnsetBatteryTemperature(void);

/*! \brief Sets the battery temperature to be used instead of ADC measurements.
    \param new_temp The temperature in degrees Celsius.
*/
void appTestInjectBatteryTestTemperature(int8 new_temp);

/*! \brief Restart normal battery temperature measurements.
*/
void appTestRestartTemperatureMeasurements(void);

/*! \brief Returns the current battery temperature
 */
int8 appTestGetBatteryTemperature(void);

#endif

/*! \brief Put Earbud into Handset Pairing mode
*/
void appTestPairHandset(void);

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void);

/*! \brief Delete Earbud peer pairing

    Attempts to delete pairing to earbud. Check the debug output if
    this test command fails, as it will report failure reason.

    \return TRUE if pairing was successfully removed.
            FALSE if there is no peer pairing, or still connected to the device.
*/
bool appTestDeletePeer(void);

/*! \brief Get the devices peer bluetooth address

    \param  peer_address    pointer to the peer paired address if found
    \return TRUE if the device is peer paired
            FALSE if device is not peer paired
*/
bool appTestGetPeerAddr(bdaddr *peer_address);


/*! \brief Read the current address from the connection library

    The address is not returned directly, call appTestGetLocalAddr
    which will populate an address and also indicate if the address
    was updated after the last call to this function.

    \note for test purposes, simply calling this function will
    cause the current bdaddr to be displayed when it is retrieved.
 */
void appTestReadLocalAddr(void);


/*! \brief retrieve the local bdaddr last read

    An update of the address is requested using appTestReadLocalAddr.
    Normal usage would be to call appTestReadLocalAddr() then call
    this function until it returns True.

    From pydbg you can allocate memory for the address and use it as below

        bd = apps1.fw.call.pnew("bdaddr")
        apps1.fw.call.appTestGetLocalAddr(bd.address)
        print(bd)

    \param[out] addr Pointer to location to store the address

    \return TRUE if the address was updated following call to
        appTestReadLocalAddr
 */
bool appTestGetLocalAddr(bdaddr *addr);


/*! \brief Return if Earbud is in a Pairing mode

    \return bool TRUE Earbud is in pairing mode
                 FALSE Earbud is not in pairing mode
*/
bool appTestIsPairingInProgress(void);

/*! \brief Initiate Earbud A2DP connection to the Handset

    \return bool TRUE if A2DP connection is initiated
                 FALSE if no handset is paired
*/
bool appTestHandsetA2dpConnect(void);

/*! \brief Stop the earbud automatically pairing with a handset

    Rules that permit pairing will be stopped while a block is in
    place.

    \param  block   Enable/Disable the block
*/
void appTestBlockAutoHandsetPairing(bool block);

/*! \brief Return if Earbud has a handset pairing

    \return TRUE Earbud is paired with at least one handset,
            FALSE otherwise
*/
bool appTestIsHandsetPaired(void);

/*! \brief Return if Earbud has an Handset A2DP connection

    \return bool TRUE Earbud has A2DP Handset connection
                 FALSE Earbud does not have A2DP Handset connection
*/
bool appTestIsHandsetA2dpConnected(void);

/*! \brief Return if Earbud has an Handset A2DP media connection

    \return bool TRUE Earbud has A2DP Handset media connection
                 FALSE Earbud does not have A2DP Handset connection
*/
bool appTestIsHandsetA2dpMediaConnected(void);

/*! \brief Return if Earbud is in A2DP streaming mode with the handset

    \return bool TRUE Earbud is in A2DP streaming mode
                     FALSE Earbud is not in A2DP streaming mode
*/
bool appTestIsHandsetA2dpStreaming(void);

/*! \brief Initiate Earbud AVRCP connection to the Handset

    \return bool TRUE if AVRCP connection is initiated
                 FALSE if no handset is paired
*/


bool appTestIsA2dpPlaying(void);

bool appTestHandsetAvrcpConnect(void);

/*! \brief Return if Earbud has an Handset AVRCP connection

    \return bool TRUE Earbud has AVRCP Handset connection
                 FALSE Earbud does not have AVRCP Handset connection
*/
bool appTestIsHandsetAvrcpConnected(void);

/*! \brief Initiate Earbud HFP connection to the Handset

    \return bool TRUE if HFP connection is initiated
                 FALSE if no handset is paired
*/
bool appTestHandsetHfpConnect(void);

/*! \brief Return if Earbud has an Handset HFP connection

    \return bool TRUE Earbud has HFP Handset connection
                 FALSE Earbud does not have HFP Handset connection
*/
bool appTestIsHandsetHfpConnected(void);

/*! \brief Return if Earbud has an Handset HFP SCO connection

    \return bool TRUE Earbud has HFP SCO Handset connection
                 FALSE Earbud does not have HFP SCO Handset connection
*/
bool appTestIsHandsetHfpScoActive(void);

/*! \brief Initiate Earbud HFP Voice Dial request to the Handset
*/
void appTestHandsetHfpVoiceDial(void);

/*! \brief Toggle Microphone mute state on HFP SCO conenction to handset
*/
void appTestHandsetHfpMuteToggle(void);

/*! \brief Transfer HFP voice to the Handset
*/
void appTestHandsetHfpVoiceTransferToAg(void);

/*! \brief Transfer HFP voice to the Earbud
*/
void appTestHandsetHfpVoiceTransferToHeadset(void);

/*! \brief Accept incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallAccept(void);

/*! \brief Reject incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallReject(void);

/*! \brief End the current call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallHangup(void);

/*! \brief Cycle to the next call
*/
void appTestHandsetVoiceCallCycle(void);

/*! \brief Join two calls
*/
void appTestHandsetVoiceCallJoinCalls(void);

/*! \brief Join two calls and hang up
*/
void appTestHandsetVoiceCallJoinCallsAndHangUp(void);

/*! \brief Initiated last number redial

    \return bool TRUE if last number redial was initiated
                 FALSE if HFP is not connected
*/
bool appTestHandsetHfpCallLastDialed(void);

/*! \brief Start decreasing the HFP volume
*/
void appTestHandsetHfpVolumeDownStart(void);

/*! \brief Start increasing the HFP volume
*/
void appTestHandsetHfpVolumeUpStart(void);

/*! \brief Stop increasing or decreasing HFP volume
*/
void appTestHandsetHfpVolumeStop(void);

/*! \brief Set the Hfp Sco volume

    \return bool TRUE if the volume set request was initiated
                 FALSE if HFP is not in a call
*/
bool appTestHandsetHfpSetScoVolume(uint8 volume);

/*! \brief Accept incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallAccept(void);

/*! \brief Reject incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallReject(void);

/*! \brief End the current call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallHangup(void);

/*! \brief Get microphone mute status

    \return bool TRUE if microphone muted,
                 FALSE if not muted
*/
bool appTestIsHandsetHfpMuted(void);

/*! \brief Check if call is in progress

    \return bool TRUE if call in progress,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCall(void);

/*! \brief Check if incoming call

    \return bool TRUE if call incoming,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallIncoming(void);

/*! \brief Check if outgoing call

    \return bool TRUE if call outgoing,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallOutgoing(void);

/*! \brief Return if Earbud has an ACL connection to the Handset

    It does not indicate if the handset is usable, with profiles
    connected. Use appTestIsHandsetConnected or
    appTestIsHandsetFullyConnected.

    \return bool TRUE Earbud has an ACL connection
                 FALSE Earbud does not have an ACL connection to the Handset
*/
bool appTestIsHandsetAclConnected(void);

/*! \brief Return if Earbud has a profile connection to the Handset

    This can be HFP, A2DP or AVRCP. It does not indicate if there
    is an ACL connection.

    \return bool TRUE Earbud has a connection to the Handset
                 FALSE Earbud does not have a connection to the Handset
*/
bool appTestIsHandsetConnected(void);

/*! \brief Is the handset completely connected (all profiles)

    This function checks whether the handset device is connected
    completely. Unlike appTestIsHandsetConnected() this function
    checks that all the handset profiles required are connected.

    \return TRUE if there is a handset, all required profiles
        are connected, and we have not started disconnecting.
        FALSE in all other cases.
 */
bool appTestIsHandsetFullyConnected(void);

/*! \brief Initiate Earbud A2DP connection to the the Peer

    \return bool TRUE if A2DP connection is initiated
                 FALSE if no Peer is paired
*/
bool appTestPeerA2dpConnect(void);

/*! \brief Return if Earbud has a Peer A2DP connection

    \return bool TRUE Earbud has A2DP Peer connection
                 FALSE Earbud does not have A2DP Peer connection
*/
bool appTestIsPeerA2dpConnected(void);

/*! \brief Check if Earbud is in A2DP streaming mode with peer Earbud

    \return TRUE if A2DP streaming to peer device
*/
bool appTestIsPeerA2dpStreaming(void);

/*! \brief Initiate Earbud AVRCP connection to the the Peer

    \return bool TRUE if AVRCP connection is initiated
                 FALSE if no Peer is paired
*/
bool appTestPeerAvrcpConnect(void);

/*! \brief Return if Earbud has a Peer AVRCP connection

    \return bool TRUE Earbud has AVRCP Peer connection
                 FALSE Earbud does not have AVRCP Peer connection
*/
bool appTestIsPeerAvrcpConnected(void);

#ifdef INCLUDE_EXTRA_TESTS
/*! \brief Simulates peer link loss */
void appTestPeerDisconnect(void);
#endif

/*! \brief Send the AV toggle play/pause command
*/
void appTestAvTogglePlayPause(void);

#ifdef INCLUDE_HDMA
/*! \brief Start dynamic handover procedure.
    \return TRUE if handover was initiated, otherwise FALSE.
*/
bool earbudTest_DynamicHandover(void);
#endif

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void);

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void);

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void);

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void);

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void);

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void);

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void);

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void);

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void);

/*! \brief Send the Avrcp volume change command to the Handset

    \param step Step change to apply to volume

    \return bool TRUE volume step change sent
                 FALSE volume step change was not sent
*/
bool appTestAvVolumeChange(int8 step);

/*! \brief Send the Avrcp pause command to the Handset

    \param volume   New volume level to set (0-127).
*/
void appTestAvVolumeSet(uint8 volume);

/*! \brief Send the Media Control pause command to the Handset
*/
void appTestMediaControlPause(void);

/*! \brief Send Media Control pause command to pause all the audio sources
*/
void appTestMediaControlPauseAll(void);

/*! \brief Send the Media Control play command to the Handset
*/
void appTestMediaControlPlay(void);

/*! \brief Send the Media Control stop command to the Handset
*/
void appTestMediaControlStop(void);

/*! \brief Send the Media Control forward command to the Handset
*/
void appTestMediaControlForward(void);

/*! \brief Send the Media Control backward command to the Handset
*/
void appTestMediaControlBackward(void);

/*! \brief Send the Media Control play/pause toggle command
*/
void appTestMediaControlTogglePlayPause(void);

/*! \brief Allow tests to control whether the earbud will enter dormant.

    If an earbud enters dormant, cannot be woken by a test.

    \note Even if dormant mode is enabled, the application may not
        enter dormant. Dormant won't be used if the application is
        busy, or connected to a charger - both of which are quite
        likely for an application under test.

    \param  enable  Use FALSE to disable dormant, or TRUE to enable.
*/
void appTestPowerAllowDormant(bool enable);

/*! \brief Simple test function to get the DSP clock speed in MegaHertz during the Active mode.
    \note Make sure DSP is up and running before using this test function on pydbg window to avoid panic.
    \returns DSP clock speed in MegaHertz during the Active mode
*/
uint32 EarbudTest_DspClockSpeedInActiveMode(void);

/*! \brief Generate event that Earbud is now in the case. */
void appTestPhyStateInCaseEvent(void);

/*! \brief Generate event that Earbud is now out of the case. */
void appTestPhyStateOutOfCaseEvent(void);

/*! \brief Generate event that Earbud is now in ear. */
void appTestPhyStateInEarEvent(void);

/*! \brief Generate event that Earbud is now out of the ear. */
void appTestPhyStateOutOfEarEvent(void);

/*! \brief Generate event that Earbud is now moving */
void appTestPhyStateMotionEvent(void);

/*! \brief Generate event that Earbud is now not moving. */
void appTestPhyStateNotInMotionEvent(void);

/*! \brief Generate event that Earbud is now (going) off. */
void appTestPhyStateOffEvent(void);

/*! \brief Return TRUE if the earbud is in the ear. */
bool appTestPhyStateIsInEar(void);

/*! \brief Return TRUE if the earbud is out of the ear, but not in the case. */
bool appTestPhyStateOutOfEar(void);

/*! \brief Return TRUE if the earbud is in the case. */
bool appTestPhyStateIsInCase(void);

/*! \brief Reset an Earbud to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void);

/*! \brief Determine if a reset has happened

    \return TRUE if a reset has happened since the last call to the function
*/
bool appTestResetHappened(void);

/*! \brief Connect to default handset. */
void appTestConnectHandset(void);

/*! \brief Connect the A2DP media channel to the handset
    \return True is request sent, else false
 */
bool appTestConnectHandsetA2dpMedia(void);

/*! \brief Send A2dp Media Start request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaStart(void);

/*! \brief Send A2dp Media Suspend request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaSuspend(void);

/*! \brief Send A2dp Media Close request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaClose(void);

/*! \brief Send A2dp Abort stream request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpAbort(void);

/*! \brief Send A2dp Reconfigure stream request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaReconfigure(void);

/*! \brief  Check if peer synchronisation was successful

    \returns TRUE if we are synchronised with the peer.
*/
bool appTestIsPeerSyncComplete(void);

/*! \brief Power off.
    \return TRUE if the device can power off - the device will drop connections then power off.
            FALSE if the device cannot currently power off.
*/
bool appTestPowerOff(void);

/*! \brief Determine if the earbud has a paired peer earbud.
    \return bool TRUE the earbud has a paired peer, FALSE the earbud has no paired peer.
*/
bool appTestIsPeerPaired(void);

/*! \brief Determine if the all licenses are correct.
    \return bool TRUE the licenses are correct, FALSE if not.
*/
bool appTestLicenseCheck(void);

/*! \brief Control 2nd earbud connecting to handset after TWS+ pairing.
    \param bool TRUE enable 2nd earbud connect to handset.
                FALSE disable 2nd earbud connect to handset, handset must connect.
    \note This API is deprecated, the feature is no longer supported.
 */
void appTestConnectAfterPairing(bool enable);

/*! \brief Asks the connection library about the sco forwarding link.

    The result is reported as debug.
 */
void appTestScoFwdLinkStatus(void);

/* \brief Enable or disable randomised dropping of SCO forwarding packets

    This function uses random numbers to stop transmissio of SCO forwarding
    packets, so causing error mechanisms to be used on the receiving side.
    Packet Loss Concealment (PLC) and possibly audio muting or disconnection.

    There are two modes.
    - set a percentage chance of a packet being dropped,
        if the previous packet was dropped
    - set the number of consecutive packets to drop every time.
        set this using a negative value for multiple_packets

    \param percentage        The random percentage of packets to not transmit
    \param multiple_packets  A negative number indicates the number of consecutive
                             packets to drop. \br 0, or a positive number indicates the
                             percentage chance a packet should drop after the last
                             packet was dropped. */
bool appTestScoFwdForceDroppedPackets(unsigned percentage_to_drop, int multiple_packets);

/*! \brief Requests that the L2CAP link used for SCO forwarding is connected.

    \returns FALSE if the device is not paired to another earbud, TRUE otherwise
 */
bool appTestScoFwdConnect(void);

/*! \brief Requests that the L2CAP link used for SCO forwarding is disconnected.

    \returns TRUE
 */
bool appTestScoFwdDisconnect(void);

/*! \brief Selects the local microphone for MIC forwarding

    Preconditions
    - in an HFP call
    - MIC forwarding enabled in the build
    - function called on the device connected to the handset

    \returns TRUE if the preconditions are met, FALSE otherwise

    \note will return TRUE even if the local MIC is currently selected
 */
bool appTestMicFwdLocalMic(void);

/*! \brief Selects the remote (forwarded) microphone for MIC forwarding

    Preconditions
    - in an HFP call
    - MIC forwarding enabled in the build
    - function called on the device connected to the handset

    \returns TRUE if the preconditions are met, FALSE otherwise

    \note will return TRUE even if the remote MIC is currently selected
 */
bool appTestMicFwdRemoteMic(void);

/*! \brief configure the Pts device as a peer device */
bool appTestPtsSetAsPeer(void);

/*! \brief Are we running in PTS test mode */
bool appTestIsPtsMode(void);

/*! \brief Set or clear PTS test mode */
bool appTestSetPtsMode(bool enable);

/*! \brief Determine if appConfigScoForwardingEnabled is TRUE
    \return bool Return value of appConfigScoForwardingEnabled.
*/
bool appTestIsScoFwdIncluded(void);

/*! \brief Determine if appConfigMicForwardingEnabled is TRUE.
    \return bool Return value of appConfigMicForwardingEnabled.
*/
bool appTestIsMicFwdIncluded(void);

/*! Handler for connection library messages
    This function is called to handle connection library messages used for testing.
    If a message is processed then the function returns TRUE.

    \param  id              Identifier of the connection library message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool appTestHandleConnectionLibraryMessages(MessageId id, Message message,
                                                   bool already_handled);

/*! Set flag so that DFU mode is entered when the device next goes "in case"

    This test function injects the Logical Input (i.e. mimics the UI button press event)
    corresponding to "enter DFU mode when the Earbuds are next placed in the case".

    The DFU mode will be requested only if the peer earbud is connected and
    the earbud is out of the case.

    \param unused - kept for backwards compatibility

    \return TRUE if DFU mode was requested, FALSE otherwise.
*/
bool appTestEnterDfuWhenEnteringCase(bool unused);

/*! Has the application infrastructure initialised

    When starting, the application initialises the modules it uses.
    This function checks if the sequence of module initialisation has completed

    \note When this function returns TRUE it does not indicate that the application
    is fully initialised. That would depend on the application state, and the
    status of the device.

    \returns TRUE if initialisation completed
 */
bool appTestIsInitialisationCompleted(void);

/*! Determine if the Earbud is currently primary.

    \return TRUE Earbud is Primary or Acting Primary. FALSE in all
             other cases

    \note A return value of FALSE for one Earbud DOES NOT imply it is
        secondary. See \ref appTestIsSecondary
*/
bool appTestIsPrimary(void);

/*! Determine if the Earbud is the Right Earbud.

    \return TRUE Earbud is the Right Earbud. FALSE Earbud is the Left Earbud
*/
bool appTestIsRight(void);

/*! \brief Get the major, minor and config versions from the upgrade library.
    \note   Prints the result to DEBUG_LOG as "major.minor.config"
    \note   Cannot return uint64 to pydbg so having to return low byte of major and config
    \return (major << 24) | (minor << 8) | (config & 0xFF)
 */
uint32 appTestUpgradeGetVersion(void);

typedef enum
{
    app_test_topology_role_none,
    app_test_topology_role_any_primary,
    app_test_topology_role_primary,
    app_test_topology_role_acting_primary,
    app_test_topology_role_secondary,
} app_test_topology_role_enum_t;

/*! Check if the earbud topology is in the specified role

    The roles are specified in the test API. If toplogy
    is modified, the test code may need to change but uses in
    tests should not.

    \return TRUE The topology is in the specified role
*/
bool appTestIsTopologyRole(app_test_topology_role_enum_t role);


/*! Test function to report if the Topology is in a stable state.

    The intention is to use this API between tests. Other than
    the topology being in the No Role state, the implementation
    is not defined here. */
bool appTestIsTopologyIdle(void);


/*! Test function to report if the Topology is running.

    \return TRUE if topology is active, FALSE otherwise.
 */
bool appTestIsTopologyRunning(void);


/*! Check if the application state machine is in the specified role

    The states are defined in earbud_sm.h, and can be accessed from
    python by name - example
    \code
      apps1.fw.env.enums["appState"]["APP_STATE_IN_CASE_DFU"])
    \endcode

    \return TRUE The application state machine is in the specified state
*/
bool appTestIsApplicationState(appState checked_state);

/*! Check if peer find role is running

    \return TRUE if peer find role is looking for a peer, FALSE
            otherwise.
 */
bool appTestIsPeerFindRoleRunning(void);

/*! Check if peer pair is active

    \return TRUE if peer pair is trying to pair, FALSE
            otherwise.
 */
bool appTestIsPeerPairLeRunning(void);

#ifdef INCLUDE_EXTRA_TESTS
/*! Report the contents of the Device Database. */
void EarbudTest_DeviceDatabaseReport(void);
#endif

void EarbudTest_ConnectHandset(void);
void EarbudTest_DisconnectHandset(void);
void EarbudTest_DisconnectHandsetExcept(uint32 profiles);

/*! Report if the primary bluetooth address originated with this board

    When devices are paired one of the two addresses is selected as the
    primary address. This process is effectively random. This function
    indicates if the address chosen is that programmed into this device.

    The log will contain the actual primary bluetooth address.

    To find out if the device is using the primary address, use the
    test command appTestIsPrimary().

    The Bluetooth Address can be retrieved by non test commands.
    Depending on the test automation it may not be possible to use that
    technique (shown here)

    \code
        >>> prim_addr = apps1.fw.call.new("bdaddr")
        >>> apps1.fw.call.appDeviceGetPrimaryBdAddr(prim_addr.address)
        True
        >>> prim_addr
         |-bdaddr  : struct
         |   |-uint32 lap : 0x0000ff0d
         |   |-uint8 uap : 0x5b
         |   |-uint16 nap : 0x0002
        >>>
    \endcode

    \returns TRUE if the primary address is that originally assigned to
        this board, FALSE otherwise
        FALSE
*/
bool appTestPrimaryAddressIsFromThisBoard(void);

/*! \brief Override the score used in role selection.

    Setting a non-zero value will use that value rather than the
    calculated one for future role selection decisions.

    Setting a value of 0, will cancel the override and revert to
    using the calculated score.

    If roles have already been selected, this will cause a re-selection.

    \param score Score to use for role selection decisions.
*/
void EarbudTest_PeerFindRoleOverrideScore(uint16 score);

/*! \brief Check to determine whether peer signalling is connected.

    Used to determine whether a peer device is connected

    \returns TRUE if peer signalling is connected, else FALSE.
*/
bool EarbudTest_IsPeerSignallingConnected(void);

/*! \brief Check to determine whether peer signalling is disconnected.

    Used to determine whether a peer device is disconnected, note this is not necessarily the same as
    !EarbudTest_IsPeerSignallingConnected() due to intermediate peer signalling states

    \returns TRUE if peer signalling is disconnected, else FALSE.
*/
bool EarbudTest_IsPeerSignallingDisconnected(void);

#ifdef INCLUDE_HDMA_RSSI_EVENT
/*! \brief Get RSSI threshold values.

    \returns pointer of the structure hdma_thresholds_t which holds data.
*/
const hdma_thresholds_t *appTestGetRSSIthreshold(void);
#endif

#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
/*! \brief Get MIC threshold values.

    \returns pointer of the structure hdma_thresholds_t which holds data.
*/
const hdma_thresholds_t *appTestGetMICthreshold(void);
#endif

/*! Resets the PSKEY used to track the state of an upgrade

    \return TRUE if the store was cleared
*/
bool appTestUpgradeResetState(void);

#ifdef INCLUDE_DFU
/*! Puts the Earbud into the In Case DFU state ready to test a DFU session

    This API also puts the peer Earbud in DFU mode, though will not put it
    In Case.
*/
void EarbudTest_EnterInCaseDfu(void);
#endif

/*! Set a test number to be displayed through appTestWriteMarker

    This is intended for use in test output. Set test_number to
    zero to not display.

    \param  test_number The testcase number to use in output

    \return The test number. This can give output on the pydbg console
        as well as the output.
*/
uint16 appTestSetTestNumber(uint16 test_number);


/*! Set a test iteration to be displayed through appTestWriteMarker

    This is intended for use in test output. Set test_iteration to
    zero to not display.

    \param  test_iteration The test iteration number to use in output

    \return The test iteration. This can give output on the pydbg console
        as well as the output.
*/
uint16 appTestSetTestIteration(uint16 test_iteration);


/*! Write a marker to output

    This is intended for use in test output

    \param  marker The value to include in the marker. A marker of 0 will
        write details of the testcase and iteration set through
        appTestSetTestNumber() and appTestSetTestIteration().

    \return The marker value. This can give output on the pydbg console
        as well as the output.
*/
uint16 appTestWriteMarker(uint16 marker);

void appTestCvcPassthrough(void);

/*! \brief  Set the ANC Enable state in the Earbud. */
void EarbudTest_SetAncEnable(void);

/*! \brief  Set the ANC Disable state in the Earbud. */
void EarbudTest_SetAncDisable(void);

/*! \brief  Set the ANC Enable/Disable state in both Earbuds. */
void EarbudTest_SetAncToggleOnOff(void);

/*! \brief  Set the ANC next mode in both Earbuds. */
void EarbudTest_AncToggleDiagnostic(void);

/*! \brief  Write ANC fine gain to PS. */
bool EarbudTest_AncWriteFineGain(audio_anc_path_id gain_path, uint8 gain);


/*! \brief  Set the ANC mode in both Earbuds.
    \param  mode Mode need to be given by user to set the dedicated mode.
*/
void EarbudTest_SetAncMode(anc_mode_t mode);

void EarbudTest_SetNextAncMode(void);
/*! \brief  To get the current ANC State in both Earbuds.
    \return bool TRUE if ANC enabled
                 else FALSE
*/

/*! \brief  Set the ANC next mode in both Earbuds. */
void EarbudTest_SetAncNextMode(void);

/*! \brief  Set the ANC toggle way config 1 in both Earbuds. */
void EarbudTest_SetAncToggleConfig1(uint8 config);

/*! \brief  Set the ANC toggle way config 2 in both Earbuds. */
void EarbudTest_SetAncToggleConfig2(uint8 config);

/*! \brief  Set the ANC toggle way config 3 in both Earbuds. */
void EarbudTest_SetAncToggleConfig3(uint8 config);

/*! \brief  Set the ANC config during standalone in both Earbuds. */
void EarbudTest_SetAncConfigDuringStandalone(uint8 config);

/*! \brief  Set the ANC config during playback in both Earbuds. */
void EarbudTest_SetAncConfigDuringPlayback(uint8 config);

/*! \brief  Set the ANC config during sco in both Earbuds. */
void EarbudTest_SetAncConfigDuringSco(uint8 config);

/*! \brief  Set the ANC config during VA in both Earbuds. */
void EarbudTest_SetAncConfigDuringVa(uint8 config);

/*! \brief  Set the ANC demo mode in both Earbuds. */
void EarbudTest_SetAncDemoState(bool demo_state);

/*! \brief  Toggle ANC Adaptivity in both Earbuds. */
void EarbudTest_ToggleAncAdaptivity(void);

/*! \brief  Toggle ANC config in both Earbuds. */
void EarbudTest_ToggleAncConfig(void);

/*! \brief  To get the current ANC State in both Earbuds.
    \return bool TRUE if ANC enabled
                 else FALSE
*/
bool EarbudTest_GetAncstate(void);

/*! \brief  To get the current ANC mode in both Earbuds.
    \return Return the current ANC mode.
*/
anc_mode_t EarbudTest_GetAncMode(void);

/*! \brief  To get the current ANC toggle config 1 in both Earbuds.
    \return Return the current ANC toggle config 1.
*/
uint8 EarbudTest_GetAncToggleConfig1(void);

/*! \brief  To get the current ANC toggle config 2 in both Earbuds.
    \return Return the current ANC toggle config 2.
*/
uint8 EarbudTest_GetAncToggleConfig2(void);

/*! \brief  To get the current ANC toggle config 3 in both Earbuds.
    \return Return the current ANC toggle config 3.
*/
uint8 EarbudTest_GetAncToggleConfig3(void);

/*! \brief  To get the current ANC config during standalone in both Earbuds.
    \return Return the current ANC config during standalone.
*/
uint8 EarbudTest_GetAncConfigDuringStandalone(void);

/*! \brief  To get the current ANC config during playback in both Earbuds.
    \return Return the current ANC config during playback.
*/
uint8 EarbudTest_GetAncConfigDuringPlayback(void);

/*! \brief  To get the current ANC config during sco in both Earbuds.
    \return Return the current ANC config during sco.
*/
uint8 EarbudTest_GetAncConfigDuringSco(void);

/*! \brief  To get the current ANC config during VA in both Earbuds.
    \return Return the current ANC config during VA.
*/
uint8 EarbudTest_GetAncConfigDuringVa(void);

/*! \brief  To get the current ANC Demo state both Earbuds.
    \return Return the current ANC Demo state.
*/
bool EarbudTest_GetAncDemoState(void);

/*! \brief  To get the current ANC Adaptivity status both Earbuds.
    \return Return the current ANC Adaptivity status.
*/
bool EarbudTest_GetAncAdaptivity(void);

/*! \brief  Start ANC tuning . */
void EarbudTest_StartAncTuning(void);

/*! \brief  Stop ANC tuning  */
void EarbudTest_StopAncTuning(void);

/*! \brief  Start Adaptive ANC tuning . */
void EarbudTest_StartAdaptiveAncTuning(void);

/*! \brief  Stop Adaptive ANC tuning  */
void EarbudTest_StopAdaptiveAncTuning(void);

/*! \brief  To inject ANC GAIA command for obtaining ANC state
*/
void EarbudTest_GAIACommandGetAncState(void);

/*! \brief  To inject ANC GAIA command for enabling ANC
*/
void EarbudTest_GAIACommandSetAncEnable(void);

/*! \brief  To inject ANC GAIA command for disabling ANC
*/
void EarbudTest_GAIACommandSetAncDisable(void);

/*! \brief  To inject ANC GAIA command to obtain number of ANC modes supported
*/
void EarbudTest_GAIACommandGetAncNumOfModes(void);

/*! \brief  To inject ANC GAIA command to change ANC Mode
*/
void EarbudTest_GAIACommandSetAncMode(uint8 mode);

/*! \brief  To inject ANC GAIA command to obtain ANC Leakthrough gain for current mode
*/
void EarbudTest_GAIACommandGetAncLeakthroughGain(void);

/*! \brief  To inject ANC GAIA command to set ANC lekathrough gain for current mode
*/
void EarbudTest_GAIACommandSetAncLeakthroughGain(uint8 gain);

/*! \brief Sets Leakthrough gain of ANC H/W for current mode
    \param gain Leakthrough gain to be set
*/
void EarbudTest_SetAncLeakthroughGain(uint8 gain);

/*! \brief Obtains Leakthrough gain of ANC H/W for current gain
    \return uint8 gain Leakthrough gain
*/
uint8 EarbudTest_GetAncLeakthroughGain(void);

/*! \brief  Enable leak-through in the Earbud. */
void EarbudTest_EnableLeakthrough(void);

/*! \brief  Disable leak-through in the Earbud. */
void EarbudTest_DisableLeakthrough(void);

/*! \brief  Toggle leak-through state (Enable or On/Disable or Off) in the Earbud. */
void EarbudTest_ToggleLeakthroughOnOff(void);

/*! \brief  Set the leak-through mode in the Earbud.
    \param  leakthrough_mode Mode need to be given by user to set the dedicated mode. valid values are LEAKTHROUGH_MODE_1,
    LEAKTHROUGH_MODE_2 and LEAKTHROUGH_MODE_3
*/
void EarbudTest_SetLeakthroughMode(leakthrough_mode_t leakthrough_mode);

/*! \brief  Set the next leak-through mode in the Earbud. */
void EarbudTest_SetNextLeakthroughMode(void);

/*! \brief Get the leak-through mode in the Earbud.
    \return Return the current leak-through mode.
*/
leakthrough_mode_t EarbudTest_GetLeakthroughMode(void);

/*! \brief Get the leak-through status (enabled or disabled) in the Earbud.
    \return bool TRUE if leak-through is enabled
                    else FALSE
*/
bool EarbudTest_IsLeakthroughEnabled(void);

/*! \brief Override the A2DP audio latency with the value provided.
    \param latency_ms The new latency in ms.
    \return TRUE if the latency override was set.
    \note Setting to zero will disable the override.
    \note The new latency will not be applied to an already active A2DP stream.
          The stream must be stopped and restarted.
    \note Requires INCLUDE_LATENCY_MANAGER to be defined.
*/
bool EarbudTest_OverrideA2dpLatency(uint32 latency_ms);

/*! \brief Set the device to have a fixed role
*/
void appTestSetFixedRole(peer_find_role_fixed_role_t role);

#ifdef INCLUDE_FAST_PAIR
/*! \brief This test API is used to configure fast pair model ID
    \param fast_pair_model_id
    \return void.
    \note The input paramater is of type uint32
    \note Eg: apps1.fw.call.appTestSetFPModelID(0x00D0082F)
    \note The fast pair model ID is written to USR5 PS Key using PsStore API
*/
void appTestSetFPModelID(uint32 fast_pair_model_id);

/*! \brief This test API is used to configure scrambled ASPK
    \param dword1
    \param dword2
    \param dword3
    \param dword4
    \param dword5
    \param dword6
    \param dword7
    \param dword8
    \return void.
    \note All input paramater is of type uint32
    \note SCRAMBLED_ASPK {0x427C, 0x1EE8, 0x939A, 0xFF17, 0x555E, 0x730B, 0x3B4C, 0x67E4,
                          0xD0E3, 0x68E0, 0x1D6B, 0x9A25, 0x799F, 0x4ED8, 0xAA6E, 0x33C5}
    \note For the above aspk use the below pydebug command to program aspk
    \note Eg: apps1.fw.call.appTestSetFPASPK(0x1EE8427C, 0xFF17939A, 0x730B555E, 0x67E43B4C,
                                             0x68E0D0E3, 0x9A251D6B, 0x4ED8799F, 0x33C5AA6E)
    \note The scrambled aspk is written to USR6 PS Key using PsStore API
*/
void appTestSetFPASPK(uint32 dword1, uint32 dword2, uint32 dword3, uint32 dword4,
                      uint32 dword5, uint32 dword6, uint32 dword7, uint32 dword8);
#endif /* INCLUDE_FAST_PAIR */

/*! \brief To get current HFP codec
    The return value follows this enum
    typedef enum
    {
        NO_SCO,
        SCO_NB,
        SCO_WB,
        SCO_SWB,
        SCO_UWB
    } appKymeraScoMode;
*/
appKymeraScoMode appTestGetHfpCodec(void);


#ifdef INCLUDE_ACCESSORY

/*! \brief Send an app launch request
    \param app_name The name of the application to launch, in reverse DNS notation.
*/
void appTestRequestAppLaunch(const char * app_name);

/*! \brief Check the accessory connected state
    \return bool TRUE if accessory is connected, else FALSE
*/
bool appTestIsAccessoryConnected(void);

#endif /* INCLUDE_ACCESSORY */

/*! \brief Get the current audio source volume

    \return the current audio source volume represented in audio source volume steps
*/
int appTestGetCurrentAudioVolume(void);

/*! \brief Get the current voice source volume

    \return the current audio source volume represented in voice source volume steps
*/
int appTestGetCurrentVoiceVolume(void);

/*! \brief Get the Adaptive ANC Feed Forward Gain
*/
uint8 appTestGetAdaptiveAncFeedForwardGain(void);

/*! \brief Get the Current Adaptive ANC Mode

    \return Current mode ranging from 0-6 with which Adaptive ANC operator is configured
*/
adaptive_anc_mode_t appTestGetCurrentAdaptiveAncMode(void);

/*! \brief To identify if noise level is below Quiet Mode threshold when Adaptive ANC is enabled

    \return TRUE if noise level is below threshold, otherwise FALSE
*/
bool appTestAdaptiveAncIsNoiseLevelBelowQuietModeThreshold(void);

/*! \brief To identify if Quiet Mode is detected on local device.
           This API returning TRUE doesn't imply that AANC operator is configured with Quiet Mode.
           Quiet Mode will be configured to Adaptive ANC operator, ONLY IF both the peer devices detect Quiet Mode.

    \return TRUE if Quiet Mode is detected, otherwise FALSE
*/
bool appTestAdaptiveAncIsQuietModeDetectedOnLocalDevice(void);

/*! \brief Set the Adaptive ANC Gain Parameters
*/
void appTestSetAdaptiveAncGainParameters(uint32 mantissa, uint32 exponent);


/*! Checks whether there is an active BREDR connection.

    \return TRUE if there is a \b connected BREDR link. Any other cases,
        including links in the process of disconnecting and connecting
        are reported as FALSE
 */
bool appTestAnyBredrConnection(void);


/*! Checks if the BREDR Scan Manager is enabled for page or inquiry scan

    \return TRUE if either is enabled
 */
bool appTestIsBredrScanEnabled(void);

/*! Checks if the device test service is supported and enabled

    \see appTestIsDeviceTestServiceEnable to enable or disable
        the service (IF supported in this build)

    \return TRUE if the device test service is supported, and is
        enabled, FALSE otherwise
 */
bool appTestIsDeviceTestServiceEnabled(void);


/*! Checks if the device test service is usable or in use

    This covers whether the service has been activated and is
    either connected, or connectable.

    \return TRUE if the device test service is currently active,
    FALSE otherwise.
 */
bool appTestIsDeviceTestServiceActive(void);


/*! Checks if the device test service has been authenticated

    \return TRUE if the service is activated, or does not
            need activation, FALSE in all other cases, including
            when authentication is in progress
 */
bool appTestIsDeviceTestServiceAuthenticated(void);


/*! Update the configuration key to enable or disable the
    device test service

    The configuration will take effect the next time the device
    test service is activated, normally when the device
    restarts.

    \note See #device_test_service_mode_t for definition of
    variations of the enabled service.

    \param mode DTS_MODE_DISABLED to disable the service.
                DTS_MODE_ENABLED or DTS_MODE_ENABLED_IDLE to enable.

    \return TRUE if the configuration value now matches the request,
            FALSE otherwise.
 */
bool appTestDeviceTestServiceEnable(device_test_service_mode_t mode);


/*! Find out how many devices have been used by the device test
   service.

   The device test service tracks devices that it is responsible
   for creating or using. This function allows an application to
   discover how many, and if requested, which devices.

    \param[out] devices Address of a pointer than can be populated
                with an array of devices. NULL is permitted.
                If the pointer is populated after the call then
                the application is responsible for freeing the
                memory

    \return The number of devices found. 0 is possible.
 */
unsigned appTestDeviceTestServiceDevices(device_t **devices);

/*!
 * \brief This returns true if the appplication state indicates the
          device is about to sleep
 * \return true or false as per description.
 */
bool appTestIsPreparingToSleep(void);


/*! See if the Kymera sub-system claims to be idle

    This does not guarantee that a Kymera request will succeed.

    \return TRUE if Kymera is idle, FALSE otherwise.
*/
bool appTestIsKymeraIdle(void);


/*!
 * \brief This returns true if peer earbud is connected over qhs and
          false if peer earbud is not connected over qhs
 * \return true or false as per description.
 */
bool appTestIsPeerQhsConnected(void);

/*!
 * \brief This returns true if handset is connected over qhs and false if
          handset is not connected over qhs
 * \return true or false as per description.
 */
bool appTestIsHandsetQhsConnected(void);

#ifdef INCLUDE_TEST_TONES
/*! \brief Start a continuous tone (at least 5 minutes).
 * Used to tune Source Sync to protect the tone from glitches
 * under weak or interfered RF conditions.
 */
void appTestToneStartContinuous(void);

/*! \brief Start a continuous tone (at least 1 minute) with periodically
 * inserted short index tones.
 * These can be used to align different
 * recordings and traces such as a KSP sniff and an analog
 * recording.
 */
void appTestToneStartIndexed(void);

/*! \brief Look up a tone by event ID from the non-recurring event tone table
 * and play it.
 */
void appTestToneStartByEvent(uint16 event_id);

/*! \brief Stop a tone.
 */
void appTestToneStop(void);
#endif

#ifdef INCLUDE_GAMING_MODE
/*! Check if Gaming Mode is enabled.
    \return TRUE if gaming mode is ON, FALSE otherwise.
*/
bool appTestIsGamingModeOn(void);

/*! Enable/Disable Gaming Mode. This injects a ui_event (i.e. mimics the UI button press event).
    This can be called on both primary and secondary.

    \param[in] enable TRUE to enable gaming mode,
                      FALSE to disable.
    \return TRUE if command is successful.
 */
bool appTestSetGamingMode(bool enable);
#endif

#ifdef INCLUDE_LATENCY_MANAGER
/*! Check if Dynamic Latency adjustment is enabled.
    \return TRUE if enabled, FALSE otherwise.
*/
bool appTestIsDynamicLatencyAdjustmentEnabled(void);

/*! Set Dynamic Latency Adjustment configuration. This API enables or disables the latency
    management feature. By default Latency adjustment is disabled during initialization.
    This API can be invoked any time post initialization.
    \param[in] enable TRUE to enable dynamic adjustment feature.
                      FALSE to disable
*/
void appTestSetDynamicLatencyAdjustment(bool enable);

/*! Get current A2DP latency.
    \returns latency in milliseconds.
*/
uint16 appTestGetCurrentA2dpLatency(void);
#endif

/*! \brief Check if earbud state is appropriate to go to sleep.

    It checks if earbud state is the one in which calling appTestIsTimeToSleep(),
    would put earbud to sleep.

    \return TRUE if earbud in the right state to go to sleep.
*/
bool appTestIsTimeToSleep(void);

/*! \brief Put earbud to sleep.

    Puts earbud to sleep immediately.
    As long as appTestIsPreparingToSleep() have returned TRUE beforehand.
*/
void appTestTriggerSleep(void);

/*! \brief Disable charger checks which would prevent going to sleep. */
void appTestForceAllowSleep(void);

/*! \brief Disable charger sending the earbud to dormant state. */
void appTestDisallowDormant(void);

/*! \brief Registers the test task to receive kymera notifications. */
void appTestRegisterForKymeraNotifications(void);

/* \brief Disconnect all currently connected handsets. */
void appTestDisconnectHandsets(void);

/*! \brief To get current A2DP codec
    The return value follows this enum
*/
typedef enum
{
    a2dp_codec_sbc,
    a2dp_codec_aac,
    a2dp_codec_aptx,
    a2dp_codec_aptx_adaptive,
    a2dp_codec_unknown,
} app_test_a2dp_codec_t;
app_test_a2dp_codec_t EarbudTest_GetA2dpCodec(void);

/*! \brief Generate event that Case lid is now open. */
void appTestCaseLidOpenEvent(void);

/*! \brief Generate event that Case lid is now closed. */
void appTestCaseLidClosedEvent(void);

/*! \brief Generate event that new battery state has been received from the case.
    \param case_battery_state Case battery percentage and charging state.
    \param peer_battery_state Peer earbud battery percentage and charging state.
    \param local_battery_state Local earbud battery percentage and charging state.
    \param case_charger_connected TRUE case charger is connected, FALSE not connected.
*/
void appTestCasePowerMsgEvent(uint8 case_battery_state,
                              uint8 peer_battery_state, uint8 local_battery_state,
                              bool case_charger_connected);

/*! \brief Determine if Case events are supported in the application.
    \return TRUE Case lid open/close and power events are supported.
            FALSE Case lid and power events not supported.
*/
bool appTestCaseEventsSupported(void);

/* \brief Update or replace pairing to a peer with pairing to a new address

    This function is for use in a factory context to pair two devices
    without the need of any radio connection.

    \note It does not matter if the devices already have earbud pairing.

    \note The earbuds cannot be connected to an earbud. They should be
        disconnected first, by placing in the case if necessary,
        appTestPhyStateInCaseEvent().

    \param primary          Pointer to the primary device address in the pairing
    \param secondary        Pointer to the secondary device address in the pairing
    \param this_is_primary  Whether this is the device for the primary address
    \param randomised_keys  New root keys to be set. This is the encryption key
                            and identity root. These must be the same on each device
                            in an earbud pair.
    \param bredr_key        Pointer to the BREDR link key.
    \param le_key           Pointer to the Bluetooth Low Energy long term key.

    \return TRUE if the pairing was successful and records were created. FALSE if
            any errors were detected.
 */
bool earbudTestAddPeerPairing(const bdaddr *primary,
                              const bdaddr *secondary,
                              bool this_is_primary,
                              const cl_root_keys *randomised_keys,
                              PEER_PAIRING_LONG_TERM_KEY_T  *bredr_key,
                              PEER_PAIRING_LONG_TERM_KEY_T  *ble_key);



/* \brief Initiate pairing to a peer with a specific public address

    This function is for use in a factory context to pair devices
    where it is possible for many earbuds to exist.

    To check for progress and completion the following test APIs can be used
    appTestIsPeerPairLeRunning() and appTestIsPeerPaired()

    \note The function will fail if peer pairing is already in progress.
        The use of a Device Test Service mode is recommended to stop this.

    \note It does not matter if the devices already have earbud pairing.

    \note The earbuds cannot be connected. They should be disconnected first,
        by placing in the case if necessary, appTestPhyStateInCaseEvent().

    \param address The Bluetooth device address to pair with

    \return FALSE if failed to clear any existing pairing, TRUE if \b pairing
            \b has \been \b started
*/
bool earbudTestPeerPairWithAddress(bdaddr *address);

#ifdef INCLUDE_GAA
#include "gaa_ota.h"
/*! \brief Get the current state of GAA OTA.
    \return uint8 from GAA_OTA_STATE_T enum, or 255 if GAA OTA is not initialised
*/
uint8 EarbudTest_GetGaaOtaState(void);

#include "gaa_info.h"
/*! \brief Get the current GAA model ID
    \return Current GAA model ID (zero if failed)
*/
uint32 EarbudTest_GetGaaModelId(void);
/*! \brief Set the GAA model ID to the paramter value
    \param uint32 model_id The new GAA model ID
    \return TRUE is successful
*/
bool EarbudTest_SetGaaModelId(uint32 model_id);

/*! \brief Get the GAA OTA control value
    \return uint16 GAA OTA control value if successfu, else MAX_UINT32 if not
*/
uint32 appTestGetGaaOtaControl(void);

/*! \brief Set the GAA OTA control value
*/
void appTestSetGaaOtaControl(uint16 GaaOtaControl);


#endif  /* INCLUDE_GAA */


#ifdef INCLUDE_MUSIC_PROCESSING

/*! \brief Prints out the available presets and their UCIDs
*/
void appTest_MusicProcessingGetAvailablePresets(void);

/*! \brief Sets a new EQ preset

    \param preset   The preset UCID
*/
void appTest_MusicProcessingSetPreset(uint8 preset);

/*! \brief Prints out the active EQ type
*/
void appTest_MusicProcessingGetActiveEqType(void);

/*! \brief Prints out the total number of user EQ bands that can be modified
*/
void appTest_MusicProcessingGetNumberOfActiveBands(void);

/*! \brief Prints out the current user EQ band information
*/
void appTest_MusicProcessingGetEqBandInformation(void);

/*! \brief Sets the gain of a specific band

    \param band     Band to modify

    \param gain     New gain value to set
*/
void appTest_MusicProcessingSetUserEqBand(uint8 band, int16 gain);

#endif /* INCLUDE_MEDIA_PROCESSING */

#ifdef INCLUDE_CHARGER

/*! Simulate charger connected/disconnected events
 *
 * \param is_connected TRUE to simulate attach or FALSE for detach */
void appTestChargerConnected(bool is_connected);

/*! Simulate detection of particular charger
 *
 * \param attached_status type of charger detected
 * \param charger_dp_millivolts voltage measured on USB_DP line
 * \param charger_dm_millivolts voltage measured on USB_DM line
 * \param cc_status USB_CC line state
 * */
void appTestChargerDetected(usb_attached_status attached_status,
                                       uint16 charger_dp_millivolts,
                                       uint16 charger_dm_millivolts,
                                       usb_type_c_advertisement );

/*! Simulate charger status events
 *
 * \param chg_status charger status to simulate */
void appTestChargerStatus(charger_status chg_status);

/*! Return back to normal operation after appsTestCharger* calls. */
void appTestChargerRestore(void);

#endif /* INCLUDE_CHARGER */

#ifdef ENABLE_EARBUD_FIT_TEST
/*! \brief  Start the Earbud fit test. */
void EarbudTest_StartFitTest(void);
/*! \brief  Stop/Abort the Earbud fit test. */
void EarbudTest_StopFitTest(void);

/*! \brief  To get the final local device fit test result.
    \return The return value follows this enum
    typedef enum
    {
        fit_test_result_bad,
        fit_test_result_good,
        fit_test_result_error,
    } fit_test_result_t;
*/
uint32 EarbudTest_GetLocalDeviceFitTestStatus(void);

/*! \brief  To get the final remote device fit test result.
    \return The return value follows this enum
    typedef enum
    {
        fit_test_result_bad,
        fit_test_result_good,
        fit_test_result_error,
    } fit_test_result_t;
*/
uint32 EarbudTest_GetRemoteDeviceFitTestStatus(void);

/*!
   \brief  Start Fit Test tuning.
   \note   This API is supposed to be used in production calibration and not intended for customers.
           No Button/GAIA support is provided for EFT Tuning.
*/
void EarbudTest_StartFitTestTuning(void);

/*! \brief  Stop Fit Test tuning.
    \note   This API is supposed to be used in production calibration and not intended for customers.
            No Button/GAIA support is provided for EFT Tuning.
*/
void EarbudTest_StopFitTestTuning(void);

#endif /* ENABLE_EARBUD_FIT_TEST */


void EarbudTest_ApplyUiUserConfigTestVector(void);

#endif /* EARBUD_TEST_H */
