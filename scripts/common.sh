#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

function message() {
    local type=$1
    shift
    echo -e "$type: $*" >&2
}

function prompt() {
    local PromptHBlue='\e[38;05;33m';
    local PromptTBlue='\e[38;05;45m';
    message "${PromptHBlue}INFO${PromptTBlue}" "$*\e[m"
}

function error() {
    local ErrorHRed='\e[38;05;1m'
    local PromptTBlue='\e[38;05;45m';
    message "${ErrorHRed}ERROR${PromptTBlue}" "$*\e[m"
}

function warning() {
    local WarningHPurple='\e[38;05;61m';
    local PromptTBlue='\e[38;05;45m';
    message "${WarningHPurple}WARN${PromptTBlue}" "$*\e[m"
}

function get_user_input_confirm() {
    local confirm
    local confirm_string
    local confirm_default="${1:-0}"
    local PromptHBlue='\e[38;05;33m';
    local PromptTBlue='\e[38;05;45m';
    confirm_string=( "(N)o" "(Y)es" )

    echo -en "${PromptHBlue}CHOOSE:${PromptTBlue} (Y)es/(N)o [default: ${confirm_string[$confirm_default]}]: \e[m" >&2
    read -r confirm
    if [[ -z "$confirm" ]]; then
        confirm="$confirm_default"
    else
        { [[ $confirm == [yY] ]] || [[ $confirm == [yY][eE][sS] ]]; } && confirm="1" || confirm="0"
    fi
    echo "${confirm}"
}

function get_user_input_def_yes() {
    get_user_input_confirm 1
}

function get_user_input_def_no() {
    get_user_input_confirm 0
}

function get_filename() {
    local path="$1"
    echo "${path##*/}"
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

# Extracts namespace and repository part from valid GitHub URL passed as argument.
#  input: valid GitHub repository URL
# output: two element string array, space separated
function get_github_elements() {
    local path_part
    local path_elements
    path_part="${1#*://github.com/}"
    mapfile -t -d'/' path_elements <<< "${path_part}"
    if [[ "${#path_elements[@]}" -lt "2" ]]; then
        error "Invalid link passed to get_github_elements method."
        return 1
    fi
    echo "${path_elements[0]} ${path_elements[1]}"
}

function get_github_namespace() {
    cut -d' ' -f1 <<< "$(get_github_elements "$1")"
}

function get_github_repo() {
    cut -d' ' -f2 <<< "$(get_github_elements "$1")"
}

# Adds sufix to base of filename from full path.
# input[1]: string sufix to be added
# input[2]: path string to be modified
#   output: [path]/[file_base][id].[extension]
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
    local number_of_sequences="${1:-2}"
    local wait_between_frames="${2:-0.025}"
    clear
    for (( pt=0; pt<number_of_sequences; pt++ ))
    do
        print_logo_sequence "${wait_between_frames}"
    done
}

function config_intel_rdma_driver() {
    prompt "Configuration of iRDMA starting."
    prompt "Enabling RoCE."

    # enable RoCE
    roce_ena_val=$(grep "options irdma roce_ena=" /etc/modprobe.d/irdma.conf | cut -d "=" -f 2)
    if [[ -z "$roce_ena_val" ]]; then
        echo "options irdma roce_ena=1" | sudo tee -a /etc/modprobe.d/irdma.conf
        sudo dracut -f
    elif [[ "$roce_ena_val" != "1" ]]; then
        sudo sed -i '/options irdma roce_ena=/s/roce_ena=[0-9]*/roce_ena=1/' /etc/modprobe.d/irdma.conf
        sudo dracut -f
    fi
    prompt "RoCE enabled."

    prompt "Increasing Queue Pair limit."
    # increase irdma Queue Pair limit
    limits_sel_val=$(grep "options irdma limits_sel=" /etc/modprobe.d/irdma.conf | cut -d "=" -f 2)
    if [[ -z "$limits_sel_val" ]]; then
        echo "options irdma limits_sel=5" | sudo tee -a /etc/modprobe.d/irdma.conf
        sudo dracut -f
    elif [[ "$limits_sel_val" != "5" ]]; then
        sudo sed -i '/options irdma limits_sel=/s/limits_sel=[0-9]*/limits_sel=5/' /etc/modprobe.d/irdma.conf
        sudo dracut -f
    fi
    prompt "Queue Pair limits_sel set to 5."
    prompt "Configuration of iRDMA finished."
}

function exec_command()
{
    # One of: yes|no|accept-new
    SSH_STRICT_HOST_KEY_CHECKING="accept-new"
    SSH_CMD="ssh -oStrictHostKeyChecking=${SSH_STRICT_HOST_KEY_CHECKING} -t -o"

    local values_returned=""
    local user_at_address=""
    [[ "$#" -eq "2" ]] && user_at_address="${2}"
    [[ "$#" -eq "3" ]] && user_at_address="${3}@${2}"

    if [ "$#" -eq "1" ]; then
        values_returned="$(eval "${1}")"
    elif [[ "$#" -eq "2" ]] || [[ "$#" -eq "3" ]]; then
        values_returned="$($SSH_CMD "RemoteCommand=eval \"${1}\"" "${user_at_address}" 2>/dev/null)"
    else
        error "Wrong arguments for exec_command(). Valid number is one of [1 2 3], got $#"
        return 1
    fi

    if [ -z "$values_returned" ]; then
        error "Unable to collect results or results are empty."
        return 1
    else
        echo "${values_returned}"
        return 0
    fi
}

function get_hostname() {
    exec_command 'hostname' "$@"
}

function get_intel_nic_device() {
    exec_command "lspci | grep 'Intel Corporation.*\(810\|X722\)'" "$@"
}

function get_default_route_nic() {
    exec_command "ip -json r show default | jq '.[0].dev' -r" "$@"
}

function get_cpu_arch() {
    local arch=""

    if ! arch="$(exec_command 'cat /sys/devices/cpu/caps/pmu_name' "$@")"; then echo "Got: $arch" && return 1; fi

    case $arch in
        icelake)
            prompt "Xeon IceLake CPU (icx)" 1>&2
            echo "icx"
            ;;
        sapphire_rapids)
            prompt "Xeon Sapphire Rapids CPU (spr)" 1>&2
            echo "spr"
            ;;
        skylake)
            prompt "Xeon SkyLake"
            echo "skl"
            ;;
        *)
            error "Unsupported architecture: ${arch}. Please edit the script or setup the architecture manually."
            return 1
            ;;
    esac
    return 0
}
