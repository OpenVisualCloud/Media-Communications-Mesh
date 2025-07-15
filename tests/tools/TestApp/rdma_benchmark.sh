#!/bin/bash

# RDMA Performance Benchmark Script
# Tests various RDMA configurations and generates comprehensive reports

set -e

# Default configuration
RDMA_PROVIDERS=("tcp" "verbs")
ENDPOINT_COUNTS=(1 2 4 8)
PACKET_SIZES=(1024 4096 8192 16384 65536)
BURST_SIZES=(1 10 50 100)
TEST_DURATION=30
OUTPUT_DIR="rdma_benchmark_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

RDMA Performance Benchmark Script

Options:
    -p, --providers <list>     Comma-separated list of RDMA providers (default: tcp,verbs)
    -e, --endpoints <list>     Comma-separated list of endpoint counts (default: 1,2,4,8)
    -s, --packet-sizes <list>  Comma-separated list of packet sizes (default: 1024,4096,8192,16384,65536)
    -b, --burst-sizes <list>   Comma-separated list of burst sizes (default: 1,10,50,100)
    -t, --duration <seconds>   Test duration per configuration (default: 30)
    -o, --output <dir>         Output directory (default: rdma_benchmark_results)
    -q, --quick               Quick test mode (reduced parameter sets)
    -v, --verbose             Verbose output
    -h, --help                Show this help

Examples:
    # Full benchmark suite
    $0 -t 60 -o results_$(date +%Y%m%d)

    # Quick test with TCP only
    $0 --quick --providers tcp

    # Test specific configuration
    $0 --providers verbs --endpoints 8 --packet-sizes 8192 --burst-sizes 100

    # Latency-focused test
    $0 --packet-sizes 64,256,1024 --burst-sizes 1 --endpoints 1,2

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--providers)
                IFS=',' read -ra RDMA_PROVIDERS <<< "$2"
                shift 2
                ;;
            -e|--endpoints)
                IFS=',' read -ra ENDPOINT_COUNTS <<< "$2"
                shift 2
                ;;
            -s|--packet-sizes)
                IFS=',' read -ra PACKET_SIZES <<< "$2"
                shift 2
                ;;
            -b|--burst-sizes)
                IFS=',' read -ra BURST_SIZES <<< "$2"
                shift 2
                ;;
            -t|--duration)
                TEST_DURATION="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -q|--quick)
                RDMA_PROVIDERS=("tcp")
                ENDPOINT_COUNTS=(1 2)
                PACKET_SIZES=(1024 8192)
                BURST_SIZES=(1 10)
                TEST_DURATION=10
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                echo "Unknown option $1"
                usage
                exit 1
                ;;
        esac
    done
}

# Logging functions
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"
}

verbose_log() {
    if [[ "${VERBOSE:-0}" == "1" ]]; then
        log "$*"
    fi
}

# Check if applications exist
check_applications() {
    local missing=0
    
    if [[ ! -f "./TxRDMATestApp" ]]; then
        log "ERROR: TxRDMATestApp not found. Please build the application first."
        missing=1
    fi
    
    if [[ ! -f "./RxRDMATestApp" ]]; then
        log "ERROR: RxRDMATestApp not found. Please build the application first."
        missing=1
    fi
    
    if [[ $missing == 1 ]]; then
        log "To build applications, run: make -C .. TxRDMATestApp RxRDMATestApp"
        exit 1
    fi
}

# Initialize output directory
init_output_dir() {
    local full_output_dir="${OUTPUT_DIR}_${TIMESTAMP}"
    mkdir -p "$full_output_dir"
    OUTPUT_DIR="$full_output_dir"
    
    log "Results will be saved to: $OUTPUT_DIR"
    
    # Create summary file
    cat > "$OUTPUT_DIR/benchmark_config.txt" << EOF
RDMA Benchmark Configuration
============================
Timestamp: $TIMESTAMP
Providers: ${RDMA_PROVIDERS[*]}
Endpoint Counts: ${ENDPOINT_COUNTS[*]}
Packet Sizes: ${PACKET_SIZES[*]} bytes
Burst Sizes: ${BURST_SIZES[*]}
Test Duration: $TEST_DURATION seconds
Output Directory: $OUTPUT_DIR

System Information:
$(uname -a)
$(lscpu | grep "Model name" || echo "CPU info not available")
$(free -h | head -2 || echo "Memory info not available")
EOF
}

