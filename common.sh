#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

function message() {
    local type=$1
    shift
    echo -e "$type: $*" >&2
}

function prompt() {
    local PomptHBlue='\e[38;05;33m';
    local PomptTBlue='\e[38;05;45m';
    message "${PomptHBlue}INFO${PomptTBlue}" "$*\e[m"
}

function error() {
    local ErrorHRed='\e[38;05;1m'
    local PomptTBlue='\e[38;05;45m';
    message "${ErrorHRed}ERROR${PomptTBlue}" "$*\e[m"
}

function warning() {
    local WarningHPurple='\e[38;05;61m';
    local PomptTBlue='\e[38;05;45m';
    message "${WarningHPurple}WARN${PomptTBlue}" "$*\e[m"
}

function get_user_input_confirm() {
    local confirm_default="${1:-0}"
    shift
    local confirm=""
    local PomptHBlue='\e[38;05;33m';
    local PomptTBlue='\e[38;05;45m';

    prompt "$@"
    echo -en "${PomptHBlue}CHOOSE:  YES / NO   (Y/N) [default: " >&2
    [[ "$confirm_default" == "1" ]] && echo -en 'YES]: \e[m' >&2 || echo -en 'NO]: \e[m' >&2
    read confirm
    if [[ -z "$confirm" ]]
    then
        confirm="$confirm_default"
    else
        ( [[ $confirm == [yY] ]] || [[ $confirm == [yY][eE][sS] ]] ) && confirm="1" || confirm="0"
    fi
    echo "${confirm}"
}

function get_user_input_def_yes() {
    get_user_input_confirm 1 "$@"
}

function get_user_input_def_no() {
    get_user_input_confirm 0 "$@"
}

