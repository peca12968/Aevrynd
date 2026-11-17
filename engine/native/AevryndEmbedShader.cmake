cmake_minimum_required(VERSION 3.25)

# 一次寫出標頭檔，避免 FXC 在大型標頭上反覆寫入許多小段文字。
file(READ "${shader_bytecode}" shader_hex HEX)
string(SUBSTRING "${shader_hex}" 0 8 shader_magic)
if(NOT shader_magic STREQUAL "44584243")
  message(FATAL_ERROR "FXC output is not DXBC: ${shader_bytecode}")
endif()

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," shader_bytes "${shader_hex}")
string(REPEAT "0x[0-9a-f][0-9a-f]," 16 shader_row)
string(REGEX REPLACE "(${shader_row})" "\\1\n  " shader_rows "${shader_bytes}")
string(STRIP "${shader_rows}" shader_rows)
file(WRITE "${shader_output}.tmp"
  "// 由 FXC 位元組碼產生。\nconst BYTE ${shader_symbol}[] =\n{\n  ${shader_rows}\n};\n")
file(RENAME "${shader_output}.tmp" "${shader_output}")
file(REMOVE "${shader_bytecode}")