# Run a single test configuration
run_test() {
    local provider="$1"
    local endpoints="$2"
    local packet_size="$3"
    local burst_size="$4"
    local test_name="${provider}_ep${endpoints}_ps${packet_size}_bs${burst_size}"
    
    verbose_log "Running test: $test_name"
    
    local tx_output="$OUTPUT_DIR/tx_${test_name}.log"
    local rx_output="$OUTPUT_DIR/rx_${test_name}.log"
    local tx_results="$OUTPUT_DIR/tx_${test_name}_results.txt"
    local rx_results="$OUTPUT_DIR/rx_${test_name}_results.txt"
    
    # Start receiver in background
    verbose_log "Starting receiver for $test_name"
    ./RxRDMATestApp \
        --rdma-provider "$provider" \
        --rdma-endpoints "$endpoints" \
        --test-duration "$TEST_DURATION" \
        --enable-latency \
        --enable-throughput \
        --enable-loss \
        --output "$rx_results" \
        > "$rx_output" 2>&1 &
    
    local rx_pid=$!
    
    # Wait a moment for receiver to start
    sleep 2
    
    # Start transmitter
    verbose_log "Starting transmitter for $test_name"
    ./TxRDMATestApp \
        --rdma-provider "$provider" \
        --rdma-endpoints "$endpoints" \
        --packet-size "$packet_size" \
        --burst-size "$burst_size" \
        --test-duration "$TEST_DURATION" \
        --enable-latency \
        --enable-throughput \
        --output "$tx_results" \
        > "$tx_output" 2>&1
    
    local tx_exit_code=$?
    
    # Wait for receiver to finish
    wait $rx_pid
    local rx_exit_code=$?
    
    # Check if test completed successfully
    if [[ $tx_exit_code -eq 0 && $rx_exit_code -eq 0 ]]; then
        verbose_log "Test $test_name completed successfully"
        return 0
    else
        log "WARNING: Test $test_name failed (TX: $tx_exit_code, RX: $rx_exit_code)"
        return 1
    fi
}

# Extract key metrics from result files
extract_metrics() {
    local test_name="$1"
    local tx_results="$OUTPUT_DIR/tx_${test_name}_results.txt"
    local rx_results="$OUTPUT_DIR/rx_${test_name}_results.txt"
    
    if [[ -f "$tx_results" && -f "$rx_results" ]]; then
        local tx_throughput=$(grep "Average Throughput" "$tx_results" | awk '{print $3}' || echo "N/A")
        local rx_throughput=$(grep "Average Throughput" "$rx_results" | awk '{print $3}' || echo "N/A")
        local tx_packets=$(grep "Packets Sent" "$tx_results" | awk '{print $3}' || echo "N/A")
        local rx_packets=$(grep "Packets Received" "$rx_results" | awk '{print $3}' || echo "N/A")
        local avg_latency=$(grep "Average Latency" "$rx_results" | awk '{print $3}' || echo "N/A")
        local packet_loss=$(grep "Packet Loss Rate" "$rx_results" | awk '{print $4}' || echo "N/A")
        
        echo "$test_name,$tx_throughput,$rx_throughput,$tx_packets,$rx_packets,$avg_latency,$packet_loss"
    else
        echo "$test_name,FAILED,FAILED,FAILED,FAILED,FAILED,FAILED"
    fi
}

# Generate CSV summary
generate_csv_summary() {
    local csv_file="$OUTPUT_DIR/benchmark_summary.csv"
    
    log "Generating CSV summary: $csv_file"
    
    echo "Test_Name,TX_Throughput_Mbps,RX_Throughput_Mbps,TX_Packets,RX_Packets,Avg_Latency_us,Packet_Loss_Rate" > "$csv_file"
    
    for provider in "${RDMA_PROVIDERS[@]}"; do
        for endpoints in "${ENDPOINT_COUNTS[@]}"; do
            for packet_size in "${PACKET_SIZES[@]}"; do
                for burst_size in "${BURST_SIZES[@]}"; do
                    local test_name="${provider}_ep${endpoints}_ps${packet_size}_bs${burst_size}"
                    extract_metrics "$test_name" >> "$csv_file"
                done
            done
        done
    done
}

