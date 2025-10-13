#!/bin/bash

# Hydra Microkernel Log Management Utility
# Provides log rotation, cleanup, and analysis tools

LOG_DIR="logs"
MAX_LOG_SIZE="1048576"  # 1MB
MAX_LOG_FILES="10"
BACKUP_DIR="logs/archive"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[LOG-MGR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[LOG-MGR]${NC} WARNING: $1"
}

print_error() {
    echo -e "${RED}[LOG-MGR]${NC} ERROR: $1"
}

print_info() {
    echo -e "${BLUE}[LOG-MGR]${NC} $1"
}

# Create necessary directories
setup_log_directories() {
    print_status "Setting up log directories..."
    mkdir -p "$LOG_DIR"
    mkdir -p "$BACKUP_DIR"
    print_status "Log directories ready: $LOG_DIR, $BACKUP_DIR"
}

# Show current log file sizes and status
show_log_status() {
    print_status "Current log status:"
    if [ ! -d "$LOG_DIR" ]; then
        print_warning "Log directory $LOG_DIR does not exist"
        return
    fi
    
    total_size=0
    file_count=0
    
    for log_file in "$LOG_DIR"/*.log; do
        if [ -f "$log_file" ]; then
            size=$(stat -c%s "$log_file" 2>/dev/null || echo "0")
            size_mb=$((size / 1024 / 1024))
            file_count=$((file_count + 1))
            total_size=$((total_size + size))
            
            if [ $size -gt $MAX_LOG_SIZE ]; then
                print_warning "$(basename "$log_file"): ${size_mb}MB (NEEDS ROTATION)"
            else
                print_info "$(basename "$log_file"): ${size_mb}MB"
            fi
        fi
    done
    
    if [ $file_count -eq 0 ]; then
        print_info "No log files found"
    else
        total_mb=$((total_size / 1024 / 1024))
        print_status "Total: $file_count files, ${total_mb}MB"
    fi
}

# Rotate a specific log file
rotate_log_file() {
    local log_file="$1"
    local base_name=$(basename "$log_file" .log)
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_file="$BACKUP_DIR/${base_name}_${timestamp}.log"
    
    if [ ! -f "$log_file" ]; then
        print_warning "Log file $log_file does not exist"
        return 1
    fi
    
    print_status "Rotating $log_file to $backup_file"
    
    # Move current log to backup
    mv "$log_file" "$backup_file"
    
    # Create new empty log file
    touch "$log_file"
    
    # Compress old log to save space
    if command -v gzip >/dev/null 2>&1; then
        gzip "$backup_file"
        print_status "Compressed backup: ${backup_file}.gz"
    fi
    
    return 0
}

# Rotate all logs that exceed size limit
rotate_large_logs() {
    print_status "Checking for logs that need rotation..."
    
    rotated=0
    for log_file in "$LOG_DIR"/*.log; do
        if [ -f "$log_file" ]; then
            size=$(stat -c%s "$log_file" 2>/dev/null || echo "0")
            if [ $size -gt $MAX_LOG_SIZE ]; then
                rotate_log_file "$log_file"
                rotated=$((rotated + 1))
            fi
        fi
    done
    
    if [ $rotated -eq 0 ]; then
        print_status "No logs need rotation"
    else
        print_status "Rotated $rotated log files"
    fi
}

# Clean old backup files
cleanup_old_backups() {
    print_status "Cleaning up old backups..."
    
    if [ ! -d "$BACKUP_DIR" ]; then
        print_info "No backup directory found"
        return
    fi
    
    # Remove backups older than 7 days
    find "$BACKUP_DIR" -name "*.log*" -type f -mtime +7 -delete 2>/dev/null
    
    # Keep only the most recent backups if there are too many
    for log_type in kernel network ipc migration; do
        backup_count=$(ls -1 "$BACKUP_DIR"/${log_type}_*.log* 2>/dev/null | wc -l)
        if [ $backup_count -gt $MAX_LOG_FILES ]; then
            excess=$((backup_count - MAX_LOG_FILES))
            ls -1t "$BACKUP_DIR"/${log_type}_*.log* 2>/dev/null | tail -n $excess | xargs rm -f
            print_status "Removed $excess old $log_type backups"
        fi
    done
}

# Analyze log content for patterns
analyze_logs() {
    print_status "Analyzing log patterns..."
    
    for log_file in "$LOG_DIR"/*.log; do
        if [ -f "$log_file" ]; then
            log_name=$(basename "$log_file")
            print_info "=== Analysis for $log_name ==="
            
            # Count log levels
            error_count=$(grep -c "ERROR" "$log_file" 2>/dev/null || echo "0")
            warn_count=$(grep -c "WARN" "$log_file" 2>/dev/null || echo "0")
            info_count=$(grep -c "INFO" "$log_file" 2>/dev/null || echo "0")
            
            echo "  Errors: $error_count, Warnings: $warn_count, Info: $info_count"
            
            # Show recent errors if any
            if [ $error_count -gt 0 ]; then
                echo "  Recent errors:"
                grep "ERROR" "$log_file" | tail -n 3 | sed 's/^/    /'
            fi
            
            echo
        fi
    done
}

# Show real-time log monitoring
monitor_logs() {
    print_status "Starting real-time log monitoring (Press Ctrl+C to stop)..."
    
    # Check if any log files exist
    log_files=("$LOG_DIR"/*.log)
    if [ ! -f "${log_files[0]}" ]; then
        print_warning "No log files found to monitor"
        return
    fi
    
    # Use tail to follow all log files
    tail -f "$LOG_DIR"/*.log
}

# Export logs for analysis
export_logs() {
    local export_dir="logs_export_$(date +%Y%m%d_%H%M%S)"
    
    print_status "Exporting logs to $export_dir..."
    mkdir -p "$export_dir"
    
    # Copy all current logs
    cp -r "$LOG_DIR"/* "$export_dir/" 2>/dev/null
    
    # Create summary report
    cat > "$export_dir/summary.txt" << EOF
Hydra Microkernel Log Export
Generated: $(date)

Log File Summary:
EOF
    
    for log_file in "$LOG_DIR"/*.log; do
        if [ -f "$log_file" ]; then
            log_name=$(basename "$log_file")
            size=$(stat -c%s "$log_file" 2>/dev/null || echo "0")
            lines=$(wc -l < "$log_file" 2>/dev/null || echo "0")
            echo "$log_name: $lines lines, $size bytes" >> "$export_dir/summary.txt"
        fi
    done
    
    # Create tarball
    tar -czf "${export_dir}.tar.gz" "$export_dir"
    rm -rf "$export_dir"
    
    print_status "Logs exported to ${export_dir}.tar.gz"
}

# Show help
show_help() {
    echo "Hydra Microkernel Log Management Utility"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  setup     - Create log directories"
    echo "  status    - Show current log file status"
    echo "  rotate    - Rotate logs that exceed size limit"
    echo "  cleanup   - Remove old backup files"
    echo "  analyze   - Analyze log content for patterns"
    echo "  monitor   - Real-time log monitoring"
    echo "  export    - Export logs for external analysis"
    echo "  help      - Show this help message"
    echo ""
    echo "Configuration:"
    echo "  Max log size: $((MAX_LOG_SIZE / 1024 / 1024))MB"
    echo "  Max backup files: $MAX_LOG_FILES"
    echo "  Log directory: $LOG_DIR"
    echo "  Backup directory: $BACKUP_DIR"
}

# Main command processing
case "${1:-help}" in
    setup)
        setup_log_directories
        ;;
    status)
        show_log_status
        ;;
    rotate)
        rotate_large_logs
        ;;
    cleanup)
        cleanup_old_backups
        ;;
    analyze)
        analyze_logs
        ;;
    monitor)
        monitor_logs
        ;;
    export)
        export_logs
        ;;
    help|*)
        show_help
        ;;
esac