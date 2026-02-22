cmake_minimum_required(VERSION 3.10)

# --- DEFINIÇÃO DE ARQUIVOS ---
set(HEADER_FILES
    include/chiaki/session.h include/chiaki/common.h include/chiaki/sock.h
    include/chiaki/thread.h include/chiaki/base64.h include/chiaki/http.h
    include/chiaki/log.h include/chiaki/ctrl.h include/chiaki/rpcrypt.h
    include/chiaki/takion.h include/chiaki/senkusha.h include/chiaki/streamconnection.h
    include/chiaki/ecdh.h include/chiaki/launchspec.h include/chiaki/random.h
    include/chiaki/gkcrypt.h include/chiaki/audio.h include/chiaki/audioreceiver.h
    include/chiaki/video.h include/chiaki/videoreceiver.h include/chiaki/frameprocessor.h
    include/chiaki/packetstats.h include/chiaki/seqnum.h include/chiaki/discovery.h
    include/chiaki/congestioncontrol.h include/chiaki/stoppipe.h include/chiaki/reorderqueue.h
    include/chiaki/discoveryservice.h include/chiaki/feedback.h include/chiaki/feedbacksender.h
    include/chiaki/controller.h include/chiaki/takionsendbuffer.h include/chiaki/time.h
    include/chiaki/fec.h include/chiaki/regist.h include/chiaki/opusdecoder.h include/chiaki/orientation.h)

set(SOURCE_FILES
    src/common.c src/sock.c src/session.c src/thread.c src/base64.c src/http.c
    src/log.c src/ctrl.c src/rpcrypt.c src/takion.c src/senkusha.c src/utils.h
    src/pb_utils.h src/streamconnection.c src/ecdh.c src/launchspec.c src/random.c
    src/gkcrypt.c src/audio.c src/audioreceiver.c src/videoreceiver.c src/frameprocessor.c
    src/packetstats.c src/discovery.c src/congestioncontrol.c src/stoppipe.c
    src/reorderqueue.c src/discoveryservice.c src/feedback.c src/feedbacksender.c
    src/controller.c src/takionsendbuffer.c src/time.c src/fec.c src/regist.c
    src/opusdecoder.c src/orientation.c)

# --- DEPENDÊNCIAS INTERNAS ---
add_subdirectory(protobuf)
set_source_files_properties(${CHIAKI_LIB_PROTO_SOURCE_FILES} ${CHIAKI_LIB_PROTO_HEADER_FILES} PROPERTIES GENERATED TRUE)

if(NOT TARGET Jerasure::Jerasure)
    add_subdirectory(third-party/jerasure EXCLUDE_FROM_ALL)
endif()

# --- BIBLIOTECA PRINCIPAL ---
add_library(chiaki-lib ${HEADER_FILES} ${SOURCE_FILES} ${CHIAKI_LIB_PROTO_SOURCE_FILES} ${CHIAKI_LIB_PROTO_HEADER_FILES})
configure_file(config.h.in include/chiaki/config.h)

target_include_directories(chiaki-lib PUBLIC 
    "${CMAKE_CURRENT_BINARY_DIR}/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "third-party/jerasure/include")

if(CHIAKI_LIB_PROTO_INCLUDE_DIR)
    target_include_directories(chiaki-lib PRIVATE "${CHIAKI_LIB_PROTO_INCLUDE_DIR}")
endif()

# --- LINKAGEM (TODAS COM KEYWORD PUBLIC) ---
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(chiaki-lib PUBLIC 
    Threads::Threads 
    OpenSSL::Crypto 
    Nanopb::nanopb 
    Jerasure::Jerasure)

if(WIN32)
    target_link_libraries(chiaki-lib PUBLIC wsock32 ws2_32 bcrypt)
endif()

if(CHIAKI_LIB_ENABLE_OPUS)
    find_package(Opus REQUIRED)
    target_link_libraries(chiaki-lib PUBLIC ${Opus_LIBRARIES})
endif()
