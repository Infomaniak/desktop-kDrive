if [ "${BASH_SOURCE[0]}" -ef "$0" ]; then
    echo "This script must be sourced, not executed."
    exit 1
fi

find_qt_conan_path() {
  local build_dir="$1"
  if [[ -z "$build_dir" ]]; then
    echo "Error: No build directory specified to the 'find_qt_conan_path' function." >&2
    exit 1
  fi

  local sourced=0
  local conan_path=""
  local qt_path=""

  if [[ -z "$CMAKE_PREFIX_PATH" ]]; then
    conan_path="$(dirname "$(find "$build_dir" -iname "conanbuild.sh" -type f | head -n 1)")"
    if [[ -n "$conan_path" && -f "$conan_path/conanbuild.sh" ]]; then
      source "$conan_path/conanbuild.sh"
      sourced=1
    else
      echo "Error: Could not find 'conanbuild.sh' in the build directory '$build_dir'." >&2
      exit 1
    fi
  fi

  IFS=':' read -ra path_dirs <<< "$CMAKE_PREFIX_PATH"
  for dir in "${path_dirs[@]}"; do
    if [[ "$dir" =~ ^/home/.*/\.conan2/p/b/qt[^/]*/p$ ]]; then
      qt_path="$dir"
      break
    fi
  done

  if [[ -n "$qt_path" ]]; then
    echo "$qt_path"
    [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanbuild.sh" ]] && source "$conan_path/deactivate_conanbuild.sh"
    return 0
  else
    echo "Error: Could not find the Qt Conan package path in CMAKE_PREFIX_PATH='$CMAKE_PREFIX_PATH'." >&2
    [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanbuild.sh" ]] && source "$conan_path/deactivate_conanbuild.sh"
    return 1
  fi
}