# 僅建立本次測試的全新輸出；不更改父程序環境或使用者存檔。
foreach(_required IN ITEMS PROGRAM OUTPUT_ROOT MODE)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "Missing explicit ${_required}")
  endif()
endforeach()
foreach(_path IN ITEMS PROGRAM OUTPUT_ROOT)
  if(NOT IS_ABSOLUTE "${${_path}}")
    message(FATAL_ERROR "${_path} must be an absolute path")
  endif()
endforeach()
if(NOT EXISTS "${PROGRAM}")
  message(FATAL_ERROR "Explicit test executable is missing")
endif()
if(MODE STREQUAL "Input")
  set(_prefix "FRAME_INPUT_LATCH_CPU_PASS")
  set(_marker "FRAME_INPUT_LATCH_CPU_PASS checks=449 exact_frozen_latch=1 real_Input_API=1 windows_created=0 global_input_injected=0 game_or_GPU_executed=0 main_or_native_qualified=0")
elseif(MODE STREQUAL "Save")
  if(NOT DEFINED BOAR_ASSET OR NOT IS_ABSOLUTE "${BOAR_ASSET}" OR NOT EXISTS "${BOAR_ASSET}")
    message(FATAL_ERROR "Explicit absolute raw boar asset is required")
  endif()
  set(_prefix "PAIR_SAVE_PREFLIGHT_CPU_PASS")
  set(_marker "PAIR_SAVE_PREFLIGHT_CPU_PASS full_prepared=13 constructed_query_controls=12 accepted=14 rejected=10 real_file_preservation=2 pending_preview_valid_after_reject=1 future_jump_steps=1 future_camera_updates=6 raw_boar=1 fallback_boar=0 current_Explorer=1 whole_original_suite_executed=0 main_F5_F9_qualified=0")
else()
  message(FATAL_ERROR "Unknown explicit input/save mode")
endif()

string(RANDOM LENGTH 32 ALPHABET "0123456789abcdef" _run_id)
set(_run "${OUTPUT_ROOT}/run-${_run_id}")
if(EXISTS "${_run}")
  message(FATAL_ERROR "Fresh output collision: ${_run}")
endif()
file(MAKE_DIRECTORY "${_run}/isolated-localapp" "${_run}/isolated-appdata" "${_run}/isolated-temp")
set(_arguments)
if(MODE STREQUAL "Save")
  # 子fixture自行建立不存在的目錄；不得預先建立或重用使用者存檔位置。
  list(APPEND _arguments "${BOAR_ASSET}" "${_run}/fixture-files")
endif()

# cmake -E env僅變更child四個目錄，父CTest環境不變，失敗證據不刪除。
# CTest總TIMEOUT同為120秒，包含wrapper，不把child期限另加到總期限。
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "LOCALAPPDATA=${_run}/isolated-localapp"
    "APPDATA=${_run}/isolated-appdata"
    "TEMP=${_run}/isolated-temp"
    "TMP=${_run}/isolated-temp"
    "${PROGRAM}" ${_arguments}
  WORKING_DIRECTORY "${_run}"
  RESULT_VARIABLE _result
  OUTPUT_FILE "${_run}/stdout.raw.log"
  ERROR_FILE "${_run}/stderr.raw.log"
  TIMEOUT 120)
file(WRITE "${_run}/result.txt" "mode=${MODE}\nexit=${_result}\n")
file(READ "${_run}/stdout.raw.log" _stdout)
file(READ "${_run}/stderr.raw.log" _stderr)
if(NOT "${_result}" STREQUAL "0")
  message(FATAL_ERROR "Input/save fixture failed (${_result}); evidence=${_run}\n${_stderr}")
endif()

# 真退出碼0與精確原marker都必要；不使用PASS_REGULAR_EXPRESSION吞掉失敗。
# 逐整行核對；保留重複相鄰marker及尾端空白的拒絕，不接受行中片段。
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
if(NOT _marker_count EQUAL 1 OR NOT _markers_equal OR _stderr MATCHES "FIRST_DIVERGENCE")
  message(FATAL_ERROR "Exact original marker missing, changed or duplicated; evidence=${_run}")
endif()
if(MODE STREQUAL "Save")
  foreach(_file IN ITEMS safe-save.txt unsafe-legacy.txt)
    if(NOT EXISTS "${_run}/fixture-files/${_file}")
      message(FATAL_ERROR "Original real file output missing: ${_file}; evidence=${_run}")
    endif()
  endforeach()
endif()
message(STATUS "${_marker}")
message(STATUS "Input/save CPU evidence: ${_run}")