# Generate performance report
generate_report() {
    local report_file="$OUTPUT_DIR/performance_report.txt"
    
    log "Generating performance report: $report_file"
    
    cat > "$report_file" << EOF
RDMA Performance Benchmark Report
=================================
Generated: $(date)
Configuration: See benchmark_config.txt

Test Results Summary:
EOF
    
    # Add best performing configurations
    if [[ -f "$OUTPUT_DIR/benchmark_summary.csv" ]]; then
        echo "" >> "$report_file"
        echo "Top 5 Configurations by Throughput:" >> "$report_file"
        echo "====================================" >> "$report_file"
        tail -n +2 "$OUTPUT_DIR/benchmark_summary.csv" | \
            sort -t, -k2 -nr | \
            head -5 | \
            awk -F, '{printf "%-30s TX: %8s Mbps, RX: %8s Mbps, Latency: %8s μs\n", $1, $2, $3, $6}' >> "$report_file"
        
        echo "" >> "$report_file"
        echo "Top 5 Configurations by Latency:" >> "$report_file"
        echo "=================================" >> "$report_file"
        tail -n +2 "$OUTPUT_DIR/benchmark_summary.csv" | \
            grep -v "N/A" | \
            sort -t, -k6 -n | \
            head -5 | \
            awk -F, '{printf "%-30s Latency: %8s μs, TX: %8s Mbps, Loss: %8s\n", $1, $6, $2, $7}' >> "$report_file"
        
        echo "" >> "$report_file"
        echo "Configurations with Packet Loss:" >> "$report_file"
        echo "================================" >> "$report_file"
        tail -n +2 "$OUTPUT_DIR/benchmark_summary.csv" | \
            awk -F, '$7 != "N/A" && $7 != "0.00%" {printf "%-30s Loss: %8s, Throughput: %8s Mbps\n", $1, $7, $2}' >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "Detailed Results:" >> "$report_file"
    echo "=================" >> "$report_file"
    echo "See individual result files in $OUTPUT_DIR/" >> "$report_file"
}

# Cleanup function
cleanup() {
    log "Cleaning up background processes..."
    pkill -f "RxRDMATestApp" 2>/dev/null || true
    pkill -f "TxRDMATestApp" 2>/dev/null || true
}

# Main execution
main() {
    parse_args "$@"
    
    log "Starting RDMA Performance Benchmark"
    log "Configuration: ${#RDMA_PROVIDERS[@]} providers, ${#ENDPOINT_COUNTS[@]} endpoint configs, ${#PACKET_SIZES[@]} packet sizes, ${#BURST_SIZES[@]} burst sizes"
    
    local total_tests=$((${#RDMA_PROVIDERS[@]} * ${#ENDPOINT_COUNTS[@]} * ${#PACKET_SIZES[@]} * ${#BURST_SIZES[@]}))
    log "Total tests to run: $total_tests (ETA: $((total_tests * TEST_DURATION / 60)) minutes)"
    
    # Setup
    check_applications
    init_output_dir
    
    # Set up cleanup on exit
    trap cleanup EXIT
    
    # Run all test combinations
    local test_count=0
    local passed_tests=0
    
    for provider in "${RDMA_PROVIDERS[@]}"; do
        for endpoints in "${ENDPOINT_COUNTS[@]}"; do
            for packet_size in "${PACKET_SIZES[@]}"; do
                for burst_size in "${BURST_SIZES[@]}"; do
                    test_count=$((test_count + 1))
                    local test_name="${provider}_ep${endpoints}_ps${packet_size}_bs${burst_size}"
                    
                    log "Running test $test_count/$total_tests: $test_name"
                    
                    if run_test "$provider" "$endpoints" "$packet_size" "$burst_size"; then
                        passed_tests=$((passed_tests + 1))
                    fi
                    
                    # Brief pause between tests
                    sleep 1
                done
            done
        done
    done
    
    log "Benchmark completed: $passed_tests/$total_tests tests passed"
    
    # Generate reports
    generate_csv_summary
    generate_report
    
    log "Results saved to: $OUTPUT_DIR"
    log "View summary: cat $OUTPUT_DIR/benchmark_summary.csv"
    log "View report: cat $OUTPUT_DIR/performance_report.txt"
}

# Execute main function with all arguments
main "$@"