function get_filename() {
    local path="$1"
    echo ${path##*/}
}

function get_dirname() {
    local path="$1"
    echo "${path%/*}/"
}

function get_extension() {
    local filename
    filename="$(get_filename "${1}")"
    echo "${filename#*.}"
}

function check_extension() {
    local filename="$1"
    local extension="$2"
    if [ "${filename}" == "${filename%"${extension}"}" ]; then
        echo "0"
    else
        echo "1"
    fi
}

function get_basename() {
    local filename
    filename="$(get_filename "${1}")"
    echo "${filename%%.*}"
}

function get_github_elements() {
    local full_url
    local path_elements
    full_url="$(get_dirname "$1")"
    path_elements=($(tr '/' ' ' <<< "${full_url#*://github.com/}"))
    echo "${path_elements[0]} ${path_elements[1]}"
}

function get_github_namespace() {
    cut -d' ' -f1 <<< "$(get_github_elements "$1")"
}

function get_github_repo() {
    cut -d' ' -f2 <<< "$(get_github_elements "$1")"
}

# Takes ID as first param and full path to output file as second for creating benchmark specific output file
#  input: [path]/[file_base].[extension]
# output: [path]/[file_base][id].[extension]
function get_filepath_add_sufix() {
    local dir_path
    local file_base
    local file_ext
    local file_sufix="${1}"
    local file_path="${2}"
    dir_path="$(get_dirname "${file_path}")"
    file_base="$(get_basename "${file_path}")"
    file_ext="$(get_extension "${file_path}")"
    echo "${dir_path}${file_base}${file_sufix}.${file_ext}"
}

function run_as_root_user()
{
    CMD_TO_EVALUATE="$*"
    if [ "${EUID:-$(id -u)}" -eq 0 ]; then
        eval "${CMD_TO_EVALUATE[*]}"
    else
        eval "sudo ${CMD_TO_EVALUATE[*]}" || echo 'Must be run as root. No sudo found.'
    fi
}

function github_api_call() {
    url=$1
    shift
    GITHUB_API_URL=https://api.github.com
    INPUT_OWNER=$(echo "${url#"${GITHUB_API_URL}/repos/"}" | cut -f1 -d'/')
    INPUT_REPO=$(echo "${url#"${GITHUB_API_URL}/repos/"}" | cut -f2 -d'/')
    API_SUBPATH="${url#"${GITHUB_API_URL}/repos/${INPUT_OWNER}/${INPUT_REPO}/"}"
    if [ -z "${INPUT_GITHUB_TOKEN}" ]; then
        echo >&2 "Set the INPUT_GITHUB_TOKEN env variable first."
        return
    fi

    echo >&2 "GITHUB_API_URL=$GITHUB_API_URL"
    echo >&2 "INPUT_OWNER=$INPUT_OWNER"
    echo >&2 "INPUT_REPO=$INPUT_REPO"
    echo >&2 "API_SUBPATH=$API_SUBPATH"
    echo >&2 "curl --fail-with-body -sSL \"${GITHUB_API_URL}/repos/${INPUT_OWNER}/${INPUT_REPO}/${API_SUBPATH}\" -H \"Authorization: Bearer ${INPUT_GITHUB_TOKEN}\" -H 'Accept: application/vnd.github.v3+json' -H 'Content-Type: application/json' $*"

    if API_RESPONSE=$(curl --fail-with-body -sSL \
        "${GITHUB_API_URL}/repos/${INPUT_OWNER}/${INPUT_REPO}/${API_SUBPATH}" \
        -H "Authorization: Bearer ${INPUT_GITHUB_TOKEN}" \
        -H 'Accept: application/vnd.github.v3+json' \
        -H 'Content-Type: application/json' \
        "$@")
    then
        echo "${API_RESPONSE}"
    else
        echo >&2 "GitHub API call failed."
        echo >&2 "${API_RESPONSE}"
        return
    fi
}

function print_logo()
{
    if [[ -z "$blue_code" ]]
    then
        local blue_code=( 26 27 20 19 20 20 21 04 27 26 32 12 33 06 39 38 44 45 )
    fi
    local IFS
	local logo_string
    local colorized_logo_string
    IFS=$'\n\t'
    logo_string="$(cat <<- EOF
	.-----------------------------------------------------------.
	|        *          .                    ..        .    *   |
	|       .                         .   .  .  .   .           |
	|                                    . .  *:. . .           |
	|                             .  .   . .. .         .       |
	|                    .     . .  . ...    .    .             |
	|  .              .  .  . .    . .  . .                     |
	|                   .    .     . ...   ..   .       .       |
	|            .  .    . *.   . .                             |
	|                   :.  .           .                       |
	|            .   .    .    .                                |
	|        .  .  .    . ^                                     |
	|       .  .. :.    . |             .               .       |
	|.   ... .            |                                     |
	| :.  . .   *.    We are here.              .               |
	|   .               .             *.                        |
	.©-Intel-Corporation--------------------ascii-author-unknown.
	=                                                           =
	=        88                                  88             =
	=        ""                ,d                88             =
	=        88                88                88             =
	=        88  8b,dPPYba,  MM88MMM  ,adPPYba,  88             =
	=        88  88P'   '"8a   88    a8P_____88  88             =
	=        88  88       88   88    8PP"""""""  88             =
	=        88  88       88   88,   "8b,   ,aa  88             =
	=        88  88       88   "Y888  '"Ybbd8"'  88             =
	=                                                           =
	=============================================================
		EOF
    )"

    colorized_logo_string=""
    for (( i=0; i<${#logo_string}; i++ ))
    do
        colorized_logo_string+="\e[38;05;${blue_code[$(( (i-(i/64)*64)/4 ))]}m"
        colorized_logo_string+="${logo_string:$i:1}"
    done;
    colorized_logo_string+='\e[m\n'

    echo -e "$colorized_logo_string" >&2
}

function print_logo_sequence()
{
    local wait_between_frames="${1:-0}"
    local wait_cmd=""
    if [ ! "${wait_between_frames}" = "0" ]; then
        wait_cmd="sleep ${wait_between_frames}"
    fi

    blue_code_fixed=( 26 27 20 19 20 20 21 04 27 26 32 12 33 06 39 38 44 45 )
    size=${#blue_code_fixed[@]}
    for (( move=0; move<size; move++ ))
    do
    	blue_code=()
    	for (( i=move; i<size; i++ ))
    	do
    		blue_code+=("${blue_code_fixed[i]}")
    	done
    	for (( i=0; i<move; i++ ))
    	do
    		blue_code+=("${blue_code_fixed[i]}")
    	done
    	echo -en "\e[0;0H"
    	print_logo
        ${wait_cmd}
    done;
}

function print_logo_anim()
{
    local number_of_sequences="${1:-3}"
    local wait_between_frames="${2:-0.025}"
    clear
    for (( pt=0; pt<${number_of_sequences}; pt++ ))
    do
        print_logo_sequence ${wait_between_frames};
    done
}
