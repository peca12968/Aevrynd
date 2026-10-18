cmake_minimum_required(VERSION 3.25)

# 每輪只選全新的輸出位置；原測試仍自行建立目錄、拒絕覆寫並執行全部存檔檢查。
# 不搬移或刪除既有證據，任一子程序失敗仍讓 CTest 失敗。
if(NOT DEFINED TEST_EXECUTABLE OR NOT IS_ABSOLUTE "${TEST_EXECUTABLE}" OR
   NOT EXISTS "${TEST_EXECUTABLE}" OR IS_DIRECTORY "${TEST_EXECUTABLE}")
    message(FATAL_ERROR "SAVE1420_EXECUTABLE_REQUIRED")
endif()
if(NOT DEFINED OUTPUT_ROOT OR NOT IS_ABSOLUTE "${OUTPUT_ROOT}" OR
   NOT IS_DIRECTORY "${OUTPUT_ROOT}" OR IS_SYMLINK "${OUTPUT_ROOT}")
    message(FATAL_ERROR "SAVE1420_EXISTING_ABSOLUTE_OUTPUT_ROOT_REQUIRED")
endif()

set(output "")
foreach(attempt RANGE 1 4)
    string(RANDOM LENGTH 32 ALPHABET 0123456789abcdef nonce)
    set(candidate "${OUTPUT_ROOT}/playable-flow-save-${nonce}")
    if(NOT EXISTS "${candidate}" AND NOT IS_SYMLINK "${candidate}")
        set(output "${candidate}")
        break()
    endif()
endforeach()
if(output STREQUAL "")
    message(FATAL_ERROR "SAVE1420_FRESH_OUTPUT_UNAVAILABLE")
endif()

message(STATUS "SAVE1420_FRESH_OUTPUT=${output}")
execute_process(COMMAND "${TEST_EXECUTABLE}" "${output}" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "SAVE1420_ORIGINAL_EXECUTABLE_EXIT=${result}; evidence preserved at ${output}")
endif()
