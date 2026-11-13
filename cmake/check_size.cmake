if(NOT EXISTS "${exe}")
  message(FATAL_ERROR "Size check: executable not found: ${exe}")
endif()

file(SIZE "${exe}" exe_size)
math(EXPR max_kb "${max} / 1024")
math(EXPR size_kb "${exe_size} / 1024")

message(STATUS "Binary size: ${exe_size} bytes (${size_kb} KB) / limit ${max} bytes (${max_kb} KB)")

if(exe_size GREATER max)
  message(FATAL_ERROR
    "Binary too large for device flash: ${exe_size} bytes > ${max} bytes (100 KB). "
    "Strip features or tighten link flags.")
endif()
