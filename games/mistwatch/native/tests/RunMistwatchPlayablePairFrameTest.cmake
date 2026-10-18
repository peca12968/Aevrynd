# 僅建立本次CPU測試的全新輸出；不更改父程序環境或使用者存檔。
foreach(_required IN ITEMS PROGRAM BOAR_ASSET OUTPUT_ROOT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "Missing explicit ${_required}")
  endif()
  if(NOT IS_ABSOLUTE "${${_required}}")
    message(FATAL_ERROR "${_required} must be an absolute path")
  endif()
endforeach()
if(NOT EXISTS "${PROGRAM}" OR NOT EXISTS "${BOAR_ASSET}")
  message(FATAL_ERROR "Explicit test executable or raw boar asset is missing")
endif()

string(RANDOM LENGTH 32 ALPHABET "0123456789abcdef" _run_id)
set(_run "${OUTPUT_ROOT}/run-${_run_id}")
if(EXISTS "${_run}")
  message(FATAL_ERROR "Fresh output collision: ${_run}")
endif()
file(MAKE_DIRECTORY "${_run}/isolated-localapp" "${_run}/isolated-appdata" "${_run}/isolated-temp")

# cmake -E env 只替換子程序的四個目錄；此CMake/CTest及使用者環境原樣保留。
# CTest總期限也是120秒，包含本wrapper，不擴張原CPU界線。
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "LOCALAPPDATA=${_run}/isolated-localapp"
    "APPDATA=${_run}/isolated-appdata"
    "TEMP=${_run}/isolated-temp"
    "TMP=${_run}/isolated-temp"
    "${PROGRAM}" "${BOAR_ASSET}" "${_run}/evidence"
  WORKING_DIRECTORY "${_run}"
  RESULT_VARIABLE _result
  OUTPUT_FILE "${_run}/stdout.raw.log"
  ERROR_FILE "${_run}/stderr.raw.log"
  TIMEOUT 120)
file(WRITE "${_run}/result.txt" "exit=${_result}\n")
file(READ "${_run}/stdout.raw.log" _stdout)
file(READ "${_run}/stderr.raw.log" _stderr)
if(NOT "${_result}" STREQUAL "0")
  message(FATAL_ERROR "Real main fixture failed (${_result}); evidence=${_run}\n${_stderr}")
endif()

# 不使用PASS_REGULAR_EXPRESSION覆蓋退出碼；兩項必須同時成立。
set(_marker "REAL_MAIN_RECOVERY_CPU_PASS checks=763 frames=151 actual_OnUpdate=1 real_Input=1 renderer_started=0 window=0 global_input=0")
set(_prefix "REAL_MAIN_RECOVERY_CPU_PASS")
# 精確整行且僅一次；子程序退出碼、五個案例與151幀追蹤規則均保持原樣。
string(REPLACE "\r\n" "\n" _normalized "${_stdout}")
string(REPLACE ";" "\\;" _escaped "${_normalized}")
string(REPLACE "\n" ";" _lines "${_escaped}")
set(_marker_count 0)
set(_markers_equal TRUE)
foreach(_line IN LISTS _lines)
  if(_line MATCHES "^${_prefix}")
    math(EXPR _marker_count "${_marker_count}+1")
    if(NOT "${_line}" STREQUAL "${_marker}")
      set(_markers_equal FALSE)
    endif()
  endif()
endforeach()
if(NOT _marker_count EQUAL 1 OR NOT _markers_equal OR _stderr MATCHES "FIRST_DIVERGENCE=")
  message(FATAL_ERROR "Exact five-case marker missing or duplicated; evidence=${_run}")
endif()
set(_trace "${_run}/evidence/actual-main-frames.tsv")
if(NOT EXISTS "${_trace}")
  message(FATAL_ERROR "Actual OnUpdate trace missing; evidence=${_run}")
endif()
file(STRINGS "${_trace}" _trace_lines)
list(LENGTH _trace_lines _trace_count)
if(NOT _trace_count EQUAL 152)
  message(FATAL_ERROR "Expected header plus151 actual frames; evidence=${_run}")
endif()
# 地圖切換另用獨立計數／追蹤；原五例與151幀仍維持前述精確驗收。
set(_survey_marker "SURVEY_MAP_CONTROLS_CPU_PASS cases=4 checks=205 frames=47 true_OnUpdate=1 real_Input=1 held_J_no_repeat=1 paused_world_frozen=1 resume_boundary_frozen=1 held_W_next_update=1 original_counters_unchanged=1 OnRender=0 GPU=0")
set(_survey_count 0)
foreach(_line IN LISTS _lines)
  if(_line MATCHES "^SURVEY_MAP_CONTROLS_CPU_PASS")
    math(EXPR _survey_count "${_survey_count}+1")
    if(NOT "${_line}" STREQUAL "${_survey_marker}")
      message(FATAL_ERROR "Unexpected survey-controls summary; evidence=${_run}")
    endif()
  endif()
endforeach()
if(NOT _survey_count EQUAL 1)
  message(FATAL_ERROR "Expected one survey-controls summary; evidence=${_run}")
endif()
set(_survey_trace "${_run}/evidence/survey-map-controls.tsv")
if(NOT EXISTS "${_survey_trace}")
  message(FATAL_ERROR "Actual survey-controls trace missing; evidence=${_run}")
endif()
file(STRINGS "${_survey_trace}" _survey_lines)
list(LENGTH _survey_lines _survey_frames)
if(NOT _survey_frames EQUAL 48)
  message(FATAL_ERROR "Expected header plus47 survey-control frames; evidence=${_run}")
endif()
message(STATUS "${_marker}")
message(STATUS "${_survey_marker}")
message(STATUS "Main CPU evidence: ${_run}")
