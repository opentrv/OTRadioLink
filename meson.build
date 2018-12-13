# Assumes OTAESGCM is in subprojects. Symlinked on DE's machine for
# convenience.
# Alternatively, commented lines assume OTAESGCM is in the same root folder as
# OTRadiolink.

project('OTRadioLink', 'cpp',
    default_options : [
        'c_std=c11', 'cpp_std=c++11']
)

# Setup and compile gtest.
# Tries to find gtest via normal dependency manager (e.g. pkgconf) and falls 
# back to downloading and compiling using a wrap file.
gtest_dep = dependency('gtest_main', required : false)
if not gtest_dep.found()
    thread_dep = dependency('threads')
    gtest_proj = subproject('gtest')
    gtest_inc = gtest_proj.get_variable('gtest_incdir')
    gtest_src = [
        gtest_proj.get_variable('gtest_libsources'), 
        gtest_proj.get_variable('gtest_mainsources')
    ]
    gtest_lib = static_library('gtest', gtest_src,
        include_directories : gtest_inc,
        dependencies : thread_dep) # + all your args here
    gtest_dep = declare_dependency(
        include_directories : gtest_inc, 
        link_with : gtest_lib)
endif

# Setup and compile OTAESGCM libs
libOTAESGCM_proj = subproject('OTAESGCM')
libOTAESGCM_dep = libOTAESGCM_proj.get_variable('libOTAESGCM_dep')

inc = include_directories(
    'content/OTRadioLink',
    'content/OTRadioLink/utility',
    'portableUnitTests',
    'portableUnitTests/OTRadioLink',
    'portableUnitTests/OTRadValve',
    'portableUnitTests/OTV0p2Base',
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
]

libOTRadioLink = static_library('OTRadioLink', src,
    include_directories : inc,
    dependencies : libOTAESGCM_dep,
    cpp_args : [
        '-O0',
        '-Wall', '-Werror',
        '-Wno-non-virtual-dtor',
        '-fstack-check', '-fstack-protector-strong',
        '-DEXT_AVAILABLE_ARDUINO_LIB_OTAESGCM'
    ],
    install : true
)

libOTRadioLink_dep = declare_dependency(
    include_directories : inc, 
    link_with : libOTRadioLink
)


test_src = [
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

test_app = executable('OTRadioLinkTests', [src, test_src],
    include_directories : inc,
    dependencies : [gtest_dep, libOTAESGCM_dep],
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
