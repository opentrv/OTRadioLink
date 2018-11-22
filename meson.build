# Assumes ninja test is in the same directory.
# Lines that are commented out facilitate usage as a subproject.
# This is the "better" way of doing things, but is a pain with our current
# mishmash of build systems.

project('OTRadioLinkTests', 'cpp',
    default_options : [
        'c_std=c11', 'cpp_std=c++11']
)

# libOTAESGCM_proj = subproject('OTAESGCM')
# libOTAESGCM_dep = libOTAESGCM_proj.get_variable('libOTAESGCM_dep')

gtest_dep = dependency('gtest_main')

inc = include_directories(
    'content/OTRadioLink',
    'content/OTRadioLink/utility',
    'portableUnitTests',
    'portableUnitTests/OTRadioLink',
    'portableUnitTests/OTRadValve',
    'portableUnitTests/OTV0p2Base',
    '../OTAESGCM/content/OTAESGCM',
    '../OTAESGCM/content/OTAESGCM/utility'
)

src = [
    'content/OTRadioLink/utility/OTV0P2BASE_HardwareTests.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_RTC.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Entropy.cpp',
    'content/OTRadioLink/utility/OTRadValve_BoilerDriver.cpp',
    'content/OTRadioLink/utility/OTRadValve_TestValve.cpp',
    'content/OTRadioLink/utility/OTRadValve_ValveMotorBase.cpp',
    'content/OTRadioLink/utility/OTRadValve_ModelledRadValveState.cpp',
    'content/OTRadioLink/utility/OTRadioLink_JeelabsOemPacket.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_MinOW.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_EEPROM.cpp',
    'content/OTRadioLink/utility/OTRadValve_Parameters.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_QuickPRNG.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorTMP112.cpp',
    'content/OTRadioLink/utility/OTRadioLink_SecureableFrameType.cpp',
    'content/OTRadioLink/utility/OTRadValve_ModelledRadValve.cpp',
    'content/OTRadioLink/utility/OTRadValve_SimpleValveSchedule.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_ErrorReport.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorAmbientLight.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_CLI.cpp',
    'content/OTRadioLink/utility/OTRFM23BLink_OTRFM23BLink.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SoftSerial.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_JSONStats.cpp',
    'content/OTRadioLink/utility/OTRadValve_FHT8VRadValve.cpp',
    'content/OTRadioLink/utility/OTRadioLink_SecureableFrameType_V0p2Impl.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Sleep.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_PowerManagement.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorSHT21.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Security.cpp',
    'content/OTRadioLink/utility/OTSIM900Link_OTSIM900Link.cpp',
    'content/OTRadioLink/utility/OTRadValve_CurrentSenseValveMotorDirect.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorAmbientLightOccupancy.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_CRC.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_ADC.cpp',
    'content/OTRadioLink/utility/OTRadValve_ActuatorPhysicalUI.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Util.cpp',
    'content/OTRadioLink/utility/OTRN2483Link_OTRN2483Link.cpp',
    'content/OTRadioLink/utility/OTRadioLink_ISRRXQueue.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Serial_IO.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorDS18B20.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorOccupancy.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SimpleBinaryStats.cpp',
    'content/OTRadioLink/utility/OTRadioLink_OTNullRadioLink.cpp',
    'content/OTRadioLink/utility/OTRadioLink_OTRadioLink.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_SensorQM1.cpp',
    'content/OTRadioLink/utility/OTV0P2BASE_Stats.cpp',
    '../OTAESGCM/content/OTAESGCM/utility/OTAESGCM_OTAES128AVR.cpp',
    '../OTAESGCM/content/OTAESGCM/utility/OTAESGCM_OTAESGCM.cpp',
    'portableUnitTests/main.cpp',
    'portableUnitTests/OTV0p2Base/ConcurrencyTest.cpp',
    'portableUnitTests/OTV0p2Base/JSONStatsTest.cpp',
    'portableUnitTests/OTV0p2Base/PseudoSensorOccupancyTrackerTest.cpp',
    'portableUnitTests/OTV0p2Base/AmbientLightTest.cpp',
    'portableUnitTests/OTV0p2Base/EEPROMTest.cpp',
    'portableUnitTests/OTV0p2Base/RTCTest.cpp',
    'portableUnitTests/OTV0p2Base/OTV0p2BaseTest.cpp',
    'portableUnitTests/OTV0p2Base/UtilTest.cpp',
    'portableUnitTests/OTV0p2Base/ByHourByteStatsTest.cpp',
    'portableUnitTests/OTV0p2Base/SystemStatsLineTest.cpp',
    'portableUnitTests/OTRadValve/CurrentSenseValveMotorDirectTest.cpp',
    'portableUnitTests/OTRadValve/ModelledRadValveTest.cpp',
    'portableUnitTests/OTRadValve/ModelledRadValveThemalModelTest.cpp',
    'portableUnitTests/OTRadValve/RadValveParamsTest.cpp',
    'portableUnitTests/OTRadValve/AmbientLightOccupancyDetectionTest.cpp',
    'portableUnitTests/OTRadValve/ValveScheduleTest.cpp',
    'portableUnitTests/OTRadValve/ModeButtonAndPotActuatorPhysicalUITest.cpp',
    'portableUnitTests/OTRadValve/FHT8VRadValveTest.cpp',
    'portableUnitTests/OTRadValve/BoilerDriverTest.cpp',
    'portableUnitTests/OTRadValve/TempControlTest.cpp',
    'portableUnitTests/OTRadValve/ValveModeTest.cpp',
    'portableUnitTests/OTRadValve/RadValveActuatorTest.cpp',
    'portableUnitTests/OTRadioLink/SecureOpStackDepthTest.cpp',
    'portableUnitTests/OTRadioLink/OTSIM900LinkTest.cpp',
    'portableUnitTests/OTRadioLink/SecureFrameTest.cpp',
    'portableUnitTests/OTRadioLink/FrameHandlerTest.cpp',
]

test_app = executable('OTRadioLinkTests', src,
    include_directories : inc,
    dependencies : gtest_dep,
    # dependencies : [gtest_dep, libOTAESGCM_dep],
    cpp_args : [
        '-O0',
        '-Wall', '-Werror',
        '-Wno-non-virtual-dtor',
        '-fstack-check', '-fstack-protector-strong',
        '-DEXT_AVAILABLE_ARDUINO_LIB_OTAESGCM'
    ],
    install : false
)

test('unit_tests', test_app)
